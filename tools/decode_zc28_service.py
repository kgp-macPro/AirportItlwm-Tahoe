#!/usr/bin/env python3
"""Decode saved KGP_ZC28_SERVICE plist records. Never queries live hardware.

Schema 28/1 is a fixed 32-QWORD (256-byte), little-endian OSData record.
One or more input files produce independent records; no automatic deltas or
cross-boot identity assumptions are made. See ZC28_SERVICE_TELEMETRY.txt.
"""
import argparse
import json
import plistlib
import struct
import sys
from pathlib import Path

MARKER = 'AirportItlwm-Tahoe-B1-ZC28-SERVICE'
KEY = 'KGP_ZC28_SERVICE'
HEALTH_KEY = 'KGP_ZC19'
QUEUE_NAMES = ('service_begins', 'service_ends', 'eligible_capacity_checks_deferred',
               'capacity_obligations_taken', 'maximum_service_depth',
               'current_service_depth', 'queue_epoch', 'reserved')
RARE_NAMES = ('single_tx_failure_events', 'last_single_status_type_raw',
              'aggregate_response_events', 'last_readable_advertised_frame_count',
              'tlc_update_events', 'last_normalized_rate_version_raw',
              'conventional_rx_allocation_failures', 'conventional_rx_cursor_failures')
FAULTS = {8: 'pc1_fault', 9: 'tx_blocked', 10: 'command_frozen',
          13: 'pc1_suspended', 14: 'rx_fault'}


class DecodeError(ValueError):
    pass


def providers(value, path='$'):
    if isinstance(value, dict):
        if KEY in value:
            yield path, value
        for key, child in value.items():
            yield from providers(child, path + '/' + str(key))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from providers(child, path + '/' + str(index))


def unpack_record(data, schema, label):
    if not isinstance(data, bytes) or len(data) != 256:
        raise DecodeError(label + ' must be exactly 256 bytes of OSData')
    words = struct.unpack('<32Q', data)
    if words[:3] != (schema, 1, 256):
        raise DecodeError(label + ' unsupported schema/revision/declared byte size')
    return words


def decode_plist(data, source='<memory>'):
    try:
        root = plistlib.loads(data)
    except Exception as error:
        raise DecodeError('invalid saved plist: ' + str(error)) from error
    found = list(providers(root))
    if len(found) != 1:
        raise DecodeError('expected exactly one ' + KEY + ' provider; found ' + str(len(found)))
    path, provider = found[0]
    if provider.get('KGP_RX_EXPERIMENT') != MARKER:
        raise DecodeError('provider candidate marker is missing or differs from ' + MARKER)
    w = unpack_record(provider[KEY], 28, KEY)
    if w[9] != 1 or w[10] not in (0, 1):
        raise DecodeError('invalid compiled-handoff or queue-snapshot-valid flag')

    warnings = [
        'Saved IORegistry properties and rare atomic fields are not one global transaction.',
        'Equal mode/generation/reset/command epochs do not establish the same boot or HAL attachment.',
        'No automatic file-to-file deltas are computed.',
    ]
    issues = []
    health = None
    if HEALTH_KEY not in provider:
        issues.append('same-provider KGP_ZC19 is missing')
    else:
        try:
            health = unpack_record(provider[HEALTH_KEY], 19, HEALTH_KEY)
        except DecodeError as error:
            issues.append(str(error))
    if w[5] not in (1, 2):
        issues.append('diagnostic actual DMA mode is unqualified/unknown: ' + str(w[5]))
    if health is not None:
        if health[5] not in (1, 2):
            issues.append('health actual DMA mode is unqualified/unknown: ' + str(health[5]))
        for name, di, hi in [('mode', 5, 5), ('generation', 6, 6),
                             ('reset_epoch', 7, 11), ('command_epoch', 8, 12)]:
            if w[di] != health[hi]:
                issues.append(name + ' mismatch: diagnostic=' + str(w[di]) + ', health=' + str(health[hi]))
        if w[3] != health[3] or w[4] != health[4]:
            warnings.append('Diagnostic and health publication/time differ; registry export is not globally atomic.')
    if any(w[12:16]):
        warnings.append('Nonzero schema-28/1 prefix reserved fields are preserved raw.')
    if w[23]:
        warnings.append('Nonzero queue reserved field is preserved raw.')
    if not w[10] and any(w[16:24]):
        warnings.append('Queue snapshot marked invalid; nonzero queue words are preserved without interpretation.')
    queue = dict(zip(QUEUE_NAMES, w[16:24])) if w[10] else None
    rare = dict(zip(RARE_NAMES, w[24:32]))
    rare['last_single_status'] = (w[25] & 0xffff) if w[24] else None
    rare['last_single_type'] = ((w[25] >> 16) & 0xff) if w[24] else None
    rare['last_normalized_rate'] = (w[29] & 0xffffffff) if w[28] else None
    rare['last_rate_notification_version'] = (w[29] >> 32) if w[28] else None
    faults = {name: health[index] for index, name in FAULTS.items()} if health else None
    return {
        'source': str(source), 'provider_path': path,
        'provider_name': provider.get('IORegistryEntryName'),
        'marker': MARKER, 'schema': 28, 'revision': 1, 'bytes': 256,
        'diagnostic_lifetime_trusted': not issues,
        'trust_scope': 'same-provider sampled DMA mode/generation/reset/command metadata only' if not issues else 'UNTRUSTED RAW DIAGNOSTIC RECORD',
        'trust_issues': issues,
        'publication': w[3], 'health_uptime_ns_copied': w[4],
        'actual_dma_mode': w[5], 'generation': w[6], 'reset_epoch': w[7], 'command_epoch': w[8],
        'finite_capacity_handoff_compiled_present': True,
        'queue_snapshot_valid': bool(w[10]), 'watchdog_publication_failures': w[11],
        'queue': queue, 'rare_events': rare,
        'health_publication': health[3] if health else None,
        'health_sampled_fault_fields': faults,
        'health_sampled_fault_fields_all_zero': all(x == 0 for x in faults.values()) if faults else None,
        'history_scope': {
            'rare_events': 'HAL attachment cumulative; not reset with generation/command/reset epochs',
            'queue': 'queue object cumulative; queue_epoch is queue-local and is not a HAL/boot identifier',
            'queue_current_depth': 'depth at the queue snapshot, normally zero outside the finite consumer scope',
            'capacity_obligations_taken': 'eligible waiting obligation cleared; not proof that a peer or upper TCP received anything',
            'aggregate_frame_count': 'last observer-readable fixed header; can be older than the latest counted aggregate event',
        },
        'warnings': warnings, 'raw_words': list(w),
        'raw_queue_words': list(w[16:24]), 'raw_health_words': list(health) if health else None,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('plists', nargs='+', type=Path, help='saved XML or binary IORegistry plist(s)')
    args = parser.parse_args(argv)
    records = []
    try:
        for path in args.plists:
            records.append(decode_plist(path.read_bytes(), str(path)))
    except (OSError, DecodeError) as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        return 2
    print(json.dumps({'decoder': 'KGP_ZC28_SERVICE 28/1 saved-only',
                      'automatic_deltas': False, 'records': records}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
