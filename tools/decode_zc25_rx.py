#!/usr/bin/env python3
"""Decode SAVED ZC25 IORegistry plists; never query hardware or live registry."""
import argparse
import json
from pathlib import Path
import plistlib
import struct
import sys

MARKER = 'AirportItlwm-Tahoe-B1-ZC25-PERF'
MAIN_KEYS = ('KGP_RX_ZC4', 'KGP_RX_ZC4_PERIODIC')
PRESSURE_KEYS = ('KGP_RX_ZC4_FIRST_PRESSURE', 'KGP_RX_ZC4_LAST_RECOVERY',
                 'KGP_RX_ZC4_PERIODIC_FIRST_PRESSURE', 'KGP_RX_ZC4_PERIODIC_LAST_RECOVERY')
MAIN_NAMES = (
    'schema', 'snapshot_serial', 'reason', 'rx_fault', 'running', 'published',
    'terminal', 'closing', 'synchronized', 'replaced', 'starved', 'rx_generation',
    'posted_rx_reset_epoch', 'recorded_reset_epoch', 'terminal_epoch', 'pc1_fault',
    'pc1_suspended', 'captured_uptime_ns', 'notifications', 'pressure_episodes',
    'pressure_recoveries', 'pressure_active', 'batch_queued', 'batch_max',
    'pool_ready', 'pool_retired', 'pool_working', 'pool_scheduled', 'pool_supplied',
    'pool_pressure', 'pool_retries', 'pool_retired_peak', 'pool_ready_min',
    'pool_worker_runs', 'pool_reused', 'pool_renewed', 'pool_allocation_failures',
    'pool_mapping_failures', 'pool_header_failures', 'pool_fault',
    'life_live', 'life_created', 'life_freed', 'life_callbacks', 'life_detached',
    'life_close_state', 'pool_entry_limit', 'rx_ring_count', 'pool_present',
    'clean_handoff', 'property_failures',
) + tuple('reserved_' + str(i) for i in range(51, 64))
POOL_NAMES = MAIN_NAMES[24:40]
HEALTH_NAMES = ('schema', 'revision', 'bytes', 'publication', 'captured_uptime_ns',
                'mode', 'generation', 'device_family', 'pc1_fault', 'tx_blocked',
                'command_frozen', 'reset_epoch', 'command_epoch', 'pc1_suspended',
                'rx_fault', 'if_flags') + tuple('ba_stat_' + str(i) for i in range(12)) + (
                'snapshot_misses', 'property_failures', 'reserved_30', 'reserved_31')
REASONS = {0: 'watchdog_periodic', 1: 'rx_arm', 3: 'terminal_close',
           4: 'unproven_terminal_close', 5: 'deferred_cleanup_handoff'}


class DecodeError(ValueError):
    pass


def _qwords(value, count, key):
    if not isinstance(value, bytes) or len(value) != count * 8:
        raise DecodeError(f'{key}: expected exactly {count * 8} OSData bytes')
    return list(struct.unpack('<' + 'Q' * count, value))


def _providers(value, path='$'):
    if isinstance(value, dict):
        if 'KGP_ZC19' in value or 'KGP_RX_EXPERIMENT' in value:
            yield path, value
        for key, child in value.items():
            if isinstance(child, (dict, list)):
                yield from _providers(child, path + '/' + str(key))
    elif isinstance(value, list):
        for i, child in enumerate(value):
            yield from _providers(child, path + '[' + str(i) + ']')


