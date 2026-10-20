#!/usr/bin/env python3
"""Saved-plist decoder regression tests; no live IORegistry or traffic calls."""
import importlib.util
import json
import os
import plistlib
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
default = HERE / 'decode_zc28_service.py'
if not default.exists():
    default = HERE.parent / 'decode_zc28_service.py'
DECODER = Path(os.environ.get('ZC28_DECODER', default))
spec = importlib.util.spec_from_file_location('decoder', DECODER)
d = importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)


def words(mode=2):
    w = [0] * 32
    w[:12] = [28, 1, 256, 100, 123456789, mode, 1, 1, 2, 1, 1, 0]
    w[16:24] = [4, 4, 3, 2, 1, 0, 7, 0]
    return w


def health(w):
    h = [0] * 32
    h[:8] = [19, 1, 256, w[3], w[4], w[5], w[6], 2]
    h[11], h[12] = w[7], w[8]
    return h


def provider(w=None, h='same', marker=d.MARKER):
    w = words() if w is None else w
    p = {'IORegistryEntryName': 'ARPT', 'KGP_RX_EXPERIMENT': marker,
         d.KEY: struct.pack('<32Q', *w)}
    if h == 'same':
        h = health(w)
    if h is not None:
        p[d.HEALTH_KEY] = struct.pack('<32Q', *h)
    return p


def decode(p):
    return d.decode_plist(plistlib.dumps([p]))


