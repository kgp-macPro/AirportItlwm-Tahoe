# AirportItlwm-Tahoe 1.1.0 Beta — Test results

[Download the comparison beta](https://github.com/kgp-macPro/AirportItlwm-Tahoe/releases/tag/v1.1.0-beta.1) · [Project and result tables](https://github.com/kgp-macPro/AirportItlwm-Tahoe#readme) · [Installation and all test commands](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/README_TESTS.txt)

This is the ONE shared results thread for all testers of AirportItlwm-Tahoe
1.1.0 Beta. Start with ZC28; ZC8/19/22/26 are optional historical controls and
unchanged laobamac53 is a separately attributed external control. Add each
campaign as a COMMENT HERE; do not open one results issue per tester/system.
Separate issues are for concrete bugs requiring independent investigation;
link the relevant result comment/Return-ZIP and preserve its context.

Follow [README_TESTS.txt](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/main/README_TESTS.txt) for prerequisites, installation, rollback and all commands.
KGP requires OCLP-CustoMac 3.0.3 with its Skywalk replacement, AMFIPass.kext
and -amfipassbeta plus the matching supported framework/boot configuration.
For Wi-Fi, external53's native approach does NOT require OCLP, Skywalk replacement
or -amfipassbeta. A kext replacement does not undo root patches: report the
ACTUAL environment, not one inferred from the driver's capabilities.
AX201 remains KGP conventional-only. External DMA labels are declared settings,
not measured KGP Mode2/fault telemetry. Unknown is not zero or unpatched.

For EACH campaign attach ONE Return-ZIP containing the untouched runner-result
ZIP, Ookla screenshot/result URL and short configuration notes. Include failures,
slow runs and interrupted attempts. If Ookla could not run, say why. Preserve
original inner ZIPs/checksums; keep result archives as attachments, not source
commits. Do not attach the full beta download.

## Copy this comment template

Start the FIRST line with card | candidate | AppleVTD configuration | actual DMA.
Replace the placeholders, attach one campaign ZIP, and post one comment:

```text
CARD | CANDIDATE | AppleVTD=on/off | DMA=conventional Mode1 / mapped Mode2 / unavailable (external declared)
System/CPU; macOS version/build; loaded UUID/hash or identity-check failure:
ACTUAL environment: OCLP version/root patches; Skywalk replacement/version; AMFIPass/-amfipassbeta; loaded wireless frameworks (unknown if not established):
AP/model/channel/width/topology; client Wi-Fi interface/IP:
Wired server OS/iperf3; adapter/chip/link/interface/IP:
Campaign date/time/timezone; first/repeat; prior traffic/sleep; stalls/failures:
Return-ZIP attachment (untouched runner ZIP + Ookla screenshot/result URL + notes):
Ookla server/time/link, or why unavailable; existing report link (no duplicate upload):
```

IM and TM submissions in the respective shared beta threads are equally welcome.
Link an existing report instead of uploading the same ZIP twice; forum submission
does not require a GitHub account. Use the same identifying first line and
one-Return-ZIP rule. Existing project threads (new beta-post permalinks pending):
- https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/
- https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/

The prerelease uses tag `v1.1.0-beta.1` and one complete download,
`AirportItlwm-Tahoe-1.1.0-Beta.zip`. New forum campaign-post permalinks will be
added after KGP posts there manually. Qualified 1.0.0 remains distinct from this
experimental comparison beta.
