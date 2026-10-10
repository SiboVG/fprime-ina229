# Requirements and verification

Datasheet references are to TI SLYS023A (Rev. A, May 2022). "Test" means an executed
host or generated F Prime test, "Inspection" means source or generated-code review, and
"HIL" means measurement on real hardware. No CubeSpec system requirement IDs have been
allocated to this library yet, so every parent below is "CubeSpec system requirement
TBD". The rationale column records why the requirement exists until that trace is available.

## Evidence recorded on 2026-10-10

Executed locally on macOS 27.0 (arm64) with Apple clang 21.0.0, the Command Line Tools 27
linker, and the default macOS 27 SDK, using F Prime `35d4c0e` (v4.2.0-267) and its native
fpp 3.3.0a12, installed by `tools/workspace/bootstrap-fprime-venv.sh`. The host tests and
the component tests used host CMake 4.4.4. The FT232H deployment was built with CMake 4.1.1:
with CMake 4.4.4 the executable links, but F Prime's post-build install step stops with
`file INSTALL given no DESTINATION` (F Prime sets the install prefix to `/`, which the
generated install script strips to an empty string). No SDK pin or Rosetta was used.

- 17 portable device tests and 3 POSIX transport tests pass
  ([host log](evidence/host-tests.txt)).
- 7 generated F Prime manager tests pass ([component log](evidence/manager-tests.txt)).
- 2 Python bridge tests and 2 generated-deployment end-to-end tests pass
  ([deployment log](evidence/deployment-tests.txt)).
- The host C++ tests pass under AddressSanitizer and UndefinedBehaviorSanitizer
  ([sanitizer log](evidence/sanitizers.txt)).
- Portable device coverage: **95.6% lines, 98.6% branches**
  ([coverage report](evidence/coverage.txt)). The one missed branch is the defensive SHUNT_CAL
  upper bound, which the shunt-drop check makes unreachable.
