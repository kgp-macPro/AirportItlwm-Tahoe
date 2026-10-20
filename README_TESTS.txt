AIRPORTITLWM-TAHOE 1.1.0 BETA — QUICK START AND ALL TEST COMMANDS
20 October 2026

QUICK START — ZC28 FIRST; COMPARISONS OPTIONAL

DIFFERENT PREREQUISITES — READ BEFORE ANY DRIVER INSTALLATION

KGP ZC28 / ZC8 / ZC19 / ZC22 / ZC26 require ALL of:
- OCLP-CustoMac 3.0.3, including its Skywalk replacement and restored legacy
  wireless frameworks.
- AMFIPass.kext with -amfipassbeta.
- The matching supported framework/boot configuration and a recovery path.
This beta does NOT work independently of its required KGP environment.

EXTERNAL laobamac53 is a native Wi-Fi implementation. For Wi-Fi it does NOT
require OCLP, Skywalk replacement or -amfipassbeta. Do not apply the KGP
requirements to the external control. Other hardware can have independent
patch requirements; this package must not remove them automatically.

Replacing AirportItlwm.kext does NOT undo existing root patches or restore
stock Skywalk. Record the ACTUAL framework/root-patch/AMFIPass environment
for EVERY comparison, including historical runs. Native-capable source is
not proof that a run was native/unpatched. Unknown details stay unknown.
See SOURCES/FRAMEWORK_ENVIRONMENT_REGISTER.txt for saved historical evidence.
Desktop setup changes NO patches, EFI, boot arguments or system settings.

KGP INSTALLATION PATH — EXISTING MATCHING PATCHED ENVIRONMENT

This updates an ALREADY WORKING AirportItlwm installation on Intel-based Tahoe
26.x with the required restored legacy wireless frameworks. It is not a
root-patch installer or an automatic EFI installer. New/unsupported setups
must satisfy the project's INSTALL.md first; do not force boot/mapper changes.

1. Download AirportItlwm-Tahoe-1.1.0-Beta.zip and extract it in Downloads.
   Keep the extracted AirportItlwm-Tahoe-1.1.0-Beta folder there. Run this ONE
   command in Terminal (no sudo):

/bin/bash "$HOME/Downloads/AirportItlwm-Tahoe-1.1.0-Beta/SETUP_AirportItlwm_Beta.command"

   Setup verifies the extracted package, creates Desktop candidate/TOOLS/TESTS/
   SOURCES/LICENSES/reference directories and opens THIS quick-start file.
   Existing TESTS stay intact; differing managed files are backed up on Desktop.
   The .command has executable permission, but Finder/Gatekeeper double-click
   behavior is not physically verified. The explicit /bin/bash command is the
   supported launch path, tested natively. Do not disable Gatekeeper/quarantine
   or other system protections to bypass a failure; return the exact message.

2. BEFORE REPLACING YOUR EXISTING DRIVER: keep a known-working recovery boot
   medium/configuration. Use your established method to open the EFI partition
   that ACTUALLY boots this macOS installation. Locate its CURRENT configured
   AirportItlwm.kext (normally EFI/OC/Kexts/AirportItlwm.kext). If the path or
   active bootloader is uncertain, stop and ask rather than modifying another EFI.
   Copy that entire installed bundle to a NEW, separate Desktop rollback folder
   and record which EFI/configuration it came from. This is distinct from the
   automatic Desktop setup backup: setup never sees or backs up the installed EFI.

3. With Finder, replace the ENTIRE existing AirportItlwm.kext bundle at the SAME
   configured location with:
   ~/Desktop/AirportItlwm/B1-ZC28-SERVICE/AirportItlwm.kext
   Remove the old destination bundle first AFTER its rollback copy is verified;
   copy the new whole bundle, not a merge of Contents. Do not rename it, resign
   it, add a second variant, write to /System/Library or /Library/Extensions,
   change root patches, or alter AppleVTD/DisableIoMapper for this first test.
   An existing correct Kernel/Add entry stays BundlePath=AirportItlwm.kext,
   ExecutablePath=Contents/MacOS/AirportItlwm, PlistPath=Contents/Info.plist,
   Enabled=true with its established dependency order. If yours differs, use
   INSTALL.md/configuration review; this package does not edit config.plist.
   Reboot YOURSELF using the same working boot configuration.

