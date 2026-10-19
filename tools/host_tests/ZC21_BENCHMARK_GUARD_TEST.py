#!/usr/bin/env python3
"""Run the actual embedded guard and snapshot shell flow on synthetic OSData.

No IORegistry, driver, network, sleep, iperf3 or hardware access. The campaign
harness substitutes local fixtures for ioreg and a counter for run_test.
"""
from pathlib import Path
import json
import plistlib
import re
import shlex
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / 'tools/ZC21_3x14_PORTABLE_IP_PROMPT.sh'
BUILT_HEAD = 'f18801f700b433199cf45808eaa674c92ec4f422'
SOURCE = RUNNER.read_text()
OLD = subprocess.check_output(
    ['git', '-C', str(ROOT), 'show', BUILT_HEAD + ':tools/ZC19_3x14_PORTABLE_IP_PROMPT.sh'],
    text=True)
MARKER = 'AirportItlwm-Tahoe-B1-ZC19-PERF'


def guard_function(source):
    return source.split('check_snapshot() {\n', 1)[1].split('\nPY\n}', 1)[0]


def python_guard(source):
    return guard_function(source).split("<<'PY'\n", 1)[1]


def record(publication=100, mode=2):
    w = [0] * 32
    w[:8] = [19, 1, 256, publication, publication * 1_000_000_000, mode, 1, 2]
    w[11:13] = [1, 518]
    return w


def fixture(words, marker=MARKER):
    return [{'KGP_ZC19': struct.pack('<32Q', *words), 'KGP_RX_EXPERIMENT': marker}]


