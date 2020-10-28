# STWI
STWI is a cross-platform software TWI (or I2C) driver for master device.

## Why to use software driver?
The benefits of using hardware-based drivers are undeniable. But in some cases this is not possible:
- There are no free pins controlled by the driver;
- Peripheral device was connected to wrong pins due to schematic mistake.

But that doesn't mean you should stop experimenting and prototyping. Software driver based on GPIO pins is the solution you need.

## Why to use STWI library?
There are some good platform-specific libraries like [SoftI2CMaster](https://github.com/felias-fogg/SoftI2CMaster) for Arduino. But STWI is written in plain C and can be used on any platform. It is **not the fastest solution**, but you can easily run it on any MCU.

The code is covered with unit tests and the result is predictable. You can see the expected SCL and SDA oscillograms by cloning the repo and running tests (see "test/main.c" and "test/Makefile").

Supported features:
- Clock stretching on bit level;
- Low-level operations such as generating start and stop conditions, reading or writing one bit;
- Complex read and write operations from 8-bit or 16-bit registers (as EEPROM requires) of devices with 7-bit address;
- Only one master is supported.

## How to use
1. Configure SCL and SDA pins as Open-Drain pins with pull-up as bus specification requires.
2. Connect STWI driver to the hardware platform by declaring these functions:
```
void write_scl(struct stwi const *bus, stwi_pin_state_t state)
{
    // Write SCL pin here
}

void write_sda(struct stwi const *bus, stwi_pin_state_t state)
{
    // Write SDA pin here
}

stwi_pin_state_t read_scl(struct stwi const *bus)
{
    // Read SCL pin here
}

stwi_pin_state_t read_sda(struct stwi const *bus)
{
    // Read SDA pin here
}

void timeout_start(struct stwi const *bus)
{
    // Start timeout for clock stretching or just do nothing
}

bool timeout_check(struct stwi const *bus)
{
    // Check timeout for clock stretching or just return false
}

void delay(struct stwi const *bus)
{
    // Wait TWI_CLOCK_FREQ / 4
}
```
3. Declare stwi structure:
```
struct stwi const stwi = {
    .write_scl = write_scl,
    .write_sda = write_sda,
    .read_scl = read_scl,
    .read_sda = read_sda,
    .delay = delay,
    .timeout_start = timeout_start,
    .timeout_check = timeout_check,
};
```
4. Communicate with peripheral devices using the functions in "stwi.h".