4. Check scanning/association and ordinary connectivity. Start iperf3 -s on a
   separate wired server (its Wi-Fi OFF). Perform the actual interface/card/IP
   preparation below, then run ONE matching ZC28 command:

   AX201, or AX210/AX211 on the conventional Mode-1 route:
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 conventional

   Qualified AX210/AX211 AppleVTD-on Mode-2 route (NOT AX201):
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 mapped

   The runner asks for the CURRENT server IPv4 and checks loaded UUID/mode.
   Expected ZC28 UUID: 469CBE97-EEB4-3084-99C4-433E73F4FC23.
   Wrong UUID/mode: preserve the failure ZIP; do not relax checks. After the
   complete 3x14 and final networkQuality, run Ookla and return one outer ZIP
   containing the UNTOUCHED runner ZIP, Ookla screenshot/link and short notes.

ROLLBACK — IF ASSOCIATION, BOOT OR NORMAL PERFORMANCE REGRESSES
Use your known-working recovery boot if necessary. Open the SAME active EFI,
remove the new whole AirportItlwm.kext, restore the ENTIRE verified previous
bundle from the separate rollback folder to the original configured path,
and reboot yourself. Keep the original config/root patches/dependency ordering.
Do not mix files from two bundles. If setup itself needs undoing, use the dated
AirportItlwm_Beta_Backup_* Desktop folder; that backup is NOT an EFI rollback.
The setup action changes only Desktop files, never the running/installed driver.
These deployment steps are reviewed against INSTALL.md; no installation/reboot
or physical deployment validation was performed by Codex in preparing this ZIP.

PUBLIC NAME VERSUS BINARY IDENTITY
AirportItlwm-Tahoe 1.1.0 Beta is the comparison PACKAGE/prerelease name, proposed
Git tag v1.1.0-beta.1. It does not relabel/rebuild the included binaries: five
KGP candidates retain internal 1.0.0 fields and their own UUID/SHA256; the
external control retains its original v2.4.0 development identity. A displayed
version alone cannot identify the loaded build. See SOURCES/SOURCE_BINARY_LICENSE_MAP.json.


Use the SAME common runner as KGP. No new benchmark or result-packaging program.
The supplied runner is unchanged r7-service, SHA256:
be9c07168daf7b8ffd6f6c71ba4f62117d1e28334c4a64a9a063b2a115cc19ed

FILES AFTER SETUP_AirportItlwm_Beta.command HAS FINISHED
~/Desktop/AirportItlwm/B1-ZC8-PERF/AirportItlwm.kext
~/Desktop/AirportItlwm/B1-ZC19-PERF/AirportItlwm.kext
~/Desktop/AirportItlwm/B1-ZC22-PERF/AirportItlwm.kext
~/Desktop/AirportItlwm/B1-ZC26-PERF/AirportItlwm.kext
~/Desktop/AirportItlwm/B1-ZC28-SERVICE/AirportItlwm.kext
~/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh
~/Desktop/AirportItlwm/TESTS/                         your new result ZIPs
~/Desktop/AirportItlwm/REFERENCE_RESULTS/             seven original KGP reference ZIPs
~/Desktop/AirportItlwm/REFERENCE_NOTES.txt            context, results and limitations
~/Desktop/AirportItlwm/ZC28_CONVENTIONAL_ANALYSIS.txt  all three new conventional runs
~/Desktop/AirportItlwm/README_TESTS.txt               this file
~/Desktop/AirportItlwm/SOURCES/                      all corresponding source/build records
~/Desktop/AirportItlwm/LICENSES/                     preserved license texts
~/Desktop/Laobamac/53/AirportItlwm.kext

SETUP_AirportItlwm_Beta.command copies files to these Desktop locations. It does NOT install a kext
into EFI, load a driver, reboot, or change AppleVTD/DisableIoMapper/root patches.
Use your established deployment procedure, install ONE candidate at a time and
boot its matching configuration before running that candidate's command.
Simply having the bundle on the Desktop does not load it. The runner checks
the loaded UUID against that Desktop candidate. Do not run all commands in a row
without installing/booting each corresponding candidate.

WHO CAN PARTICIPATE AND WHAT TO COMPARE
This public beta invites owners of Intel AX201, AX210 and AX211 on Intel-based
systems running macOS Tahoe 26.x, through InsanelyMac (IM) and tonymacx86 (TM).
An invitation to test does not establish compatibility on every host/card/AP.
Use the matching supported driver/framework environment and your established
kext deployment procedure; setup only places files on the Desktop.

