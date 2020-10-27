/*  
 * Copyright (c) 2020 Oleg Dolgy
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Software implementation of Two Wire Interface
 * 
 */

#ifndef SOFTBUS_STWI_H
#define SOFTBUS_STWI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Error codes */
typedef enum
{
    /* Success */
    STWI_ERR_OK = 0,
    /* Clock stretch timeout */
    STWI_ERR_STRETCH,
    /* NACK received */
    STWI_ERR_NACK,
} stwi_err_t;

/* Complex operation progress */
typedef enum
{
    /* Generating start condition */
    STWI_STAGE_START,
    /* Sending device address */
    STWI_STAGE_ADDR,
    /* Sending register address */
    STWI_STAGE_REG,
    /* Sending or receiving data */
    STWI_STAGE_DATA,
    /* Generating stop condition */
    STWI_STAGE_STOP,
} stwi_stage_t;

/* Complex operation result */
struct stwi_res
{
    stwi_err_t err;
    /* Last stage */
    stwi_stage_t stage;
    /* Sent or received bytes count */
    size_t data_size;
};

/* GPIO pin state */
typedef enum
{
    STWI_PIN_LOW = 0,
    STWI_PIN_HIGH,
} stwi_pin_state_t;

/* Register size in bits */
typedef enum
{
    STWI_REG_0,
    STWI_REG_8,
    STWI_REG_16,
} stwi_reg_size_t;

/* Software TWI bus handle */
struct stwi
{
    /* Set state of the SCL pin */
    void (*write_scl)(struct stwi const *bus, stwi_pin_state_t state);
    /* Set state of the SDA pin */
    void (*write_sda)(struct stwi const *bus, stwi_pin_state_t state);
    /* Get state of the SCL pin */
    stwi_pin_state_t (*read_scl)(struct stwi const *bus);
    /* Get state of the SDA pin */
    stwi_pin_state_t (*read_sda)(struct stwi const *bus);
    /* Wait for a period equals to the quarter period of the clock frequency */
    void (*delay)(struct stwi const *bus);
    /* Start timeout timer for clock stretching */
    void (*timeout_start)(struct stwi const *bus);
    /* Check whether clock stretching timeout is not expired.
     * If you want to disable clock stretch you should just return 'false' always. */
    bool (*timeout_check)(struct stwi const *bus);
};

#define STWI_ASSERT(exp, act) \
    if (!(exp)) { act }

/* Wait until slave device releases SCL line (clock stretch) */
static inline stwi_err_t stwi_stretch_wait(struct stwi const *bus)
{
    if (bus->read_scl(bus) == STWI_PIN_LOW)
    {
        bus->timeout_start(bus);
        do
        {
            STWI_ASSERT(bus->timeout_check(bus), return STWI_ERR_STRETCH;);
            bus->delay(bus);
        } while (bus->read_scl(bus) == STWI_PIN_LOW);
    }
    return STWI_ERR_OK;
}

/* Generate clock pulse and send one bit */
static inline stwi_err_t stwi_write_bit(struct stwi const *bus, stwi_pin_state_t bit)
{
    stwi_err_t err;
    bus->write_sda(bus, bit);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    STWI_ASSERT(!(err = stwi_stretch_wait(bus)), return err;);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_LOW);
    bus->delay(bus);
    return STWI_ERR_OK;
}

/* Generate clock pulse and receive one bit */
static inline stwi_err_t stwi_read_bit(struct stwi const *bus, stwi_pin_state_t *bit)
{
    stwi_err_t err;
    bus->write_sda(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    STWI_ASSERT(!(err = stwi_stretch_wait(bus)), return err;);
    bus->delay(bus);
    *bit = bus->read_sda(bus);
    bus->write_scl(bus, STWI_PIN_LOW);
    bus->delay(bus);
    return STWI_ERR_OK;
}

/* Generate start or repeated start condition */
static inline stwi_err_t stwi_start(struct stwi const *bus)
{
    stwi_err_t err;
    /* Release lines (necessary for repeated start) */
    bus->write_sda(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    STWI_ASSERT(!(err = stwi_stretch_wait(bus)), return err;);
    /* Generate srart */
    bus->write_sda(bus, STWI_PIN_LOW);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_LOW);
    bus->delay(bus);
    return STWI_ERR_OK;
}

/* Generate stop condition */
static inline stwi_err_t stwi_stop(struct stwi const *bus)
{
    stwi_err_t err;
    bus->write_sda(bus, STWI_PIN_LOW);
    bus->delay(bus);
    bus->write_scl(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    STWI_ASSERT(!(err = stwi_stretch_wait(bus)), return err;);
    bus->write_sda(bus, STWI_PIN_HIGH);
    bus->delay(bus);
    return STWI_ERR_OK;
}

/* Send one byte and receive ACK or NACK bit */
static inline stwi_err_t stwi_write_byte(struct stwi const *bus, uint8_t byte)
{
    stwi_err_t err;
    /* Send byte */
    for (int i = 0; i < 8; i++)
    {
        err = stwi_write_bit(bus, (byte & 0x80) ? STWI_PIN_HIGH : STWI_PIN_LOW);
        STWI_ASSERT(!err, return err;);
        byte <<= 1;
    }
    /* Receive ACK or NACK bit */
    stwi_pin_state_t bit;
    STWI_ASSERT(!(err = stwi_read_bit(bus, &bit)), return err;);
    return (bit == STWI_PIN_LOW) ? STWI_ERR_OK : STWI_ERR_NACK;
}

/* Receive one byte and send ACK or NACK bit */
static inline stwi_err_t stwi_read_byte(struct stwi const *bus, uint8_t *byte, bool ack)
{
    stwi_err_t err;
    uint8_t data = 0;
    /* Receive byte */
    for (int i = 0; i < 8; i++)
    {
        stwi_pin_state_t bit;
        STWI_ASSERT(!(err = stwi_read_bit(bus, &bit)), return err;);
        data = data << 1 | ((bit == STWI_PIN_HIGH) ? 0x01 : 0x00);
    }
    /* Send ACK or NACK bit */
    STWI_ASSERT(!(err = stwi_write_bit(bus, ack ? STWI_PIN_LOW : STWI_PIN_HIGH)), return err;);
    *byte = data;
    return STWI_ERR_OK;
}

/* Send data array to the specified register of the device with 7-bit address */
struct stwi_res stwi_dev_write(struct stwi const *bus,
                               uint8_t addr,
                               stwi_reg_size_t reg_size,
                               uint16_t reg,
                               uint8_t const *buff,
                               size_t size);

/* Receive data array from the specified register of the device with 7-bit address */
struct stwi_res stwi_dev_read(struct stwi const *bus,
                              uint8_t addr,
                              stwi_reg_size_t reg_size,
                              uint16_t reg,
                              uint8_t *buff,
                              size_t size);

#endif /* SOFTBUS_STWI_H */
