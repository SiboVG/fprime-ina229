# Device API

Section, table and register references are to the TI INA229 datasheet SLYS023A
(Rev. A, May 2022).

Construct `Device(SpiBus&, Delay&)` with objects that outlive the device. Construction
performs no I/O; `initialize(Configuration)` validates all inputs before bus access,
checks TI/device IDs, resets, waits 2 ms, confirms shutdown, writes and verifies
calibration/configuration, then starts the selected ADC mode. State becomes valid
only after all steps pass. A failed register write/readback invalidates state.
The 2 ms wait exceeds the 300 µs start-up time in the electrical characteristics.

`shuntOhms` and `maxCurrent` must be finite and positive; the expected shunt drop
must fit the selected range and the rounded SHUNT_CAL must fit 1..32767 (section 8.1.2).
The drop limit already caps SHUNT_CAL at 4096, so the upper bound is defensive. Default
configuration is 10 milliohms, 8 A, wide range, continuous all-channel conversion,
50 microseconds per channel and no averaging. These are examples, not mission defaults.
Reinitialization resets device registers, alarm configuration and accumulators.

`configureAdc` accepts the ADC_CONFIG field encodings (Table 7-6): mode 0..15, conversion
times 0..7 (50, 84, 150, 280, 540, 1052, 2074, 4120 microseconds), averaging 0..7
(1, 4, 16, 64, 128, 256, 512, 1024). Modes 0 and 8 are shutdown; 1..7 are
triggered and 9..15 continuous. Low three mode bits select bus/shunt/temperature.
`trigger()` restarts only an initialized triggered mode; `stop()` selects shutdown.

## Reading

`read(Measurement&)` checks state, then reads DIAG_ALRT and seven result registers. It
returns NOT_READY rather than waiting, and replaces its output only after all reads
succeed. The seven register reads are sequential, not an atomic multi-register
snapshot; continuous conversion may update between reads. For coherent
voltage/current/temperature measurements, use triggered all-channel conversion and
serialize all access.

Whole-read rejections:

| Condition | Status | Basis |
| --- | --- | --- |
| Not initialized, or mode selects no channel | INVALID_STATE | Table 7-6 |
| DIAG_ALRT MEMSTAT = 0 (trim-memory checksum error) | DEVICE_ERROR | Table 7-16 |
| DIAG_ALRT bit 8, or VSHUNT/VBUS/CURRENT bits 3..0, set | DEVICE_ERROR | Tables 7-9, 7-10, 7-12, 7-16 ("always reads 0") |
| CNVRF = 0 | NOT_READY | Table 7-16 |
| Any transfer failure | SPI_ERROR | transport contract |

The reserved-bit checks catch a bus that does not return device data, such as MISO
stuck high, which a status-bearing transport still reports as a completed transfer.

## Channel validity

Validity bits: 0 bus, 1 shunt, 2 temperature, 3 current, 4 power, 5 energy, 6 charge.
`expectedValidMask()` gives the bits the current mode produces: current requires shunt;
power requires bus and shunt; energy and charge also require continuous operation.
Values in cleared channels may be stale or untrustworthy and must not be used.

Arithmetic and accumulator overflow degrade only the affected channels (Table 7-16):

| Flag | This read | Latched |
| --- | --- | --- |
| MATHOF | current and power invalid | energy and charge |
| ENERGYOF | — | energy |
| CHARGEOF | — | charge |

ENERGYOF and CHARGEOF clear when ENERGY and CHARGE are read, and MATHOF persists until a
new conversion or RSTACC. The core therefore latches accumulator invalidity as soon as it
reads the flag, even if a later transfer in the same read fails. The latch is visible
through `invalidAccumulators()` and clears only after a successful `resetAccumulators()`
or `initialize()`. Bus voltage, shunt voltage and temperature stay available throughout.

## Controls

`resetAccumulators()` sets RSTACC (zeroes ENERGY and CHARGE and clears MATHOF, Tables 7-5
and 7-16), verifies the retained configuration, then reads ENERGY and CHARGE once to clear
any overflow flag raised before the reset. It does not verify that continuously
accumulating values stay zero.

`setAlertConfig` accepts only DIAG_ALRT bits 15..12 (ALATCH, CNVR, SLOWALERT, APOL).
DIAG_ALRT bits 7..0 are read/write, including MEMSTAT (reset 1). The method therefore
reads DIAG_ALRT, writes the new configuration bits while preserving the current MEMSTAT
value, and verifies both. The write clears the alert-limit flags and CNVRF. When ALATCH is
set, the read also clears latched flags, so the next `read()` may return NOT_READY.

`setThreshold` accepts raw 16-bit threshold registers 0x0C..0x11. BOVL and BUVL bit 15
is reserved (Tables 7-19 and 7-20) and must be 0. SOVL, SUVL, TEMP_LIMIT and PWR_LIMIT
are full 16-bit fields (Tables 7-17, 7-18, 7-21 and 7-22), so their readback compares all
bits. Signed limits use two's-complement encodings.

`readRegister` offers bounded diagnostic access to implemented register addresses.
Reading DIAG_ALRT can clear ready/latched status; reading ENERGY/CHARGE clears their
overflow indications without updating the accumulator latch. Diagnostic reads must be
serialized with acquisition.

The core has no locks, allocation, retries or scheduling. Maximum SPI transaction is
six bytes; one sample performs eight bounded transfers. The platform implements a
bounded transaction timeout, keeps CS asserted across command plus data, and reports
short/error transfers as failures. Delay failure is propagated explicitly.