Start with ONE complete ZC28 campaign in your normal working configuration,
followed by Ookla. If practical, repeat in the other already working AppleVTD
boot configuration. Volunteers can compare all frozen ZC8/19/22/26/28 on the
same hardware, plus laobamac53 as a separate external control. All individual
commands remain below; a full five-build matrix is optional participation.
If ZC28 regresses, compare the last known good build on the same setup and
report both originals. Preserve failures and slow runs as well as good ones.
We particularly want to know whether the historical strong AX211/AX201 RX
survives later changes, and whether TX, losses and later rounds improve.

All five are experimental comparison betas. ZC26 had a severe conventional
incident; ZC28 has mapped dips. The three new
ZC28 conventional campaigns completed without that sustained collapse; this
is useful beta evidence, not proof of a permanent fix. See the analysis file.

PREREQUISITES
Client: macOS Tahoe 26.x on an Intel-based system, /bin/bash, Python 3.9 or newer, iperf3,
Xcode command-line tools providing
xcrun/dwarfdump, and the normal macOS networkQuality/ioreg/kmutil/route/ipconfig/
ditto/shasum commands. The runner checks these and preserves failures. Report a
missing dependency; do not remove a check. No extra Python package is required.

Server: a separate wired Ethernet computer with iperf3, its Wi-Fi off, reachable
over your router/switch. On that computer run:

iperf3 -s

On Windows the corresponding command is iperf3.exe -s. Record that computer's
current IPv4, OS, iperf3 version, Ethernet adapter/chipset and wired link speed.
Enter its CURRENT IP when the CLIENT runner asks; the displayed .33 is only an
example. Do not inherit KGP's server address or Wi-Fi interface.

ONCE IN EACH NEW CLIENT TERMINAL
First identify your actual Wi-Fi Device:

networksetup -listallhardwareports
printf 'Enter the Device under Hardware Port: Wi-Fi (for example en0): '
read -r WIFI_IFACE
export WIFI_IFACE

Use the supplied default Desktop paths and prompt for the current server:

unset AIRPORTITLWM_ROOT AIRPORTITLWM_TEST_DIR LAOBAMAC_KEXT LAOBAMAC_SOURCE_REF
unset BENCHMARK_PROFILE EXPECTED_MODE CANDIDATE_LABEL EXTERNAL_SOURCE_REF EXTERNAL_DECLARED_MODE
unset IPERF_SERVER IPERF_PORT IOREG_WIFI_SERVICE TEST_GAP_SECONDS

Enter your actual card name in uppercase:
printf 'Wi-Fi card (AX201, AX210 or AX211): '
read -r DEVICE_LABEL
export DEVICE_LABEL

The Wi-Fi node defaults to ARPT. If the required node/record is absent, return
the failure ZIP; let KGP inspect it before setting another IOREG_WIFI_SERVICE.
Do not bypass the identity or mode checks.

APPLE VTD ON/OFF — IMPORTANT CARD DISTINCTION
The runner argument checks an EXPECTED driver route; it does not switch boot
settings. In KGP telemetry conventional is Mode 1, mapped is Mode 2.

AX201: previous peer tests DID have system AppleVTD active. In this driver
the AX201 still uses the original/conventional route, including with AppleVTD
active and DisableIoMapper=false. Use the conventional commands for an AX201
in BOTH AppleVTD-on and AppleVTD-off boot configurations. Record which one was
used. System AppleVTD-on does not require numeric Mode 2 on this card.

AX210/AX211: use mapped for an independently qualified AppleVTD-on/Mode-2 boot.
Use conventional for the matching AppleVTD-off/original-route boot. AppleVTD
presence alone is not proof that the card actually selected Mode 2. A mode
mismatch must be reported with its ZIP, not forced past the guard.

Test both AppleVTD boot configurations where already supported by your setup.
Keep the AP, radio settings, placement, server and Ethernet adapter consistent.
Do not invent a new EFI/patch combination just to force a test to pass.

KGP CANDIDATES — CONVENTIONAL
For all three cards on the original/Mode-1 route, including AX201 with AppleVTD ON.
These are INDIVIDUAL commands after booting the corresponding kext, not a batch:

/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC19 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC22 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC26 conventional
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 conventional

KGP CANDIDATES — QUALIFIED AX210/AX211 APPLEVTD ON / MAPPED
For a qualified AX210/AX211 route; NOT a numeric Mode-2 test for AX201:

/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC8 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC19 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC22 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC26 mapped
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" ZC28 mapped

