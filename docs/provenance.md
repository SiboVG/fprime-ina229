# Provenance

The starting reference is SiboVG's MIT-licensed `ina229-ft232h` Python library,
revision `adeb537829b3f4362ce1d0e325e45033a60a0052`, provided locally at
`/Users/sibo/Desktop/Software/ina229-ft232h`. Its MIT notice is retained in LICENSE.
The reference was inspected but not modified. The C++ implementation preserves the
register framing and SI conversion equations while adding validation, readback,
explicit lifecycle and status, and bounded acquisition.

Register definitions and conversion semantics were checked against the
[TI INA229 datasheet](https://www.ti.com/lit/ds/symlink/ina229.pdf), SLYS023A Rev. A
(December 2020, revised May 2022). On 2026-09-27 that revision was downloaded again and
the SPI frame (section 7.5.1.1), every register table in section 7.6.1, and the section
8.1.2 equations were re-read against the code. That review led to the DIAG_ALRT
read-modify-write that preserves MEMSTAT, and to the per-channel overflow containment.
It confirmed that TEMP_LIMIT is a full 16-bit field.
The FT232H mode-2 workaround is a bench observation from the supplied Python
library, not a universal requirement for other SPI controllers. The core leaves
SPI mode and electrical timing to the platform driver.

Host tests establish protocol/logic behavior only. Physical waveform, accuracy,
calibration, accumulator behavior over time, and target timing remain open.
