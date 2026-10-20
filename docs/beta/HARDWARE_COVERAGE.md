# Hardware coverage and evidence boundaries — AirportItlwm-Tahoe 1.1.0 Beta

Source baseline: **ZC28 `483940988c502379fb6d3dcde885dca83f00b7aa`**, tree
`c92481ec7f00f744e058ef398114cffc4de0955a`. The public repository root still
contains frozen 1.0.0 driver source; do not use it to infer ZC28 coverage.
This overview changes documentation only, not PCI matching, DMA policy or r7.

## Physical evidence versus source eligibility

| Hardware | Actual evidence | DMA/configuration boundary | ZC28 status |
| --- | --- | --- | --- |
| KGP AX210, PCI 8086:2725 | Frozen qualified 1.0.0 and later identified original campaigns; ZC28 has three complete mapped and three complete conventional campaigns | ZC28 saved actual Mode2 / Mode1 respectively, separate boots; same frozen ZC28 binary | Physically tested on this system; three repeats per mode share that mode's boot. Not universal stability or repeated-wake qualification |
| AX201, tester **@netiave (TM) / @funcio (IM)**, one person | Earlier ZC5-BOOTFIX1 reports: TCP TX 324/327, RX 601/569Mbps; UUID 1A496783-086D-37D1-85B3-EA6110F301C9 | KGP conventional route only, including when system AppleVTD was active. Historical reported values, not newly executed validation | No supplied ZC28 peer qualification |
| AX211, **Luiz (@LuizMeliga on TM)** | Earlier ZC8 originals: TCP RX P1 rounds 600/617/601, P4 615/649/648Mbps | System AppleVTD was active; those originals did not positively establish KGP numerical mapped Mode2. Different AP/network/server cohort | No supplied ZC28 peer qualification |

The AX201 and AX211 reports motivated this comparison; they do not qualify
all intervening changes. Their provided TM profiles are
https://www.tonymacx86.com/members/1548699/ and
https://www.tonymacx86.com/members/1658846/. No additional account is inferred.
Current KGP prerequisites remain OCLP-CustoMac 3.0.3, its Skywalk replacement,
AMFIPass.kext/-amfipassbeta and matching supported frameworks/boot configuration.

## Additional IWX source candidates — NOT physical qualification

| Source-derived group | What is present in frozen ZC28 | Limit |
| --- | --- | --- |
| AX200; AX101 / AX203 variants | IWX match/config paths with enclosed CC or Qu/QuZ/So68 firmware for specific MAC/RF/subdevice combinations | Await physical confirmation; a retail name does not determine the selected family or DMA route |
| Killer AX1650w/x (AX200), AX1650i/s (AX201), AX1675w/x (AX210), AX1675i/s (AX211), AX1690i/s (AX411) | Explicit device/subdevice names and corresponding IWX configurations | OEM/revision combinations must actually match; no blanket mapped qualification. AX201-based models remain conventional-only for this project |
| AX411, selected So/GF4 variants | SO68 firmware and GF4 PNVM are enclosed; matching IWX entries exist | Not physically qualified here; other SnJ/SoSnj variants can select absent firmware |
| Selected Wireless-AC 9461/9462/9560 and Killer 1550i/s on IWX Qu/QuZ/So paths | Named config rows with enclosed 68 firmware for some combinations | Other PCI variants of the same retail names bind IWM instead. Do not advertise the same fix across all variants |

The IWX table also contains AX221/AX231 and further MA/SnJ/QnJ configuration
entries, but their selected MA/SoSnj63 or QuQnj48 firmware is **not enclosed**.
These are table-only references, not working-beta or supported-mapped claims.
A related AX201/AX211/AX411 name in such a row does not make that variant usable.
BE200 remains future work; a matching PCI ID or generic MA entry does not
establish BE200 support. No new firmware, PCI ID, mode or card has been enabled.

## Separate preserved upstream backends

- **IWM**, 28 active PCI table entries: source models 3160/3165/3168,7260/7265,
  8260/8265,9260 and selected9560/9461/9462. Firmware selection is in that backend;
  it does not enter IWX TX-control, IWX DMA selection or ZC28 service scopes.
- **IWN**, 40 active PCI table entries: 4965,5100/5150/5300/5350,
  100/105/130/135/1000/1030,2200/2230,6005/6030/6050/6150/6200/6235/6300.
  These are preserved older backend paths, not new beta performance qualification.

