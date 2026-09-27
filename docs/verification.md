# Requirements and verification

Datasheet references are to TI SLYS023A (Rev. A, May 2022). "Test" means an executed
host or generated F Prime test, "Inspection" means source or generated-code review, and
"HIL" means measurement on real hardware. No CubeSpec system requirement IDs have been
allocated to this library yet, so every parent below is "CubeSpec system requirement
TBD". The rationale column records why the requirement exists until that trace is available.

## Evidence recorded on 2026-09-27

Executed locally on macOS 27.0 (arm64) with Apple clang 21.0.0, the Command Line Tools 27
linker, and the default macOS 27 SDK, using F Prime 3.5.1 and fpp 2.2.1 from its
platform-independent (JVM) wheels, installed by `tools/workspace/bootstrap-fprime-venv.sh`.
No SDK pin or Rosetta was used for any build. Only the static analysis used older SDK
headers, because Homebrew LLVM 18 `clang-tidy` cannot parse the macOS 27 SDK.

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
- `clang-tidy` (clang-analyzer checks, warnings as errors) reports no findings in
  `Device.cpp` or `Manager.cpp` ([static-analysis log](evidence/static-analysis.txt)).
- Strict compiler warnings are errors for authored C++ sources; generated framework
  sources retain their framework flags. The macOS F Prime link emits duplicate-library
  warnings from generated target dependencies.
- The documentation builds with strict MkDocs validation.
- `docs/evidence/source-sha256.json` pins every authored source, test and build file at
  the state these logs describe; `python3 tests/check_evidence.py` fails on any drift.

CI defines Linux and macOS host checks, Linux sanitizers/coverage, generated component
and example builds, static analysis, the evidence check and strict docs. Its remote
execution has not been observed. The tests require no USB device; they do not establish
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

Static analysis uses the component build's compilation database:

```sh
cmake -S tests/fprime-consumer -B build-components -G Ninja \
  -DFPRIME_FRAMEWORK_PATH=/absolute/path/to/fprime -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-components --target Ina229_Manager_ut_exe
clang-tidy -p build-components --warnings-as-errors='clang-analyzer-*' \
  Ina229/Device/Device.cpp Ina229/Manager/Manager.cpp
```

Use a `clang-tidy` at least as new as the platform SDK (on macOS, a current Homebrew
`llvm`). An older one fails while parsing system headers: Homebrew LLVM 18, for example,
cannot parse the macOS 27 SDK even when pointed at its own libc++. Linux CI uses the
distribution's `clang-tidy`.

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs build --strict --clean
```

After rerunning the commands above and refreshing `docs/evidence/*.txt`, pin the sources
with `python3 tests/check_evidence.py --write`. Without `--write`, the script fails if
any authored file changed after the evidence was recorded.

## Workspace registration

The parent FlightSoftware workspace intentionally has no Git repository. Its
maintained-project table has been updated; the exact change is preserved in this
library's [workspace registration patch](workspace-registration.patch).
Apply from the FlightSoftware directory with `patch -p1 < libraries/fprime-ina229/docs/workspace-registration.patch`
when registering a fresh copy. Do not reapply to the already updated workspace.