LAOBAMAC BUILD 53 — APPLEVTD ON AND OFF
The bundle is already at ~/Desktop/Laobamac/53/AirportItlwm.kext, exactly where
the unchanged numbered-build command expects it. Boot that actual driver in
the matching native framework environment first. KGP's OCLP/Skywalk/AMFIPass
requirements above are NOT requirements of this external Wi-Fi implementation.

EXTERNAL INSTALLATION PATH — READY BUNDLE, SEPARATE FRAMEWORK PREREQUISITES
1. Keep a known-working recovery boot and back up the ENTIRE currently installed
   AirportItlwm bundle plus your existing boot/framework configuration.
2. Use an ALREADY verified native-framework configuration supported by build53.
   If this machine has KGP root patches/Skywalk replacement, merely changing the
   driver does NOT undo them. Do not improvise patch removal from this guide;
   use the established, reviewed platform/upstream procedure with recovery.
   If the actual environment is not established, record it as unknown and defer
   the clean native comparison. No automatic patch/EFI conversion is supplied.
3. At the existing configured OpenCore location (normally EFI/OC/Kexts), replace
   ONE whole AirportItlwm.kext with ~/Desktop/Laobamac/53/AirportItlwm.kext.
   Keep BundlePath/ExecutablePath/PlistPath as documented above. Do not merge
   bundles or load competing drivers. Do not apply KGP-only dependencies to a
   native configuration. You control any separately reviewed boot configuration.
4. Reboot yourself, check loaded external UUID AEF5621B-B1D8-3487-BC9D-92E035BC087E,
   association/connectivity and ACTUAL loaded frameworks/patch state, then use
   the matching declared-configuration command below. To roll back, restore
   the whole saved driver AND its matching saved framework/boot environment;
   restoring a driver alone cannot reverse framework/root-patch changes.

If build53 is tested on an already patched system, label that ACTUAL environment
explicitly; do not call the run native/unpatched from source capability alone.
The historical controls are not a clean one-function or framework-identical A/B.
Neither Desktop setup nor the runner applies/removes patches or edits EFI.

AppleVTD ON / declared mapped configuration, for any of the three cards if working:
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 mapped

AppleVTD OFF / declared conventional configuration:
/bin/bash "$HOME/Desktop/AirportItlwm/TOOLS/AirportItlwm_3x14_PORTABLE_IP_PROMPT.sh" LAOBAMAC 53 conventional

For external laobamac, these labels record your declared configuration only.
The runner verifies loaded UUID/candidate hashes at the normal boundaries;
numeric KGP DMA/reset/fault telemetry is unavailable, not zero. The word mapped
in this external command does NOT certify KGP numeric Mode 2 on an AX201.
Declared source: 738d4c61b5a64433b27c1ab09dba94916ab77d8d; source linkage is established by upstream run53/artifact digest, nested release
asset digest and byte-identical tested bundle, not an independent rebuild. The
pinned MacKernelSDK and full source/licenses are included; external runtime
numeric DMA telemetry remains unavailable. See the source/identity map.
If laobamac does not work with your card/environment, report that instead of
changing guards or treating an unloaded/wrong driver as a completed test.

THE ROUTINE
One campaign already contains THREE rounds of FOURTEEN tests:
TCP TX P1, RX P1, TX P4, RX P4 — each 8 seconds;
then UDP TX/RX at 100,250,400,500,600 Mbps — each 5 seconds,1200-byte payloads;
2-second pauses, saved original client JSON/server output/intervals/commands;
then one separate online networkQuality test over the Wi-Fi interface.
No system_profiler SPAirPortDataType before/between rounds, no repeated ioreg
sampler/radio-status polling, and no concurrent Ookla or large background load.
Do not change network/radio settings in the middle of a campaign.
One complete campaign per version/boot configuration is the starting comparison.
Keep slow and failed attempts. Repeat anomalies or the preferred candidate only
as useful, and mark first-after-boot versus later repeats and any sleep/wake.
The 'wrong ZC19 actual DMA mode' text refers to the common health format used
by later versions; it does not itself mean ZC19 is loaded.

OOKLA AND ONE RETURN ZIP PER TEST
After each campaign, wait for the final networkQuality stage to finish. Then
run one Ookla Speedtest in the same candidate/configuration, preferably with
the same Ookla server throughout. Save its result/curve screenshot and result
URL. Keep LAN and Internet measurements distinct.