- The [static-analysis gate](#static-analysis) (cppcheck 2.22.0 and clang-tidy 22.1.8)
  reports zero findings in every production translation unit: the device core, the manager
  and all FT232H example sources ([static-analysis log](evidence/static-analysis.txt)).
- Strict compiler warnings are errors for authored C++ sources; generated framework
  sources retain their framework flags. The component and deployment builds emit no
  warnings.
- The documentation builds with strict MkDocs validation.
- `docs/evidence/source-sha256.json` pins every authored source, test and build file at
  the state these logs describe; `python3 tests/check_evidence.py` fails on any drift.

CI defines Linux and macOS host checks, Linux sanitizers/coverage, generated component
and example builds, static analysis, the evidence check and strict docs. Before the move to
F Prime `35d4c0e`, the first remote run to pass every job was [run 36331162640](https://github.com/SiboVG/fprime-ina229/actions/runs/36331162640) on commit
`4a762b2` (2026-09-27). The first attempt, [run 36330896032](https://github.com/SiboVG/fprime-ina229/actions/runs/36330896032), failed only the
GCC branch-coverage gate, which that commit fixed by excluding exception edges. CI logs
expire, so `docs/evidence/` remains the retained record. The tests require no USB device; they do not establish
sensor accuracy or physical timing. Coverage is scoped to authored portable device code,
not generated F Prime code or the whole deployment.

## Requirement-to-test map

| ID | Requirement | Rationale (parent: CubeSpec system requirement TBD) | Verification and evidence |
| --- | --- | --- | --- |
| INA-001 | Every register access shall send a 6-bit address, a 0 bit and a R/W bit (1 = read), followed by the register's exact width (16, 24 or 40 bits), MSB first. | SPI frame, section 7.5.1.1 and Table 7-3 | Test: `ProtocolWidthsAndByteOrder` |
| INA-002 | Initialization shall validate the configuration before any bus access. It shall then verify manufacturer ID 0x5449 and die ID 0x229, reset, stop conversion, and write and verify CONFIG, SHUNT_CAL, SHUNT_TEMPCO and ADC_CONFIG before reporting ready. | Unknown or misconfigured parts must never produce data | Test: `InitializeValidatesIdentityAndCalibrationBeforeMeasurement`, `InvalidConfigurationDoesNoIo`, `ConfigurationBoundariesAndPositiveNarrowScaling`, `RemainingValidationAndFaultBranches` |
| INA-003 | The core shall report SPI, delay and readback failures as distinct statuses and shall invalidate the device after any failed verified write. | Uncertain register state must not be measured | Test: `ConfigurationFaultsInvalidateState`, `InitializationFaultMatrixAndInvalidLifecycle`, `RemainingValidationAndFaultBranches`, manager `Failures`, `Commands` |
| INA-004 | Results shall be converted to SI units with the section 8.1.2 equations and the field encodings of Tables 7-9 to 7-15. | Correct engineering units | Test: `SignedMeasurementsAndScaling`, `ConfigurationBoundariesAndPositiveNarrowScaling` |
| INA-005 | A read shall return DEVICE_ERROR on a trim-memory checksum error or a set always-zero bit, NOT_READY before conversion completes, and SPI_ERROR on transfer failure. It shall leave the caller's output unchanged in each case. | Table 7-16; never present stale or foreign bus data as fresh | Test: `ReadFailuresPreserveOutputAndDoNotClaimFreshness`, `EveryAcquisitionTransferFailurePreservesOutput`, `ImplausibleBusDataIsRejected`, manager `Nominal`, `Failures` |
| INA-006 | The core shall support all 16 ADC modes and report validity for exactly the channels the mode produces. `trigger` shall be accepted only in an initialized triggered mode. | Table 7-6 | Test: `AdcModesValidityAndTriggering`, `RemainingValidationAndFaultBranches` |
| INA-007 | On MATHOF a read shall mark current and power invalid. On MATHOF, ENERGYOF or CHARGEOF it shall latch the affected accumulators invalid until a successful accumulator reset or initialization. Bus, shunt and temperature shall remain valid. | Table 7-16; an overflow must not blank unaffected channels or be forgotten when its flag self-clears | Test: `OverflowInvalidatesOnlyAffectedChannelsAndLatches`, `OverflowSeenBeforeTransferFailureStaysLatched`, `ResetAccumulatorsDrainsStaleOverflowFlags`, manager `OverflowDegradesAndRestores` |
| INA-008 | Alarm controls shall accept only defined threshold registers and encodings and shall verify every writable bit. DIAG_ALRT configuration shall change without altering MEMSTAT. | Tables 7-16 to 7-22; a configuration write must not fake or hide a checksum error | Test: `AccumulatorResetAndAlertControls`, `AlertConfigurationPreservesTrimStatusAndReadsSucceed`, manager `Commands`, `AlertConfigurationKeepsReadsValid` |
| INA-009 | The manager shall expose typed FPP ports, commands, events and telemetry, emit exactly one response per command, map invalid arguments to VALIDATION_ERROR, and count neither invalid arguments nor out-of-sequence commands as device faults. | Operator interface, workspace F Prime practices | Test: manager `Commands`, `ValidationErrorsAreNotDeviceFaults` |
| INA-010 | Under an intermittent fault the manager shall emit fault, recovery and degradation events only on status edges. It shall emit at most 10 of each between throttle clears, while counting every failure. | Bounded event rate; avoid event floods | Test: manager `IntermittentFaultsAreRateLimited`, `Failures`, `OverflowDegradesAndRestores` |
| INA-011 | The manager shall serialize all commands and port calls on one instance with the component mutex. | Command-dispatcher and scheduler threads share one device | Inspection: every command and input port in `Manager.fpp` is `guarded`; generated `ManagerComponentAc.cpp` locks in all 12 handler bases. Timing under load: INA-HIL-006 |
| INA-012 | The core shall use no dynamic allocation or recursion, only bounded loops, and transactions of at most six bytes. | Deterministic resource use | Inspection of `Device.cpp`; Test: sanitizer run; static analysis |
| INA-013 | FT232H support shall stay outside the reusable targets and carry only bytes, completion status and bounded socket I/O. | Bench tooling must not enter flight code | Test: `BridgeTransfersBytesAndPreservesOutputOnFailure`, `BridgeTimesOutAndDisconnects`, `BridgeValidatesBoundsAndHandlesClosedPeer`, `test_bridge.py` |
| INA-014 | The generated bench topology shall produce CSV samples and shall not emit a data row after a failed USB exchange. | End-to-end F Prime path | Test: `test_full_fprime_path`, `test_transport_fault_is_not_valid_data` |

The emulator in `tests/TestSupport.hpp` models the section 7.6.1 reset values and writable
masks, self-clearing RST/RSTACC, read-only result and ID registers, and the DIAG_ALRT
read/clear rules. It counts any write to a read-only or unlisted register, and the tests
assert that count stays zero. It does not simulate ADC integration, analogue inputs or
real-time energy accumulation. Timeouts and partial/closed socket responses are exercised
using real local sockets. The end-to-end server substitutes an emulated SPI port for
PyFtdi; the F Prime topology, C++ bridge and Python framing code are real.

## Hardware and target validation still required

| ID | Activity |
| --- | --- |
| INA-HIL-001 | Identify the board and shunt. Verify SPI idle level, clock edges, CS hold and byte alignment with a logic analyzer, starting at 1 MHz. Check the reset delay and readback on the real INA229, then repeat with the intended native flight SPI driver. |
| INA-HIL-002 | Compare bus, shunt, temperature and both current polarities against calibrated instruments in both ADC ranges. Verify calibration across the operating range. |
| INA-HIL-003 | Exercise triggered and continuous acquisition, disabled-channel validity, not-ready polling, alarm latching, accumulator reset and read-clear behavior. |
| INA-HIL-004 | Integrate known power and current over time and compare against energy and charge. Inject or simulate MATHOF, ENERGYOF and CHARGEOF, and confirm the latch and recovery. Confirm that the DIAG_ALRT read-modify-write leaves MEMSTAT unchanged on silicon. |
| INA-HIL-005 | Disconnect USB/SPI, remove sensor power, hold MISO high and low, and inject partial or failed transfers. Demonstrate no valid output on failure and bounded execution time. Confirm that reserved result bits read 0 on silicon. |
| INA-HIL-006 | Qualify the target toolchain and OS, component-mutex hold time against the scheduler deadline, shared-bus arbitration, stack usage, telemetry bandwidth, watchdog budget and application timestamps under load. |

These open activities prevent any claim of flight qualification or completed HIL.

The TE0715 sensor bench (ZephyrOSTe0715 repository, `docs/tests/ina229.md`) runs the
manager unchanged on a Zynq-7000 with Zephyr's SPI driver and has procedures for
INA-HIL-001 (waveforms, reset and readback), INA-HIL-002 (one operating point per run),
and parts of INA-HIL-003 to INA-HIL-006 (continuous acquisition, accumulator reset,
MEMSTAT after the alert read-modify-write, MISO and supply faults, read duration). An
activity closes only with a reviewed as-run record.

## Reproduce

Portable build commands are on the [overview](index.md). Generated manager build
commands are in [F Prime integration](fprime.md). Build the example using
[FT232H instructions](ft232h.md), then run:

```sh
INA229_DEMO="$PWD/build-ft232h/bin/$(uname)/Deployment" python3 tests/test_deployment.py
```

These tests bind a temporary local Unix socket. A restricted sandbox may require
permission for that local socket; no network or hardware connection is used.

```sh
cmake -S tests -B build-san -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-san
ctest --test-dir build-san --output-on-failure

cmake -S tests -B build-coverage -G Ninja -DINA229_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
python -m gcovr --root . --object-directory build-coverage \
  --filter 'Ina229/Device/' --txt --txt-metric branch --txt-summary \
  --exclude-throw-branches --exclude-unreachable-branches \
  --fail-under-line 90 --fail-under-branch 90
```

GCC counts an exception edge on every call that could throw; the library uses no
exceptions, so those edges are excluded (GCC 14 measures 89.1% without and 98.9% with
the exclusion; Apple Clang reports no such edges).

With Apple Clang, add `--gcov-executable "xcrun llvm-cov gcov"` to gcovr.
Remove old `.gcda` files from that build directory after changing coverage sources.
Install `gcovr==8.2` for the locally tested reporting version.

Run the [static-analysis gate](#static-analysis) with
`python3 scripts/static_analysis.py --report docs/evidence/static-analysis.txt`.

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs build --strict --clean
```

After rerunning the commands above and refreshing `docs/evidence/*.txt`, pin the sources
with `python3 tests/check_evidence.py --write`. Without `--write`, the script fails if
any authored file changed after the evidence was recorded.

## Static analysis

Every production C++ source is analysed by the CubeSpec static-analysis gate:
`Ina229/Device`, `Ina229/Manager` and the FT232H example (`examples/Ft232h`: socket
transport, bridge driver, client and deployment `Main.cpp`). Test code (`tests/`,
`Ina229/*/test/`) is verified by execution and is not analysed. F Prime framework sources
and generated `*Ac.cpp`/`*Ac.hpp` files are not analysed either.

`scripts/static_analysis.py` reads `static-analysis.toml`, which defines two builds:

| Build | Directory | Translation units | Needs |
| --- | --- | --- | --- |
| `host` | `build-static` | `Device.cpp`, `UnixSpi.cpp` | CMake, Ninja, a C++ compiler |
| `ft232h` | `build-ft232h` | `Device.cpp`, `Manager.cpp` and every `examples/Ft232h` source | The F Prime tool chain for `35d4c0e` ([F Prime integration](fprime.md#f-prime-tool-chain)) |

The script configures each build and exports its compilation database. For `ft232h` it
also builds the `Deployment_Top` target, so the generated headers exist. It then keeps only
the first-party entries and runs both analysers on them. It prints the tool versions, the
translation units and every finding, and exits non-zero on any finding. The `ft232h` build
uses `FPRIME_FRAMEWORK_PATH` (default: the workspace's `third-party/upstream/fprime`). It
activates a repository-local `.venv-fprime` when one exists.

Rule sets and pinned versions:

- **cppcheck 2.22.0**: `--enable=warning,style,performance,portability`, exhaustive checking,
  the POSIX library, and inline suppressions. Every reported severity, including style, fails
  the gate.
- **clang-tidy 22.1.8** (the PyPI wheel): `.clang-tidy` enables the `clang-analyzer`,
  `bugprone`, `cert`, `misc`, `performance` and `portability` families with all warnings as
  errors. The file lists the few disabled checks and why. Only headers below `Ina229/` or
  `examples/` are reported.
- `scripts/static_analysis.py`, `scripts/install-cppcheck.sh`, `scripts/clang-tidy` and
  `.clang-tidy` are byte-identical copies of the workspace reference in
  `tools/static-analysis/`; only `HeaderFilterRegex` may differ. The workspace's
  `tools/static-analysis/check_drift.py` fails on drift. Only `static-analysis.toml` is
  specific to this repository. On CI (`CI` set) the script fails unless the pinned versions
  run; locally it warns.

Suppression policy: fix the code. A suppression is allowed only inline, on the finding's line
or the line before, and must name the check and give a reason, for example
`// NOLINT(bugprone-xyz): reason` or `// cppcheck-suppress id ; reason`. File-level or
blanket suppressions are not allowed. There are currently no inline suppressions. The script
counts cppcheck findings only in first-party files. Findings in the build tree (generated
`*Ac.hpp`) and in an F Prime checkout outside the repository are reported as not counted.
`suppress_paths` in `static-analysis.toml` lists only `fprime/`, where CI checks F Prime
out inside the repository. Analysis-integrity errors, such as a failing `#error`, count
wherever they occur. To pass F Prime's compiler checks, `static-analysis.toml` defines
`__GNUC__` and `__SIZEOF_POINTER__` for cppcheck. A finding that reveals a real defect is fixed as a
behaviour change with its own test, not suppressed.

Run it:

```sh
python3 scripts/static_analysis.py                   # every build (zero findings expected)
python3 scripts/static_analysis.py --build host      # portable build only; no F Prime needed
python3 scripts/static_analysis.py --no-configure    # reuse existing compile databases
python3 scripts/static_analysis.py --report docs/evidence/static-analysis.txt
```

Tools: `uvx` (from uv) fetches the pinned clang-tidy automatically, or use
`pipx install clang-tidy==22.1.8`. Install cppcheck 2.22.0 with Homebrew, or build it with
`sh scripts/install-cppcheck.sh` and set `CPPCHECK=$HOME/.local/cppcheck-2.22.0/bin/cppcheck`.
CI runs the same script in the `static-analysis` job.

In VS Code, the recommended C/C++ extension (`ms-vscode.cpptools`) uses
`build-ft232h/compile_commands.json` and runs the pinned clang-tidy through
`scripts/clang-tidy` as you edit (`.vscode/settings.json`). The task **Static analysis
(cppcheck + clang-tidy)** (*Terminal > Run Task*) runs the full gate and lists findings in
the Problems panel. Run the gate once first, so that the `build-ft232h` compilation
database exists.

## Workspace registration

The parent FlightSoftware workspace intentionally has no Git repository. Its
maintained-project table has been updated; the exact change is preserved in this
library's [workspace registration patch](workspace-registration.patch).
Apply from the FlightSoftware directory with `patch -p1 < libraries/fprime-ina229/docs/workspace-registration.patch`
when registering a fresh copy. Do not reapply to the already updated workspace.