These lists come from active matching arrays, not every constant/name declared
somewhere in source (for example, an unused 4165 constant does not add support).
Shared frontend/net80211 dependencies exist; “separate backend” does not mean
no shared code. No IWM/IWN mapped-mode or same-performance-fix claim is made.
The comparison campaign and unchanged r7 remain focused on AX201/AX210/AX211.
Wider source coverage does not require a larger tester matrix.

## Exact source contracts and references

All references below are local-source facts at the pinned commit above.

1. `AirportItlwm/Info.plist`, `IOKitPersonalities.itlwm.IOPCIMatch`, and the frozen
   bundle Info.plist: outer PCI match set. `AirportItlwm/AirportItlwm.cpp:66`
   (`probe`) selects IWX first, IWM second, IWN third. The legacy target's actual
   probe is used here; the alternative Skywalk wrapper is not substituted.
2. `itlwm/hal_iwx/ItlIwx.cpp:14432` (`iwx_devices`): 19 matched Intel PCI device
   IDs; `:14820` (`iwl_dev_info_table`): 208 config rows, including both
   `IWL_DEV_INFO` and `_IWL_DEV_INFO`. 35 referenced configs;23 have an image in
   the embedded firmware list,12 do not. MAC/RF/subdevice predicates and later
   overrides matter, not PCI ID or marketing name alone.
3. `ItlIwx.cpp:15597` through `:15700` (attach configuration selection): checks
   device/subdevice, MAC/step/RF/core flags, applies QuC0/QuZ/SnJ adjustments,
   assigns `sc_fwname` and `sc_device_family`, then calls `dma5Select`.
4. `ItlIwx.cpp:387` (`dma5Select`): resolved AppleVTD/system/device mapper policy
   and stable selection required; unresolved/contradictory state fails closed.
   Mapped admission rejects `sc_device_family < IWX_DEVICE_FAMILY_AX210`.
   This is a family test, NOT an AX210/AX211 retail-model qualification allowlist.
   AX201 has no new mapped authorization. System AppleVTD-active alone never
   establishes a card's actual DMA route.
5. `ItlIwx.cpp:3470` (`iwx_read_firmware` lookup): `getFWDescByName(sc_fwname)`
   must succeed or returns EINVAL. `include/FwBinary.cpp`, final `fwList` array,
   contains 35 total embedded firmware/PNVM records across all three backends.
   The machine-readable ledger provides exact list lines and missing names.
6. `ItlIwx.cpp:4445` (TX ring allocation/control layout), `:9236` and `:16631`
   (packet control construction/publication), and `:12848` (`_iwx_start_task`):
   IWX changes have family/mode-dependent branches. Some gen3 payload/control
   machinery is restricted to family>=AX210; not every listed IWX card executes
   every changed path. Required mapping/backing, completion and full publication
   fences remain the frozen build's existing contracts.
7. `ItlIwx.cpp:12855`/`:12858` and `itl80211/openbsd/sys/_ifq.cpp:192`/`:199`:
   ZC28's finite capacity-handoff scope is entered from IWX service; queue helper
   storage is shared but IWM/IWN do not enter that scope. Hardware-completion
   wake paths remain intact; this is not an ACK-priority or pacing alteration.
8. `itlwm/hal_iwm/mac80211.cpp:4323`/`:4356` and `:4551` onwards: IWM match and
   firmware switch. `itlwm/hal_iwn/ItlIwn.cpp:286`/`:330`: IWN match.

`HARDWARE_SOURCE_LEDGER.json` encloses exact PCI rows, config arguments and line
numbers, selected firmware names/presence, source hashes and preserved backend
lists. This extraction validates documentary claims, not execution on hardware.

## Qualification still pending

ZC28 AX201/AX211 peer comparisons, broader-device testing, repeated sleep/wake
and long-run reliability remain open. The severe earlier ZC26 conventional
incident, ZC28 mapped dips, UDP losses/sender limits and WAN tails are preserved
in the linked original results. Listing candidates does not remove those risks.
External53's native Wi-Fi approach and its declared AppleVTD configurations
remain separate; its actual historical patch/framework evidence is not inferred
from capability. No driver, boot setting or framework was changed for this note.