The runner prints the original result ZIP path, normally in:
~/Desktop/AirportItlwm/TESTS/

In Finder, select that original result ZIP and the Ookla screenshot together
(plus a short notes TXT if needed), choose Compress, and name the resulting
single archive clearly with candidate/configuration and _RETURN.zip.
Send ONLY that one outer ZIP for that campaign. No extra packaging program.
Do not edit the original runner ZIP or its internal checksum list.
If a test fails and Ookla cannot run, include the original failure ZIP and a
short explanation instead; do not discard the failure.

Please record exact card/computer/macOS build, OpenCore/OCLP/root-patch versions,
AppleVTD and DisableIoMapper, AP/channel/width, wired server/adapter and topology,
Ookla server/time/result link, first versus repeated workload, and any stalls,
connection loss or sleep/wake issues. A full system_profiler report is not needed.

SUBMIT BETA RESULTS — ONE CENTRAL THREAD
The GitHub results thread will be titled exactly:
AirportItlwm-Tahoe 1.1.0 Beta — Test results
URL: https://github.com/kgp-macPro/AirportItlwm-Tahoe/issues/2
Do NOT create a tester-specific results
issue. ALL testers add EACH campaign as a COMMENT in this ONE central thread.
Start every comment with card | candidate | AppleVTD setting | actual DMA route.
Separate issues are reserved for concrete bugs requiring independent investigation;
link their relevant central-thread comment/attachment instead of duplicating it.

For EACH campaign attach ONE Return-ZIP containing:
- The UNTOUCHED runner-result ZIP (do not unpack/repack it internally).
- Ookla screenshot/result URL and short configuration notes.
Keep failures and slow runs. If Ookla cannot run, include why. Keep archives as
issue/forum attachments, not repository/README changes or the entire beta ZIP.

COPYABLE CAMPAIGN COMMENT — replace placeholders, one campaign per comment
CARD | CANDIDATE | AppleVTD=on/off | DMA=conventional Mode1 / mapped Mode2 / unavailable (external declared)
System/CPU; macOS version/build; loaded UUID/hash or identity-check failure:
ACTUAL environment: OCLP version/root patches; Skywalk replacement/version; AMFIPass/-amfipassbeta; loaded wireless frameworks (unknown if not established):
AP/model/channel/width/topology; client Wi-Fi interface/IP:
Wired server OS/iperf3; adapter/chip/link/interface/IP:
Campaign date/time/timezone; first/repeat; prior traffic/sleep; stalls/failures:
Return-ZIP attachment (untouched runner ZIP + Ookla screenshot/result URL + notes):
Ookla server/time/link, or why unavailable; existing report link (no duplicate upload):
END COMMENT TEMPLATE

Actual DMA guidance: KGP AX201 stays conventional Mode1 even with system
AppleVTD active; AX210/AX211 use their qualified actual Mode1/Mode2 route.
laobamac53 mode labels are DECLARED AppleVTD configurations; unavailable KGP
numerical telemetry is not zero. Actual patch/framework state is a separate field.

IM/TM shared beta threads are EQUIVALENT alternatives; no GitHub account needed:
https://www.insanelymac.com/forum/topic/363194-airportitlwm-tahoe-100-%E2%80%93-intel-wi-fi-with-applevtdiommu-and-awdl-baseline-support-on-macos-tahoe/
https://www.tonymacx86.com/threads/airportitlwm-tahoe-1-0-0-intel-wi-fi-with-applevtd-iommu-and-awdl-baseline-support-on-macos-tahoe.333361/
The new shared beta-post permalinks will be added after KGP posts there. Use the same first
line, comment template and one-Return-ZIP rule. Link existing GitHub/IM/TM reports
instead of duplicating uploads. Add later campaigns as replies in that shared
thread. A forum account may be needed there; a GitHub account is not required.

HARDWARE AND EARLIER TESTERS
This runner remains focused on AX201/AX210/AX211; the new source-derived hardware
overview does not add a required matrix or mapped qualification. See
SOURCES/BETA_DOCUMENTATION/HARDWARE_COVERAGE.md for exact ZC28 source boundaries.
Earlier AX201 tests were by @netiave (TM) / @funcio (IM), one tester. Earlier AX211
tests were by Luiz (@LuizMeliga on TM); neither is claimed to have tested ZC28.
Use the r7 campaign separately from any support forensic capture/profiler; the
existing 1.0.0 forensic attachment is not a beta identity/health validator.
