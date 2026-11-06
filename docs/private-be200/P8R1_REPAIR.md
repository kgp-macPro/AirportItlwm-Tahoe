# Private P8R1 — TX completion repair (not yet hardware validated)

The original P8 is physically regressed: associated/IPv4 present, but no usable
Wi-Fi traffic. Loaded UUID53925674-06E8-3F06-AAD6-DF6A9A58B2BD and authentic
BE200/Mode2 binding are verified. The first saved fault is TX-response region23,
Darwin EPROTO100 at uptime149.002s in the22:31:15 boot. Later recovery/rejoin
reaches the same region again. Recorded transport generation/reset5/3 differs
from first-fault1/1; these are separate lifetimes in the same boot. The22:24:56
pre-reboot wrong-P7 attempt is separate. The22:34:43 P8 attempt also stopped
at INITIAL with zero benchmarks and actual region23; do not conflate these.

## Source-proven defect

`iwx_rx_tx_cmd` rejected idx==tail as fatal. Equal scheduler boundary is valid
no-progress, including a TX status after compressed BA already reclaimed that
boundary. Linux v6.9 a38297e3fb012ddfa7ce0321a7e5a8daeb1872b6,
drivers/net/wireless/intel/iwlwifi/mvm/tx.c:1675–1711 reclaims up to SSN;
its transport accepts empty reclaim. BA no-progress is also explicit at2140.
No-progress is NOT positive new ownership release. P8R1 returns without reading
idx-1, updating the node, retiring/freeing any backing or signaling output.
Required payload/QID/ownership/bounds/positive-progress checks remain. Invalid
forward/old-window progress still fails closed and retains backing until the
existing positive stop/reset proof. No QoS rollback, fake completion or guard
disablement is used. Firmware/SDK/DMA qualification and P7 recovery remain.

This defect existed latently in P7; P8 made per-TID QoS/firmware BA traffic
reachable. Saved P8 counters prove1822 QoS/TID publications and341 BA updates,
not one-to-one packets or successful over-air receipt. The exact rejected P8
wire packet/guard subreason was not retained, so no claim that this is uniquely
proven to initiate the observed fault. New bounded event15 records which guard
and queue/index/tail/queued facts if a real invalid response recurs. No per-packet
printing/allocation/registry publication. Counter13 is named no-progress only
for P8R1; old P8/P7 reserved fields stay reserved. No observer-added packet data.

## Scope and delivery

Only BZ runtime completion behavior changes; existing AX data paths retain the
P8 RX scheduling/correctness changes and conventional/mapped contracts. Same
new binary identifies BZ asPRIVATE-P8R1, other admitted IWX asprivateZC30.
AX201/AX210/AX211 and BE200 remain active1.2.0 targets. Find My is still open
and separate, not claimed repaired. Source branch/worktree and build identity
are recorded alongside delivery. KGP explicitly authorized replacing the old
Desktop BE200-EXPERIMENTAL-P8 for THIS repair; old P8+notes and originals are
preserved in the new evidence, P7 directory is untouched. No P9 delivery.
