# Frozen ZC28 comparison context

This is the historical motivation and source-derived hardware overview for frozen ZC28, not BE200 qualification. This beta is now an optional reference comparison; current Alpha is primary. Laobamac controls are AX201/AX210/AX211-only and do not support BE200. Current tool commands are in [the shared guide](../benchmark/README.md).

### Why this comparison beta

Earlier strong reports from **AX201 tester Fulvio ([@netiave on tonymacx86](https://www.tonymacx86.com/members/netiave.1548699/) / [@Funzio on InsanelyMac](https://www.insanelymac.com/forum/profile/2547924-funzio/) / [@netiave on GitHub](https://github.com/netiave))** and **AX211 tester Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/luizmeliga.1658846/) / [@luizmeliga on GitHub](https://github.com/luizmeliga))** motivated the attempt to combine fast TCP TX with roughly 600 Mbps TCP RX on KGP's AX210. Their earlier reports are valuable historical evidence, **not ZC28 qualification**.

TX-command/memory work brought normal AX210 TCP TX to roughly 480–490 Mbps; later work addressed publication, RX ownership, queues and lifecycle safety. A System Profiler interaction affected TX measurements, and replacing the wired M1 peer's Realtek RTL8156 adapter with ASIX AX88179B substantially changed RX results. Those cohorts stay separate; neither finding proves a universal card/adapter defect. ZC28 builds on ZC26, grouping software-capacity wakes during IWX TX service while retaining hardware-completion wakes. Useful progress and remaining slow/lossy states are both documented.

This is a deliberate pause in local optimisation to gather independent system/AP/network comparisons. The [full campaign narrative](FORUM_IM_TM_DRAFT.txt), with [IM](FORUM_IM_DRAFT.txt) and [TM](FORUM_TM_DRAFT.txt) variants, uses selected coherent three-round campaign means. The tables below instead show historical best **individual whole-test** results; neither presentation combines different attempts into a claimed stable configuration.

### Hardware coverage — qualification is narrower than source matching

| Group | Current evidence and scope |
| --- | --- |
| AX210 / 8086:2725 | ZC28 physically tested in actual mapped Mode2 and conventional Mode1; frozen 1.0.0 has its separate AX210 qualification. Repeat/long-run/sleep-wake limits remain |
| AX201; AX211 | Earlier results from AX201 tester Fulvio ([@netiave on tonymacx86](https://www.tonymacx86.com/members/netiave.1548699/) / [@Funzio on InsanelyMac](https://www.insanelymac.com/forum/profile/2547924-funzio/) / [@netiave on GitHub](https://github.com/netiave)) and AX211 tester Luiz ([@LuizMeliga on tonymacx86](https://www.tonymacx86.com/members/luizmeliga.1658846/) / [@luizmeliga on GitHub](https://github.com/luizmeliga)) motivated this beta. Do not assign a DMA route from the AX201 label; verify the hardware identity and actual reported mode. Luiz's historical AppleVTD-active result is not positive proof of numerical Mode2. Neither is yet ZC28-qualified |
| Additional IWX source candidates | AX200, selected AX101/AX203/AX411 and named Killer 1650/1675/1690 variants; selected AC 9461/9462/9560/Killer 1550 variants. Actual PCI/MAC/RF/subdevice selection and enclosed firmware are required; these are **awaiting physical confirmation**, not blanket mapped support |
| Separate upstream backend paths | IWM: 3160/3165/3168, 7260/7265, 8260/8265, 9260 and selected 9461/9462/9560. IWN: 4965,5100/5150/5300/5350,100/105/130/135/1000/1030,2200/2230,6005/6030/6050/6150/6200/6235/6300. They do not enter the IWX-specific service/DMA paths; no same-fix or new qualification claim |
| Table-only / excluded or future | AX221/AX231 and other MA/SnJ/QnJ entries can select firmware absent from this frozen package; table presence is not usable support. BE200 is outside frozen ZC28 and is developed separately in the BE200 alpha |

The [source-grounded hardware overview](HARDWARE_COVERAGE.md) and [exact 208-row source ledger](HARDWARE_SOURCE_LEDGER.json) trace frozen ZC28, not the repository root's 1.0.0 code. Some changes are shared across IWX, others depend on family/mode; not every upstream card received the same implementation. This list **does not expand the candidate qualification or DMA admission** or silently authorise another DMA route.
