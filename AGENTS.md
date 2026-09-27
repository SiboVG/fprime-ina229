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
manager test executable, the FT232H deployment test, `clang-tidy`, and a strict MkDocs
build. Then refresh `docs/evidence/` and run `python3 tests/check_evidence.py --write`. Bus
timing, accuracy or electrical claims require bench or flight-target HIL evidence; do not
infer them from emulator tests.
