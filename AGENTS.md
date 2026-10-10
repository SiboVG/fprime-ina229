# INA229 contributor guidance

Before changing this library, read `../../docs/engineering-practices/fprime.md`, which holds
the workspace-wide reusable flight-software practices distilled from the 2024 JPL workshop.
Also read `docs/verification.md` for the current requirements, evidence and open
hardware work.

Keep `docs/verification.md` synchronized with behavior and tests. Add or update
requirement-to-test evidence whenever an interface, command, event, register access,
failure response or timing assumption changes. Cite the datasheet section or table
(TI SLYS023A) for any behavior that depends on the device.

Preserve the manager/device/driver separation and the hardware-independent
`Ina229::Device` core. The core must stay fixed-capacity, bounded and free of heap
allocation, locks and scheduling. The manager's commands and ports stay `guarded`;
long-running work belongs in the application, not in the manager.

Create the tool chain for the pinned F Prime revision (`35d4c0e`) with `../../tools/workspace/bootstrap-fprime-venv.sh`
or the equivalent commands in `docs/fprime.md`; do not pin a macOS SDK or rely on Rosetta.
Before handing off a change, run the host tests, sanitizers, coverage gate, the generated
manager test executable, the FT232H deployment test, the static-analysis gate
(`python3 scripts/static_analysis.py`, which must report zero findings) and a strict MkDocs
build. Then refresh `docs/evidence/` (the static-analysis log with
`python3 scripts/static_analysis.py --report docs/evidence/static-analysis.txt`) and run
`python3 tests/check_evidence.py --write`. Bus timing, accuracy or electrical claims
require bench or flight-target HIL evidence; do not infer them from emulator tests.

Fix static-analysis findings in the code. Suppress one only when it is a false positive or
the check does not apply, and then only inline: on the finding's line or the line before,
naming the check and giving a reason (`// NOLINT(check-id): reason` or
`// cppcheck-suppress id ; reason`). Never add file-level or blanket suppressions, and never
disable a check in `static-analysis.toml` or `.clang-tidy`. `scripts/static_analysis.py`,
`scripts/install-cppcheck.sh`, `scripts/clang-tidy` and `.clang-tidy` are copies of the
workspace reference in `tools/static-analysis/`. Change them there, not here. If a finding
reveals a real defect whose fix changes behavior, treat the fix as a behavior change: add a
test and update `docs/verification.md`.

## Test-first, from the requirement

Develop test-first, and derive tests from what the code must do, not from how it does it:

1. Add or update the requirement (ID, `shall` statement, rationale or parent, method) in this repository's requirements matrix.
2. Design test cases from the requirement and the interface contract (datasheet, ICD, FPP model) without reading the implementation: nominal values computed independently, boundaries, invalid inputs, injected dependency faults, state transitions.
3. Watch the tests fail, implement the minimum to make them pass, then refactor.
4. When coverage shows unexecuted code, add a requirement-derived test, remove dead code, add the missing requirement, or justify the code by analysis. Never add a test only to execute a line.
5. Name the requirement ID in each test, and list the test in the requirement's row.

Coverage target (ECSS category C, CubeSpec): 100% statement coverage, tested or justified, and at least 80% decision coverage. The full procedure is in the CubeSpec workspace `AGENTS.md` (`../../AGENTS.md` in the workspace layout).
