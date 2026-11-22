# English text follow-up for the next regular release

Use the [maintained English checker](https://github.com/kgp-macPro/AirportItlwm-Tahoe/blob/airdrop-beta/tools/bluetooth-intel/CHECK_LOADED.py)
when preparing the next Bluetooth or AirDrop package. Check that its
`BUILD_IDENTITY.json` matches the components in that package.

| Published asset | Text to replace in future packages |
| --- | --- |
| BlueToolFixup 2.7.6 hotfix ZIP | German status and error strings in `CHECK_LOADED.py`. |
| 2.1.0 Beta 1 RELEASE.zip and SOURCE.zip | The bundled `CHECK_LOADED.py` and the recorder prompts, progress and error messages in `R49_CAPTURE.py`. Use a translated recorder if it ships again. |
| 2.1.0 Beta 1 SOURCE.zip and BlueToolFixup 2.7.6 hotfix SOURCE.zip | German IPv4 validation messages in copied benchmark runner sources. Use the maintained sources for any newly packaged runners. |
| 2.0.0 Benchmark Tools r13/r14 ZIPs | German IPv4 validation message in the six supplied runner scripts. |
| 1.2.0 BE200 Alpha 2 and 1.1.0 Beta 1 ZIPs | German IPv4 validation in the archived standalone runner. Use the maintained runner for future packages. |

Keep original captured results, executed runner snapshots and historical
reference archives as provenance. For a new package, review help, prompts,
status and failure output before generating its own manifest.
