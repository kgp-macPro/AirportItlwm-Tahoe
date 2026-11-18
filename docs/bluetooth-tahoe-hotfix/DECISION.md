# Tahoe discovery hotfix — implementation decision, 2026-11-18

The running system is 26.7.3 (25G342). The original R49 WLAN UUID, BlueToolFixup
2.7.5, and stock IntelBluetoothFirmware/IntelBTPatcher 2.5.3 UUIDs match the
published beta. `-kgpble` is present; `-btlfxoff` is absent. All eight additional
native edits are absent from the current readable bluetoothd. The production
2.7.5 classifier rejects CoreInit. Startup logs do not retain its original
diagnostic; this is not evidence of a different initialization failure.

The stored native images from 25F71 (26.5), 25G342 (26.7.3), and 25G416 (the
mounted Tahoe beta, whose plist also says 26.7.3) have identical complete
instruction layouts for the four edited functions, the two setters, and the
event-mask consumer, after normalizing independently disassembled PC-relative
address operands. Internal branches, ordinary immediates, vtable slots, opcodes,
and non-relative field offsets remain identical. The original 25G332 contracts
also match the 25G416 functions byte-for-byte. This does not qualify other Apple
code, controller firmware, RF operation, or every future OS release.

Chosen change: BlueToolFixup 2.7.6 retains the same eight instruction/data edits,
Intel USB selection, native initialization, and page-validation hook. Replace
the absolute function hashes with hashes of relocation-normalized instruction
layouts for the seven functions actually needed by these edits and their
operand graph. A bounded temporary workloop buffer is used, never a kernel
stack allocation or work in the page hook. Keep all original call/global
relationships, signed page checks, vnode/CDHash binding and page-level atomicity.
Preserve each binary's actual relative operands when constructing edits.

Do not add OS build numbers or Wi-Fi model allowlists. The unchanged Intel USB
selector is common to BE200, AX210, AX211 and AX201 and other matching Intel
Bluetooth controllers. This is not new hardware qualification for those cards.
An unknown instruction layout still skips the extra patches safely.

Unchanged: frozen R49 WLAN binary and its GO/PHY/filter/queue/channel behavior,
stock firmware loader and patcher, EFI, NVRAM and all installed files. No HCI
preflight or FirmwareLoaded gate is reintroduced. No 26.7.4 RC installation is
needed to investigate this confirmed local compatibility defect.

Validation will distinguish: disassembly establishes relative operands and
unchanged instruction layouts; production-classifier tests establish selection,
operand relationships, unchanged edit effects and negative integrity handling;
the final Kext build establishes compiled inputs and identity. Only a manual
boot with the new Kext can establish native patch activation and restored
MBP/iPhone discovery. No universal firmware/REA PASS will be claimed.
