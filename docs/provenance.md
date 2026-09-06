# Provenance

The starting reference is SiboVG's MIT-licensed `ina229-ft232h` Python library,
revision `adeb537829b3f4362ce1d0e325e45033a60a0052`, provided locally at
`/Users/sibo/Desktop/Software/ina229-ft232h`. Its MIT notice is retained in LICENSE.
The reference was inspected but not modified. The C++ implementation preserves the
register framing and SI conversion equations while adding validation, readback,
explicit lifecycle and status, and bounded acquisition.

Register definitions and conversion semantics were checked against the
[TI INA229 Rev. A datasheet](https://www.ti.com/lit/ds/symlink/ina229.pdf).
The FT232H mode-2 workaround is a bench observation from the supplied Python
library, not a universal requirement for other SPI controllers. The core leaves
SPI mode and electrical timing to the platform driver.

Host tests establish protocol/logic behavior only. Physical waveform, accuracy,
calibration, accumulator behavior over time, and target timing remain open.
