#!/usr/bin/env python3
"""Compile actual HE IE parsers with ASan/UBSan against bounded AP fixtures."""
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NODE = (ROOT / "itl80211/openbsd/net80211/ieee80211_node.c").read_text()


def function(name):
    mark = "\n" + name + "("
    start = NODE.index(mark)
    start = NODE.rfind("\nint", 0, start)
    brace = NODE.index("{", start)
    level = 1
    end = brace + 1
    while level:
        level += (NODE[end] == "{") - (NODE[end] == "}")
        end += 1
    return NODE[start:end]


code = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define __packed __attribute__((packed))
#define IEEE80211_NODE_HECAP 0x400000
#define IEEE80211_NODE_HEOP 0x800000
#define IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G 0x08
#define IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G 0x10
#define IEEE80211_HE_PHY_CAP6_PPE_THRESHOLD_PRESENT 0x80
#define IEEE80211_HE_OPERATION_VHT_OPER_INFO 0x00004000
#define IEEE80211_HE_OPERATION_CO_HOSTED_BSS 0x00008000
#define IEEE80211_HE_OPERATION_6GHZ_OP_INFO 0x00020000
#define le32toh(x) (x)
#define le16toh(x) (x)
struct ieee80211_he_cap_elem { uint8_t mac_cap_info[6], phy_cap_info[11]; } __packed;
struct ieee80211_he_mcs_nss_supp {
    uint16_t rx_mcs_80,tx_mcs_80,rx_mcs_160,tx_mcs_160,rx_mcs_80p80,tx_mcs_80p80;
} __packed;
struct ieee80211_he_operation { uint32_t he_oper_params; uint16_t he_mcs_nss_set; uint8_t optional[]; } __packed;
struct ieee80211_node {
    uint32_t ni_flags=0;
    ieee80211_he_cap_elem ni_he_cap_elem={};
    ieee80211_he_mcs_nss_supp ni_he_mcs_nss_supp={};
    uint8_t ni_ppe_thres[25]={};
    uint32_t ni_he_oper_params=0;
    uint16_t ni_he_oper_nss_set=0;
    uint8_t ni_he_optional[9]={};
};
static uint8_t ieee80211_he_mcs_nss_size(const ieee80211_he_cap_elem *cap) {
    return 4 + ((cap->phy_cap_info[0]&8)?4:0) + ((cap->phy_cap_info[0]&16)?4:0);
}
static uint8_t ieee80211_he_ppe_size(uint8_t first,const uint8_t *phy) {
    if (!(phy[6]&0x80)) return 0;
    unsigned n=__builtin_popcount(first&0x78)*(1+(first&7));
    return (n*6+7+7)/8;
}
static uint8_t ieee80211_he_oper_size(const uint8_t *data) {
    uint32_t flags;
    memcpy(&flags,data,4);
    return 7 + ((flags&0x4000)?3:0) + ((flags&0x8000)?1:0) + ((flags&0x20000)?5:0);
}
'''
code += function("ieee80211_setup_hecaps") + "\n" + function("ieee80211_setup_heop")
code += r'''
int main() {
    ieee80211_node n;
    uint8_t cap[32]={};
    cap[17]=0xfa; cap[18]=0xff; cap[19]=0xfa; cap[20]=0xff;
    assert(!ieee80211_setup_hecaps(&n,cap,16));
    assert(!(n.ni_flags&IEEE80211_NODE_HECAP));
    assert(!ieee80211_setup_hecaps(&n,cap,20));
    assert(ieee80211_setup_hecaps(&n,cap,21));
    assert(n.ni_flags&IEEE80211_NODE_HECAP);
    assert(n.ni_he_mcs_nss_supp.rx_mcs_160==0xffff);
    cap[6+6]=0x80; // phy_cap_info[6]
    cap[21]=0x08; cap[22]=0x12;
    assert(!ieee80211_setup_hecaps(&n,cap,21));
    assert(!ieee80211_setup_hecaps(&n,cap,22));
    assert(ieee80211_setup_hecaps(&n,cap,23));
    assert(n.ni_ppe_thres[0]==0x08 && n.ni_ppe_thres[1]==0x12);
    cap[6]=0x08; cap[12]=0;
    cap[21]=0xfa; cap[22]=0xff; cap[23]=0xfa; cap[24]=0xff;
    assert(!ieee80211_setup_hecaps(&n,cap,24));
    assert(ieee80211_setup_hecaps(&n,cap,25));
    assert(n.ni_he_mcs_nss_supp.rx_mcs_160==0xfffa);
    uint8_t op[16]={}; op[4]=0xfa; op[5]=0xff;
    assert(!ieee80211_setup_heop(&n,op,5));
    assert(ieee80211_setup_heop(&n,op,6));
    assert(n.ni_flags&IEEE80211_NODE_HEOP);
    op[1]=0x40; op[6]=1; op[7]=2; op[8]=3;
    assert(!ieee80211_setup_heop(&n,op,8));
    assert(ieee80211_setup_heop(&n,op,9));
    assert(n.ni_he_optional[0]==1 && n.ni_he_optional[2]==3);
    puts("PASS actual HE IE parsers: truncated, PPE, 80/160 MCS, optional operation");
}
'''

with tempfile.TemporaryDirectory(prefix="p10-he-ie-") as td:
    source = Path(td) / "he_ie.cpp"
    exe = Path(td) / "he_ie"
    source.write_text(code)
    compiler = subprocess.check_output(["xcrun", "--find", "clang++"], text=True).strip()
    sdk = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
    command = [compiler, "-isysroot", sdk, "-std=c++17", "-O1", "-g", "-fsanitize=address,undefined",
               str(source), "-o", str(exe)]
    subprocess.run(command, check=True)
    subprocess.run([str(exe)], check=True)
print("node.c sha256", hashlib.sha256(NODE.encode()).hexdigest())
