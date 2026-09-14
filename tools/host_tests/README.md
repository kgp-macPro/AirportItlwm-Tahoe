# Retained host regression models

These are the source-derived ownership/reset/mapping/command/drain models used during the accepted PC1 and 1.0.0 reviews. Only fixture file paths have been made relative to each test's disposable working directory. They compile as user-space test programs; no kext is built or loaded by the test runner.

PC1 covers checked-terminal recovery, stale-completion collision controls, admission and modeled concurrency. POWER_CYCLE preserves the earlier ownership/retirement controls; GATE_DRAIN covers contended scheduling and teardown fencing. IOKit, device memory and kernel scheduling are mocked. Assertions establish model behavior, not hardware terminality or exhaustive kernel race safety.

Run `python3 tools/run_host_tests.py` from the repository root. The runner checks frozen driver source first so these retained models are not silently presented as validation of changed runtime code. Future functional changes require both model/source review and new qualification. Models are project test tooling under the repository license; inherited notices in the modeled source remain authoritative.
