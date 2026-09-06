# Requirements and verification

## Evidence recorded on 2026-09-06

Executed locally on Darwin with Apple Clang and F Prime 3.5.1:

- 11 portable device scenario groups and 3 POSIX transport scenario groups pass.
- 3 generated F Prime manager tests pass, including command responses, data output,
  fault/recovery events, short transfers, telemetry and failed reinitialization.
- 2 Python bridge tests pass; 2 generated-deployment end-to-end tests pass with a
  local SPI register emulator, including injected USB errors.
- Host C++ tests pass under AddressSanitizer and UndefinedBehaviorSanitizer.
- Portable device coverage: **94.5% lines and 93.0% branches** (gcovr 8.2, Apple
  Clang; includes device-header inline/default initialization in the denominator).
- Strict compiler warnings are errors for authored C++ sources; generated framework
  sources retain their framework flags. The macOS F Prime link emits duplicate-library
  warnings from generated target dependencies.
- Documentation is built with strict MkDocs validation.

CI defines Linux and macOS host checks, Linux sanitizers/coverage, generated component
and example builds, and strict docs. Its remote execution has not been observed in
this run. The tests require no USB device; they do not establish sensor accuracy or
physical timing. Coverage is scoped to authored portable device code, not generated
F Prime code or the whole deployment.

## Requirement-to-test map

| ID | Requirement | Executable evidence |
| --- | --- | --- |
| INA-001 | Framing uses exact 16/24/40-bit widths, big-endian bytes and correct read bit | `ProtocolWidthsAndByteOrder` |
| INA-002 | Validate finite calibration and fields before I/O; identify, reset, stop, calibrate and verify before ready | `InitializeValidatesIdentityAndCalibrationBeforeMeasurement`, `InvalidConfigurationDoesNoIo`, `ConfigurationBoundariesAndPositiveNarrowScaling` |
| INA-003 | Propagate SPI/delay/readback failures and invalidate uncertain configuration | `ConfigurationFaultsInvalidateState`, `InitializationFaultMatrixAndInvalidLifecycle`, manager `Failures`/`Commands` |
| INA-004 | Convert signed 20/16/40-bit fields and unsigned power/energy into SI units | `SignedMeasurementsAndScaling`, `ConfigurationBoundariesAndPositiveNarrowScaling` |
| INA-005 | Reject invalid state, not-ready, bad trim memory and numeric overflow; preserve caller output on failure | `ReadFailuresPreserveOutputAndDoNotClaimFreshness`, `EveryAcquisitionTransferFailurePreservesOutput`, manager `Nominal`/`Failures` |
| INA-006 | Support all ADC modes with explicit channel validity, trigger and shutdown | `AdcModesValidityAndTriggering` |
| INA-007 | Support checked alarm thresholds/configuration and accumulator reset | `AccumulatorResetAndAlertControls`, manager `Commands` |
| INA-008 | Expose typed data/status and one command response; suppress repeated fault events and report recovery | manager `Nominal`, `Failures`, `Commands` |
| INA-009 | Keep FT232H outside reusable targets; carry bytes, completion status and bounded socket I/O | `BridgeTransfersBytesAndPreservesOutputOnFailure`, `BridgeTimesOutAndDisconnects`, `BridgeValidatesBoundsAndHandlesClosedPeer`, `test_bridge.py` |
| INA-010 | Generated topology produces CSV and rejects failed USB exchange | `test_full_fprime_path`, `test_transport_fault_is_not_valid_data` |

Emulators model ID/reset/configuration/byte-width behavior and selected read-clear
side effects. They do not simulate ADC integration, analogue inputs or real-time
energy accumulation. Timeouts and partial/closed socket responses are exercised
using real local sockets. The end-to-end server substitutes an emulated SPI port for
PyFtdi; the F Prime topology, C++ bridge and Python framing code are real.

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
  --filter 'Ina229/Device/' --txt --txt-summary
```

With Apple Clang, add `--gcov-executable "xcrun llvm-cov gcov"` to gcovr.
Remove old `.gcda` files from that build directory after changing coverage sources.
Install `gcovr==8.2` for the locally tested reporting version.

```sh
python -m pip install -r requirements-docs.txt
python -m mkdocs build --strict --clean
```

## Hardware and target validation still required

1. Identify the board/shunt and verify SPI idle level, clock edges, CS hold and byte
   alignment with a logic analyzer, starting at 1 MHz. Check reset delay and readback
   on the real INA229; repeat with the intended native flight SPI driver.
2. Compare bus, shunt, temperature and both current polarities against calibrated
   instruments in both ADC ranges. Verify calibration across the operating range.
3. Exercise triggered and continuous acquisition, disabled-channel validity,
   not-ready polling, alarm latching, accumulator reset and read-clear behavior.
4. Integrate known power/current over time and compare energy/charge; inject or
   simulate math/energy/charge overflow and confirm recovery policy.
5. Disconnect USB/SPI, remove sensor power, and inject partial/failed transfers;
   demonstrate no valid output on failure and bounded target execution time.
6. Qualify target toolchain/OS, serialized calls, shared-bus arbitration, stack usage,
   telemetry bandwidth, watchdog budget and application timestamps under load.

These open activities prevent any claim of flight qualification or completed HIL.

## Workspace registration

The parent FlightSoftware workspace intentionally has no Git repository. Its
maintained-project table has been updated; the exact change is preserved in this
library's [workspace registration patch](workspace-registration.patch).
Apply from the FlightSoftware directory with `patch -p1 < libraries/fprime-ina229/docs/workspace-registration.patch`
when registering a fresh copy. Do not reapply to the already updated workspace.

The [saved test logs](evidence/host-tests.txt), [component results](evidence/manager-tests.txt),
[sanitizer results](evidence/sanitizers.txt), [coverage report](evidence/coverage.txt),
and [deployment results](evidence/deployment-tests.txt) retain the local observations.