class Tests(unittest.TestCase):
    def test_01_current_both_modes(self):
        for mode in (1, 2):
            r = decode(provider(words(mode)))
            self.assertTrue(r['diagnostic_lifetime_trusted'])
            self.assertEqual(r['actual_dma_mode'], mode)
            self.assertEqual(r['queue']['service_begins'], 4)
            self.assertTrue(r['health_sampled_fault_fields_all_zero'])

    def test_02_exact_lengths(self):
        for n in (0, 1, 255, 257, 512):
            p = provider();p[d.KEY] = bytes(n)
            with self.assertRaises(d.DecodeError):decode(p)

    def test_03_exact_schema(self):
        for index, value in ((0, 27), (1, 2), (2, 255)):
            w = words();w[index] = value
            with self.assertRaises(d.DecodeError):decode(provider(w))

    def test_04_exact_marker(self):
        for marker in ('AirportItlwm-Tahoe-B1-ZC27-STABILITY', '', 'ZC28'):
            with self.assertRaises(d.DecodeError):decode(provider(marker=marker))

    def test_05_ambiguous_even_identical_provider(self):
        p = provider()
        with self.assertRaises(d.DecodeError):d.decode_plist(plistlib.dumps([p, p]))

    def test_06_nested_provider(self):
        r = d.decode_plist(plistlib.dumps([{'IORegistryEntryChildren': [provider()]}]))
        self.assertIn('IORegistryEntryChildren/0', r['provider_path'])

    def test_07_health_missing_untrusted_raw(self):
        r = decode(provider(h=None))
        self.assertFalse(r['diagnostic_lifetime_trusted'])
        self.assertEqual(r['raw_words'][0], 28)
        self.assertIsNone(r['health_sampled_fault_fields'])

    def test_08_health_malformed_untrusted_raw(self):
        p = provider();p[d.HEALTH_KEY] = bytes(255)
        r = decode(p);self.assertFalse(r['diagnostic_lifetime_trusted'])
        self.assertIn('256', r['trust_issues'][0])

    def test_09_health_schema_untrusted_raw(self):
        h = health(words());h[1] = 2
        self.assertFalse(decode(provider(h=h))['diagnostic_lifetime_trusted'])

    def test_10_each_lifetime_mismatch(self):
        for index, name in ((5, 'mode'), (6, 'generation'), (11, 'reset_epoch'), (12, 'command_epoch')):
            h = health(words());h[index] += 1
            r = decode(provider(h=h));self.assertFalse(r['diagnostic_lifetime_trusted'])
            self.assertTrue(any(name in issue for issue in r['trust_issues']))

    def test_11_unknown_mode_not_qualified(self):
        for mode in (0, 3):
            r = decode(provider(words(mode)))
            self.assertFalse(r['diagnostic_lifetime_trusted'])

    def test_12_equal_publication_allowed(self):
        p = provider();r1 = decode(p);r2 = decode(p)
        self.assertTrue(r1['diagnostic_lifetime_trusted'] and r2['diagnostic_lifetime_trusted'])

    def test_13_publication_mismatch_reported_not_invented_fault(self):
        h = health(words());h[3] += 1;h[4] += 100
        r = decode(provider(h=h));self.assertTrue(r['diagnostic_lifetime_trusted'])
        self.assertTrue(any('not globally atomic' in warning for warning in r['warnings']))
        self.assertTrue(r['health_sampled_fault_fields_all_zero'])

    def test_14_faults_reported(self):
        h = health(words());h[8] = 1
        r = decode(provider(h=h));self.assertTrue(r['diagnostic_lifetime_trusted'])
        self.assertFalse(r['health_sampled_fault_fields_all_zero'])
        self.assertEqual(r['health_sampled_fault_fields']['pc1_fault'], 1)

    def test_15_invalid_queue_retains_raw(self):
        w = words();w[10] = 0
        r = decode(provider(w));self.assertIsNone(r['queue'])
        self.assertEqual(r['raw_queue_words'][0], 4)
        self.assertTrue(any('invalid' in warning for warning in r['warnings']))

    def test_16_flags_must_be_valid(self):
        for index, value in ((9, 0), (9, 2), (10, 2)):
            w = words();w[index] = value
            with self.assertRaises(d.DecodeError):decode(provider(w))

    def test_17_raw_zero_status_is_valid_after_event(self):
        w = words();w[24] = 1;w[25] = 0;w[28] = 1;w[29] = 0
        r = decode(provider(w))['rare_events']
        self.assertEqual(r['last_single_status'], 0)
        self.assertEqual(r['last_single_type'], 0)
        self.assertEqual(r['last_normalized_rate'], 0)
        self.assertEqual(r['last_rate_notification_version'], 0)

    def test_18_no_event_does_not_invent_last_value(self):
        r = decode(provider())['rare_events']
        self.assertIsNone(r['last_single_status'])
        self.assertIsNone(r['last_normalized_rate'])

    def test_19_packed_fields(self):
        w = words();w[24] = 2;w[25] = 0xab1234;w[28] = 2;w[29] = (7 << 32) | 0xdeadbeef
        r = decode(provider(w))['rare_events']
        self.assertEqual((r['last_single_status'], r['last_single_type']), (0x1234, 0xab))
        self.assertEqual((r['last_normalized_rate'], r['last_rate_notification_version']), (0xdeadbeef, 7))

    def test_20_lifetime_scopes_explicit(self):
        r = decode(provider())
        self.assertIn('HAL attachment', r['history_scope']['rare_events'])
        self.assertIn('not a HAL/boot', r['history_scope']['queue'])
        self.assertTrue(any('same boot' in warning for warning in r['warnings']))

    def test_21_no_live_tools(self):
        source = DECODER.read_text()
        self.assertNotIn('subprocess', source)
        self.assertNotIn('os.system', source)

    def test_22_binary_plist(self):
        r = d.decode_plist(plistlib.dumps([provider()], fmt=plistlib.FMT_BINARY))
        self.assertTrue(r['diagnostic_lifetime_trusted'])

    def test_23_cli_multiple_files_no_deltas(self):
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for i in (10, 1):
                w = words();w[3] = i;w[16] = i
                path = Path(tmp) / (str(i) + '.plist');path.write_bytes(plistlib.dumps([provider(w)]));paths.append(path)
            r = subprocess.run([sys.executable, str(DECODER), *map(str, paths)], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            result = json.loads(r.stdout);self.assertFalse(result['automatic_deltas'])
            self.assertEqual([x['publication'] for x in result['records']], [10, 1])

    def test_24_bad_input_and_absent_provider(self):
        for blob in (b'not a plist', plistlib.dumps([{}])):
            with self.assertRaises(d.DecodeError):d.decode_plist(blob)


if __name__ == '__main__':
    unittest.main(verbosity=2)
