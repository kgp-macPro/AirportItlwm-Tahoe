#!/usr/bin/env python3
"""Synthetic saved-plist decoder regression tests; no live registry access."""
import importlib.util
import json
from pathlib import Path
import plistlib
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('zc28_decoder', ROOT/'tools/decode_zc28_rx.py')
DEC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEC)


def pack(words):
    return struct.pack('<'+'Q'*len(words), *words)


def health(mode=2, generation=8, epoch=12):
    w = [0]*32
    w[:3] = [19, 1, 256]
    w[3:7] = [80, 1000000, mode, generation]
    w[11:13] = [epoch, 22]
    return pack(w)


def rx(serial=5, reason=0, generation=8, epoch=12, captured=None):
    w = [0]*64
    w[:3] = [4, serial, reason]
    w[4:6] = [1, 1]
    w[11:14] = [generation, epoch, epoch]
    w[17] = serial*10000 if captured is None else captured
    w[24:40] = [100, 12, 0, 1, 1000, 2, 3, 15, 90, 100, 900, 100, 0, 0, 0, 0]
    w[46:49] = [2048, 512, 1]
    return pack(w)


def pressure(event=1):
    w = [0]*24
    w[:3] = [4, event, 123456]
    w[3:19] = list(range(16))
    return pack(w)


def node(mode=2):
    return {'IORegistryEntryName': 'ARPT', 'KGP_RX_EXPERIMENT': DEC.MARKER,
            'KGP_ZC19': health(mode), 'KGP_RX_ZC4': rx(1, 1),
            'KGP_RX_ZC4_PERIODIC': rx(5, 0)}


