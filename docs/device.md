# Device API

Construct `Device(SpiBus&, Delay&)` with objects that outlive the device. Construction
performs no I/O; `initialize(Configuration)` validates all inputs before bus access,
checks TI/device IDs, resets, waits 2 ms, confirms shutdown, writes and verifies
calibration/configuration, then starts the selected ADC mode. State becomes valid
only after all steps pass. A failed register write/readback invalidates state.

`shuntOhms` and `maxCurrent` must be finite and positive; the expected shunt drop
must fit the selected range and the rounded SHUNT_CAL must fit 1..32767. Default
configuration is 10 milliohms, 8 A, wide range, continuous all-channel conversion,
50 microseconds per channel and no averaging. These are examples, not mission defaults.
Reinitialization resets device registers, alarm configuration and accumulators.

`configureAdc` accepts the ADC_CONFIG field encodings: mode 0..15, conversion times
0..7 (50, 84, 150, 280, 540, 1052, 2074, 4120 microseconds), averaging 0..7
(1, 4, 16, 64, 128, 256, 512, 1024). Modes 0 and 8 are shutdown; 1..7 are
triggered and 9..15 continuous. Low three mode bits select bus/shunt/temperature.
`trigger()` restarts only an initialized triggered mode; `stop()` selects shutdown.

`read(Measurement&)` checks state, trim-memory health, numeric overflow and conversion
ready before reading seven registers. It returns NOT_READY rather than waiting.
Output is replaced only after all reads succeed. The seven register reads are
sequential, not an atomic multi-register snapshot; continuous conversion may update
between reads. For coherent voltage/current/temperature measurements, use triggered
all-channel conversion and serialize all access. Energy/charge require continuous
operation and must not be used when their validity bits are clear.

Validity bits: 0 bus, 1 shunt, 2 temperature, 3 current, 4 power, 5 energy,
6 charge. Values in disabled channels may be stale. Current requires shunt;
power/energy require bus and shunt. Energy/charge require continuous operation.

`resetAccumulators()` issues a reset and verifies retained configuration, not that
continuously accumulating values remain zero. `setAlertConfig` accepts only bits
15..12 of DIAG_ALRT. `setThreshold` accepts raw 16-bit threshold registers 0x0C..0x11;
bus thresholds must fit 15 bits. Signed limits use two's-complement encodings.

`readRegister` offers bounded diagnostic access to implemented register addresses.
Reading DIAG_ALRT can clear ready/latched status; reading ENERGY/CHARGE clears their
overflow indications. Diagnostic reads must be serialized with acquisition. Overflow
is returned as NUMERIC_OVERFLOW; application policy decides when to reset accumulators.

The core has no locks, allocation, retries or scheduling. Maximum SPI transaction is
six bytes; one sample performs eight bounded transfers. The platform implements a
bounded transaction timeout, keeps CS asserted across command plus data, and reports
short/error transfers as failures. Delay failure is propagated explicitly.