def decode_saved(document):
    providers = list(_providers(document))
    if len(providers) != 1:
        raise DecodeError(f'expected one unambiguous health/experiment provider, found {len(providers)}')
    path, node = providers[0]
    if node.get('KGP_RX_EXPERIMENT') != MARKER:
        raise DecodeError('provider marker is not exact ZC25 candidate marker')
    health_raw = _qwords(node.get('KGP_ZC19'), 32, 'KGP_ZC19')
    if health_raw[:3] != [19, 1, 256]:
        raise DecodeError('KGP_ZC19: unsupported schema/revision/byte count')
    if health_raw[5] not in (1, 2):
        raise DecodeError('KGP_ZC19: mode must be qualified conventional(1) or mapped(2)')
    health = dict(zip(HEALTH_NAMES, health_raw))
    warnings = ['IORegistry properties and pool atomic counters are not a globally atomic snapshot.',
                'uptime_ns is local monotonic boot time; do not subtract unsynchronized host clocks.',
                'Pressure records contain no generation/epoch tuple; their lifetime cannot be inferred from their key.']
    records = []
    for key in MAIN_KEYS:
        if key not in node:
            continue
        raw = _qwords(node[key], 64, key)
        if raw[0] != 4:
            raise DecodeError(f'{key}: unsupported RX schema')
        if raw[2] not in REASONS or (key.endswith('_PERIODIC') and raw[2] != 0) or (
                key == 'KGP_RX_ZC4' and raw[2] == 0):
            raise DecodeError(f'{key}: reason does not match forced/periodic publication contract')
        for i in (4, 5, 6, 7, 16, 21, 27, 48, 49):
            if raw[i] not in (0, 1):
                raise DecodeError(f'{key}: non-boolean flag at qword {i}')
        if raw[45] > 4:
            raise DecodeError(f'{key}: invalid RX life close state')
        matches = raw[11] == health_raw[6] and raw[12] == health_raw[11] and raw[13] == health_raw[11]
        records.append({'key': key, 'raw_qwords': raw, 'raw_hex': node[key].hex(),
                        'fields': dict(zip(MAIN_NAMES, raw)), 'reason_name': REASONS[raw[2]],
                        'matches_health_lifetime': matches,
                        'terminal_or_handoff': raw[2] in (3, 4, 5)})
    pressure = []
    for key in PRESSURE_KEYS:
        if key not in node:
            continue
        raw = _qwords(node[key], 24, key)
        expected = 1 if key.endswith('FIRST_PRESSURE') else 2
        if raw[0] != 4 or raw[1] != expected:
            raise DecodeError(f'{key}: invalid pressure schema/event')
        names = ('schema', 'event', 'captured_uptime_ns') + POOL_NAMES + (
            'replaced', 'starved', 'pc1_fault', 'rx_fault', 'life_detached')
        pressure.append({'key': key, 'raw_qwords': raw, 'raw_hex': node[key].hex(),
                         'fields': dict(zip(names, raw)), 'lifetime': 'not encoded'})

    current = None
    status = 'conventional_no_active_mapped_pool' if health_raw[5] == 1 else 'no_matching_rx_record'
    matching = [r for r in records if r['matches_health_lifetime']]
    if health_raw[5] == 2 and matching:
        # The pool setup resets its serial. If the encoded generation/reset
        # tuple has been reused, conflicting serial/time order cannot prove a
        # unique pool lifetime. Do not let an old large serial win silently.
        if len(matching) > 1:
            a, b = sorted(matching, key=lambda r: r['raw_qwords'][17])[:2]
            if a['raw_qwords'][17] < b['raw_qwords'][17] and a['raw_qwords'][1] > b['raw_qwords'][1]:
                raise DecodeError('matching epoch tuple has serial rollback versus capture time; RX setup lifetime is ambiguous')
        max_serial = max(r['raw_qwords'][1] for r in matching)
        newest = [r for r in matching if r['raw_qwords'][1] == max_serial]
        if len(newest) > 1 and newest[0]['raw_qwords'] != newest[1]['raw_qwords']:
            raise DecodeError('same-lifetime equal-serial RX records disagree; cannot select one')
        current = newest[0]
        status = 'matching_mapped_rx_record'
    elif health_raw[5] == 2:
        warnings.append('No RX record matches health generation AND both recorded RX/reset epochs; no fallback selected.')
    if health_raw[5] == 1 and records:
        warnings.append('RX pool properties retained in conventional mode are historical evidence, not an active mapped pool.')
    return {'decoder': 'ZC25_RX_SAVED_PLIST_1', 'provider_path': path,
            'provider_name': node.get('IORegistryEntryName'), 'candidate_marker': MARKER,
            'health': health, 'health_raw_qwords': health_raw,
            'health_raw_hex': node['KGP_ZC19'].hex(), 'selection_status': status,
            'current_rx_key': current['key'] if current else None,
            'current_rx': current['fields'] if current else None,
            'rx_records': records, 'pressure_records': pressure,
            'retained_other_lifetime_forced_keys': [r['key'] for r in records
                if not r['matches_health_lifetime'] and r['terminal_or_handoff']],
            'warnings': warnings}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plist', type=Path, help='existing saved ioreg XML or binary plist')
    args = parser.parse_args(argv)
    try:
        document = plistlib.loads(args.plist.read_bytes())
        result = decode_saved(document)
    except (OSError, ValueError, TypeError, plistlib.InvalidFileException) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        return 2
    json.dump(result, sys.stdout, indent=2)
    print()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