class DecoderTests(unittest.TestCase):
    def test_nested_provider(self):
        d = DEC.decode_saved([{'IORegistryEntryChildren': [node()]}])
        self.assertEqual(d['current_rx_key'], 'KGP_RX_ZC4_PERIODIC')
        self.assertEqual(d['current_rx']['snapshot_serial'], 5)
        self.assertEqual(d['current_rx']['pool_ready'], 100)
        self.assertEqual(d['current_rx']['pool_mapping_failures'], 0)

    def test_all_qwords_named_and_original_bytes_retained(self):
        n = node()
        for key in DEC.PRESSURE_KEYS:
            n[key] = pressure(1 if key.endswith('FIRST_PRESSURE') else 2)
        d = DEC.decode_saved(n)
        self.assertEqual(len(d['current_rx']), 64)
        for r in d['rx_records'] + d['pressure_records']:
            self.assertEqual(bytes.fromhex(r['raw_hex']), n[r['key']])
            self.assertEqual(pack(r['raw_qwords']), n[r['key']])
        self.assertEqual(len(d['pressure_records']), 4)

    def test_setup_serial_reset_old_forced_retained(self):
        n = node()
        n['KGP_RX_ZC4'] = rx(999, 3, generation=7, epoch=11)
        n['KGP_RX_ZC4_PERIODIC'] = rx(2, 0, generation=8, epoch=12)
        d = DEC.decode_saved(n)
        self.assertEqual(d['current_rx']['snapshot_serial'], 2)
        self.assertEqual(d['retained_other_lifetime_forced_keys'], ['KGP_RX_ZC4'])

    def test_old_periodic_cannot_hide_newer_forced(self):
        n = node()
        n['KGP_RX_ZC4'] = rx(6, 3)
        d = DEC.decode_saved(n)
        self.assertEqual(d['current_rx_key'], 'KGP_RX_ZC4')
        self.assertEqual(d['current_rx']['reason'], 3)

    def test_each_lifetime_component_required(self):
        for index in (11, 12, 13):
            with self.subTest(index=index):
                n = node()
                del n['KGP_RX_ZC4']
                w = list(struct.unpack('<64Q', n['KGP_RX_ZC4_PERIODIC']))
                w[index] += 1
                n['KGP_RX_ZC4_PERIODIC'] = pack(w)
                d = DEC.decode_saved(n)
                self.assertIsNone(d['current_rx'])
                self.assertEqual(d['selection_status'], 'no_matching_rx_record')

    def test_conventional_no_pool(self):
        n = node(1)
        del n['KGP_RX_ZC4'], n['KGP_RX_ZC4_PERIODIC']
        d = DEC.decode_saved(n)
        self.assertEqual(d['selection_status'], 'conventional_no_active_mapped_pool')
        self.assertEqual(d['rx_records'], [])

    def test_conventional_stale_properties_retained_not_active(self):
        d = DEC.decode_saved(node(1))
        self.assertIsNone(d['current_rx'])
        self.assertEqual(len(d['rx_records']), 2)

    def test_forced_reasons_retained_across_lifetimes(self):
        for reason in (3, 4, 5):
            with self.subTest(reason=reason):
                n = node()
                n['KGP_RX_ZC4'] = rx(50, reason, generation=7, epoch=11)
                d = DEC.decode_saved(n)
                self.assertEqual(d['retained_other_lifetime_forced_keys'], ['KGP_RX_ZC4'])

    def test_faults_exposed_for_forensics_not_suppressed(self):
        n = node()
        w = list(struct.unpack('<32Q', n['KGP_ZC19'])); w[8:11] = [9, 1, 1]
        n['KGP_ZC19'] = pack(w)
        d = DEC.decode_saved(n)
        self.assertEqual((d['health']['pc1_fault'],d['health']['tx_blocked'],d['health']['command_frozen']), (9,1,1))

    def test_equal_serial_disagreement_rejected(self):
        n = node(); n['KGP_RX_ZC4'] = rx(5, 1)
        with self.assertRaisesRegex(DEC.DecodeError, 'equal-serial'):
            DEC.decode_saved(n)

    def test_reused_tuple_setup_serial_rollback_rejected(self):
        n = node(); n['KGP_RX_ZC4'] = rx(1, 1, captured=1000000)
        n['KGP_RX_ZC4_PERIODIC'] = rx(100, 0, captured=500000)
        with self.assertRaisesRegex(DEC.DecodeError, 'serial rollback'):
            DEC.decode_saved(n)

    def test_ambiguous_provider_rejected(self):
        with self.assertRaisesRegex(DEC.DecodeError, 'unambiguous'):
            DEC.decode_saved([node(), node()])

    def test_missing_or_wrong_marker_rejected(self):
        for marker in (None, 'AirportItlwm-Tahoe-B1-ZC24-PERF', b'ZC28'):
            n = node(); n['KGP_RX_EXPERIMENT'] = marker
            with self.assertRaisesRegex(DEC.DecodeError, 'marker'):
                DEC.decode_saved(n)

    def test_missing_provider_and_health_rejected(self):
        for n in ({}, {'KGP_RX_EXPERIMENT': DEC.MARKER}):
            with self.assertRaises(DEC.DecodeError): DEC.decode_saved(n)

    def test_health_schema_revision_bytes_mode_rejected(self):
        for index, value in ((0,18),(1,2),(2,255),(5,0),(5,3)):
            with self.subTest(index=index,value=value):
                n = node(); w=list(struct.unpack('<32Q',n['KGP_ZC19'])); w[index]=value
                n['KGP_ZC19']=pack(w)
                with self.assertRaises(DEC.DecodeError): DEC.decode_saved(n)

    def test_present_malformed_records_rejected_even_conventional(self):
        for mode in (1,2):
            for key in ('KGP_ZC19',)+DEC.MAIN_KEYS+DEC.PRESSURE_KEYS:
                with self.subTest(mode=mode,key=key):
                    n=node(mode);n[key]=b'bad'
                    with self.assertRaises(DEC.DecodeError): DEC.decode_saved(n)

    def test_schema_reason_and_boolean_validation(self):
        for index,value in ((0,5),(2,2),(4,2),(45,5)):
            n=node();w=list(struct.unpack('<64Q',n['KGP_RX_ZC4_PERIODIC']));w[index]=value
            n['KGP_RX_ZC4_PERIODIC']=pack(w)
            with self.assertRaises(DEC.DecodeError): DEC.decode_saved(n)

    def test_pressure_wrong_event_rejected(self):
        n=node();n['KGP_RX_ZC4_FIRST_PRESSURE']=pressure(2)
        with self.assertRaisesRegex(DEC.DecodeError,'pressure schema/event'): DEC.decode_saved(n)

    def test_saved_xml_and_binary_cli_only(self):
        with tempfile.TemporaryDirectory(prefix='zc28-decoder-') as tmp:
            for fmt in (plistlib.FMT_XML, plistlib.FMT_BINARY):
                p=Path(tmp)/'saved.plist';p.write_bytes(plistlib.dumps([node()],fmt=fmt))
                r=subprocess.run([sys.executable,str(ROOT/'tools/decode_zc28_rx.py'),str(p)],capture_output=True,text=True)
                self.assertEqual(r.returncode,0,r.stderr)
                self.assertEqual(json.loads(r.stdout)['current_rx']['snapshot_serial'],5)
            p.write_bytes(b'not plist')
            r=subprocess.run([sys.executable,str(ROOT/'tools/decode_zc28_rx.py'),str(p)],capture_output=True,text=True)
            self.assertEqual(r.returncode,2)
            self.assertEqual(r.stdout,'')


if __name__ == '__main__':
    unittest.main(verbosity=2)