class BenchmarkGuard(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='zc19-benchmark-')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.guard = self.path / 'guard.py'
        self.guard.write_text(python_guard(SOURCE))
        self.sequence = 0

    def run_guard(self, words=None, initial='NONE', previous='NONE', phase='boundary',
                  mode='mapped', marker=MARKER, properties=None):
        self.sequence += 1
        src = self.path / f'snapshot-{self.sequence}.plist'
        src.write_bytes(plistlib.dumps(properties if properties is not None else
                                      fixture(words if words is not None else record(), marker)))
        result = subprocess.run([sys.executable, '-B', str(self.guard), str(src),
                                 str(initial), str(previous), mode, 'ZC19', phase],
                                capture_output=True, text=True)
        return result, src.with_suffix('.health.json')

    def accepted(self, *args, **kwargs):
        result, health = self.run_guard(*args, **kwargs)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(health.is_file())
        return health

    def rejected(self, *args, message, **kwargs):
        result, health = self.run_guard(*args, **kwargs)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(message, result.stderr)
        self.assertFalse(health.exists(), 'Rejected health must not become the next reference')

    def test_original_false_failure_reproduced(self):
        old_guard = self.path / 'old_guard.py'
        old_guard.write_text(python_guard(OLD))
        src = self.path / 'old.plist'
        src.write_bytes(plistlib.dumps(fixture(record())))
        args = [sys.executable, '-B', str(old_guard), str(src)]
        first = subprocess.run(args + ['NONE', 'mapped', 'ZC19'], capture_output=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        second = subprocess.run(args + [str(src.with_suffix('.health.json')), 'mapped', 'ZC19'],
                                capture_output=True, text=True)
        self.assertNotEqual(second.returncode, 0)
        self.assertIn('STOP: stale ZC19 health snapshot', second.stderr)

    def test_adjacent_repeats_and_final_equal_previous_in_both_modes(self):
        for mode, numeric in [('mapped', 2), ('conventional', 1)]:
            with self.subTest(mode=mode):
                initial = previous = self.accepted(record(100, numeric), mode=mode)
                for counter in [100, 201, 201, 302, 302, 403]:
                    previous = self.accepted(record(counter, numeric), initial, previous, mode=mode)
                final = self.accepted(record(403, numeric), initial, previous, 'final', mode=mode)
                self.assertEqual(json.loads(final.read_text())['Publication'], 403)

    def test_counter_rollback_even_when_above_initial(self):
        for mode, numeric in [('mapped', 2), ('conventional', 1)]:
            initial = self.accepted(record(100, numeric), mode=mode)
            previous = self.accepted(record(200, numeric), initial, initial, mode=mode)
            for counter in [199, 100, 99]:
                with self.subTest(mode=mode, counter=counter):
                    self.rejected(record(counter, numeric), initial, previous, mode=mode,
                                  message='publication counter rolled back')

    def test_unchanged_full_campaign_fails_at_final(self):
        for mode, numeric in [('mapped', 2), ('conventional', 1)]:
            initial = previous = self.accepted(record(100, numeric), mode=mode)
            for _ in range(6):
                previous = self.accepted(record(100, numeric), initial, previous, mode=mode)
            self.rejected(record(100, numeric), initial, previous, 'final', mode=mode,
                          message='publication did not advance during campaign')

    def test_final_requires_campaign_references(self):
        initial = self.accepted()
        for ref, previous in [('NONE', 'NONE'), (initial, 'NONE'), ('NONE', initial)]:
            self.rejected(record(101), ref, previous, 'final', message='missing ZC19 campaign')
        self.rejected(record(101), initial, initial, 'typo', message='unknown ZC19 snapshot phase')

    def test_equal_counter_never_bypasses_faults_or_epochs(self):
        for mode, numeric in [('mapped', 2), ('conventional', 1)]:
            initial = self.accepted(record(100, numeric), mode=mode)
            for field in [8, 9, 10, 13, 14]:
                with self.subTest(mode=mode, fault=field):
                    w = record(100, numeric)
                    w[field] = 1
                    self.rejected(w, initial, initial, mode=mode, message='fault state')
            for field in [6, 11, 12]:
                with self.subTest(mode=mode, epoch=field):
                    w = record(100, numeric)
                    w[field] += 1
                    self.rejected(w, initial, initial, mode=mode, message='epoch changed')

    def test_actual_mode_and_marker_remain_strict(self):
        for mode, numeric in [('mapped', 2), ('conventional', 1)]:
            initial = self.accepted(record(100, numeric), mode=mode)
            for wrong in [0, 3 - numeric]:
                self.rejected(record(100, wrong), initial, initial, mode=mode,
                              message='wrong ZC19 actual DMA mode')
            self.rejected(record(100, numeric), initial, initial, mode=mode,
                          marker='AirportItlwm-Tahoe-B1-ZC18-PERF', message='wrong ZC19 marker')

    def test_malformed_unpublished_and_ambiguous_records_rejected(self):
        for field, value in [(0, 18), (1, 2), (2, 255), (3, 0), (4, 0)]:
            w = record()
            w[field] = value
            self.rejected(w, message='invalid/unpublished ZC19 schema')
        for props in [[], fixture(record()) * 2,
                      [{'KGP_ZC19': bytes(255), 'KGP_RX_EXPERIMENT': MARKER}],
                      fixture(record()) + [{'KGP_ZC10': bytes(10912)}]]:
            self.rejected(properties=props, message='expected exactly one ZC19')

    def campaign(self, counters, mode):
        folder = self.path / f'campaign-{self.sequence}'
        self.sequence += 1
        folder.mkdir()
        fixtures = folder / 'fixtures'
        fixtures.mkdir()
        output = folder / 'out'
        output.mkdir()
        for i, counter in enumerate(counters):
            (fixtures / f'{i}.plist').write_bytes(plistlib.dumps(
                fixture(record(counter, 2 if mode == 'mapped' else 1))))
        # Execute the real snapshot functions AND real initial/round/final
        # control flow. Stub only IO/network traffic; no actual benchmark runs.
        functions = SOURCE.split('check_snapshot() {\n', 1)[1].split('\ncheck_network() {', 1)[0]
        initial_rounds = SOURCE.split('stage="INITIAL_SNAPSHOT"\n', 1)[1].split(
            '# Mandatory FINAL ONLINE', 1)[0]
        final = SOURCE.split('stage="FINAL_SNAPSHOT"\n', 1)[1]
        script = f'''set -Eeuo pipefail
OUT={shlex.quote(str(output))}
FIXTURES={shlex.quote(str(fixtures))}
MODE={mode}
LABEL=ZC19
DEVICE=AX210
IOREG_SERVICE=ARPT
INITIAL_HEALTH=""
PREVIOUS_HEALTH=""
completed=0
cursor=0
finished_ok=0
ioreg() {{ cat "$FIXTURES/$cursor.plist"; cursor=$((cursor+1)); }}
check_network() {{ :; }}
run_test() {{ completed=$((completed+1)); }}
check_snapshot() {{
{functions}
stage="INITIAL_SNAPSHOT"
{initial_rounds}
stage="FINAL_SNAPSHOT"
{final}
[[ "$completed" == 42 && "$cursor" == 8 && "$finished_ok" == 1 ]]
[[ "$PREVIOUS_HEALTH" == "$OUT/FINAL.health.json" ]]
echo MOCK_CAMPAIGN_COMPLETE
'''
        return subprocess.run(['/bin/bash', '-c', script], capture_output=True, text=True)

    def test_actual_shell_campaign_accepts_adjacent_repeats(self):
        for mode in ['mapped', 'conventional']:
            result = self.campaign([100, 100, 200, 200, 300, 300, 400, 400], mode)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('MOCK_CAMPAIGN_COMPLETE', result.stdout)

    def test_actual_shell_campaign_never_marks_stale_or_rollback_complete(self):
        for mode in ['mapped', 'conventional']:
            for counters, message in [([100] * 8, 'did not advance during campaign'),
                                      ([100, 100, 200, 199, 300, 300, 400, 400], 'rolled back'),
                                      ([100, 100, 200, 200, 300, 300, 400, 399], 'rolled back')]:
                result = self.campaign(counters, mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn(message, result.stderr)
                self.assertNotIn('BENCHMARK FINISHED', result.stdout)
                self.assertNotIn('MOCK_CAMPAIGN_COMPLETE', result.stdout)

    def test_failed_guard_does_not_replace_previous_reference(self):
        initial = self.accepted(record(100))
        fixture_path = self.path / 'bad.plist'
        fixture_path.write_bytes(plistlib.dumps(fixture(record(99))))
        functions = SOURCE.split('check_snapshot() {\n', 1)[1].split('\ncheck_network() {', 1)[0]
        script = f'''set -Eeuo pipefail
MODE=mapped
LABEL=ZC19
IOREG_SERVICE=ARPT
INITIAL_HEALTH={shlex.quote(str(initial))}
PREVIOUS_HEALTH="$INITIAL_HEALTH"
ioreg() {{ cat {shlex.quote(str(fixture_path))}; }}
check_snapshot() {{
{functions}
if capture {shlex.quote(str(self.path / 'rejected.plist'))}; then exit 99; fi
[[ "$PREVIOUS_HEALTH" == "$INITIAL_HEALTH" ]]
'''
        result = subprocess.run(['/bin/bash', '-c', script], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('rolled back', result.stderr)

    def test_loaded_uuid_guard_unchanged_and_rejects_mismatch(self):
        pattern = r'LOADED="\$\(grep.*?\nfi\n'
        block = re.search(pattern, SOURCE, re.S).group()
        self.assertEqual(block, re.search(pattern, OLD, re.S).group())
        uuid = '1C2D5EBC-764D-3EF2-9714-11ADF9A2016A'
        for loaded, success in [(f'com.zxystd.AirportItlwm {uuid}', True),
                                ('com.zxystd.AirportItlwm WRONG-UUID', False), ('', False)]:
            (self.path / 'PREFLIGHT.txt').write_text(loaded + '\n')
            result = subprocess.run(['/bin/bash', '-c', 'set -Eeuo pipefail\nOUT=' +
                                     shlex.quote(str(self.path)) + '\nUUID=' + uuid + '\n' + block],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)

    def test_legacy_guards_and_benchmark_protocol_unchanged(self):
        self.assertEqual(python_guard(SOURCE).split("if label=='ZC8':", 1)[1],
                         python_guard(OLD).split("if label=='ZC8':", 1)[1])
        # Includes all 42-case arguments, gaps, online test and snapshot calls.
        for src in [SOURCE, OLD]:
            self.assertNotRegex(src, r'(?m)^\s*(?:/usr/sbin/)?system_profiler\s')
        tail = SOURCE.split('stage="INITIAL_SNAPSHOT"\n', 1)[1]
        self.assertEqual(tail.replace('capture "$OUT/FINAL.plist" final',
                                      'capture "$OUT/FINAL.plist"'),
                         OLD.split('stage="INITIAL_SNAPSHOT"\n', 1)[1])
        # No new sampling, sleep, network checks or collection commands.
        for prefix in ['ioreg ', 'sleep ', 'run_test ', 'networkQuality ']:
            self.assertEqual([x.strip() for x in SOURCE.splitlines() if x.strip().startswith(prefix)],
                             [x.strip() for x in OLD.splitlines() if x.strip().startswith(prefix)])
        subprocess.run(['/bin/bash', '-n', str(RUNNER)], check=True)


if __name__ == '__main__':
    unittest.main(verbosity=2)
