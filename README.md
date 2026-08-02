# AHT20 I2C Userspace Driver (C)

A userspace driver for the AHT20 temperature and humidity sensor, written in C for Raspberry Pi.
Implemented directly from the datasheet using the Linux I2C device interface, without relying on any sensor library.

## Why not use an existing library?

Libraries like `adafruit_ahtx0` abstract away the entire I2C communication layer.
I wrote this from scratch to understand what actually happens at the bus level:
command sequencing, measurement timing, packed data layout, and integrity checking.

## Features

- Direct I2C communication via `open` / `ioctl` / `write` / `read`
- Datasheet-compliant command sequence (`0xAC 0x33 0x00`) with required 80ms measurement delay
- 20-bit packed data parsing (humidity and temperature share one byte)
- CRC-8 verification (polynomial `0x31`, init `0xFF`)
- Sensor status checks (busy bit, calibration bit)
- Full error handling with resource cleanup on every failure path

## Data Format

The sensor returns 7 bytes:

| Byte | Content |
|------|---------|
| 0 | Status (bit7 = busy, bit3 = calibrated) |
| 1-2 | Humidity (upper 16 bits) |
| 3 | Humidity (low nibble) + Temperature (high nibble) |
| 4-5 | Temperature (lower 16 bits) |
| 6 | CRC-8 of bytes 0-5 |

Byte 3 is shared between both readings, which requires masking and shifting to separate.

## Build & Run

```bash
gcc -Wall aht20.c -o aht20
./aht20
```

Output:
```
Temperature: 29.71 C
Humidity:    36.72 %
```

## Hardware

- Raspberry Pi (I2C bus 1 enabled)
- AHT20 sensor at address `0x38`

Verify the sensor is detected:
```bash
i2cdetect -y 1
```

## What I learned

- Timing matters and fails silently: skipping the 80ms delay returns stale data with no error
- CRC catches transmission corruption that would otherwise look like valid readings
- Every error path must release acquired resources — the same discipline required in kernel drivers

## Next steps

- Port to a Linux kernel module using `i2c_master_send` / `i2c_master_recv`
- Add device tree overlay for automatic driver binding
- Expose readings through the IIO subsystem

- Continuous monitoring loop with error recovery (retries on transient failures)

### Wiring

| AHT20 | Raspberry Pi |
|-------|--------------|
| VCC   | Pin 1 (3.3V) |
| GND   | Pin 6        |
| SDA   | Pin 3 (GPIO2)|
| SCL   | Pin 5 (GPIO3)|



