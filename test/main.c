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
 * Test for Software TWI module
 * 
 */

#include "stwi.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

/* Set to 1 if you want to see GPIO oscillograms */
#ifndef PRINT_SAMPLES
#define PRINT_SAMPLES 0
#endif

/*------------------------------------------------------------------------------------------------*/
/* GPIO pins */
/*------------------------------------------------------------------------------------------------*/
struct gpio_pin
{
    stwi_pin_state_t real;
    stwi_pin_state_t out;
    bool out_set;
    char const *in_samples;
    size_t samples_count;
    char samples[500];
};

/* Set default pin state */
static inline struct gpio_pin gpio_pin_new(void)
{
    return (struct gpio_pin){
        .real = STWI_PIN_HIGH, /* Pull-up */
        .out = STWI_PIN_HIGH,  /* Pull-up */
        .in_samples = "",
    };
}

/* Get actual pin state */
static inline stwi_pin_state_t gpio_pin_read(struct gpio_pin *pin)
{
    return pin->real;
}

/* Change pin state from the MCU side */
static void gpio_pin_write(struct gpio_pin *pin, stwi_pin_state_t state)
{
    TEST_ASSERT(!pin->out_set);
    pin->out_set = true;
    pin->out = state;
}

/* Get actual oscillogram */
static inline char const *gpio_pin_get_samples(struct gpio_pin *pin)
{
    return pin->samples;
}

/* Set oscillogram of the connected external device */
static void gpio_pin_set_in(struct gpio_pin *pin,
                            char const *samples)
{
    TEST_ASSERT(samples);
    for (char const *c = samples; *c; c++)
    {
        TEST_ASSERT(*c == '_' || *c == '/' || *c == '^' || *c == '\\');
    }
    pin->in_samples = samples;
}

/* Save current GPIO state to the log */
static void gpio_pin_sample(struct gpio_pin *pin)
{
    TEST_ASSERT(pin->samples_count < sizeof(pin->samples));

    stwi_pin_state_t pin_in = (*pin->in_samples == '_' || *pin->in_samples == '\\') ?
                                  STWI_PIN_LOW :
                                  STWI_PIN_HIGH; /* Default state */
    if (*pin->in_samples) { pin->in_samples++; }

    /* Open drain pins with pull up */
    stwi_pin_state_t new_real = (pin->out == STWI_PIN_LOW || pin_in == STWI_PIN_LOW) ?
                                    STWI_PIN_LOW :
                                    STWI_PIN_HIGH; /* Default state */

    pin->samples[pin->samples_count++] = (pin->real == new_real) ?
                                             (new_real ? '^' : '_') :
                                             (new_real ? '/' : '\\');
    pin->out_set = false;
    pin->real = new_real;
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Software TWI implementation */
/*------------------------------------------------------------------------------------------------*/
static struct gpio_pin pin_scl, pin_sda;
static int stretch_timer;
static int const stretch_timer_max = 16;

static void write_scl(struct stwi const *bus, stwi_pin_state_t state)
{
    gpio_pin_write(&pin_scl, state);
}

static void write_sda(struct stwi const *bus, stwi_pin_state_t state)
{
    gpio_pin_write(&pin_sda, state);
}

static stwi_pin_state_t read_scl(struct stwi const *bus)
{
    return gpio_pin_read(&pin_scl);
}

static stwi_pin_state_t read_sda(struct stwi const *bus)
{
    return gpio_pin_read(&pin_sda);
}

static void timeout_start(struct stwi const *bus)
{
    stretch_timer = stretch_timer_max;
}

static bool timeout_check(struct stwi const *bus)
{
    return (stretch_timer > 0);
}

static void delay(struct stwi const *bus)
{
    gpio_pin_sample(&pin_scl);
    gpio_pin_sample(&pin_sda);
    if (stretch_timer) { stretch_timer--; }
}

static struct stwi const stwi = {
    .write_scl = write_scl,
    .write_sda = write_sda,
    .read_scl = read_scl,
    .read_sda = read_sda,
    .delay = delay,
    .timeout_start = timeout_start,
    .timeout_check = timeout_check,
};
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Unity hooks */
/*------------------------------------------------------------------------------------------------*/
void setUp(void)
{
    stretch_timer = 0;
    pin_scl = gpio_pin_new();
    pin_sda = gpio_pin_new();
}

void tearDown(void)
{
#if PRINT_SAMPLES
    printf("\nSCL: %s", gpio_pin_get_samples(&pin_scl));
    printf("\nSDA: %s\n\n", gpio_pin_get_samples(&pin_sda));
#endif
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Tests */
/*------------------------------------------------------------------------------------------------*/
static void test_start(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    /* The timer hasn't been started */
    TEST_ASSERT_EQUAL_INT(0, stretch_timer);
    TEST_ASSERT_EQUAL_STRING("^^^\\", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_", gpio_pin_get_samples(&pin_sda));
}

static void test_start_stretch(void)
{
    gpio_pin_set_in(&pin_scl, "____");
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_INT(11, stretch_timer);
    TEST_ASSERT_EQUAL_STRING("\\___/^\\", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^^^^\\_", gpio_pin_get_samples(&pin_sda));
}

static void test_start_stretch_timeout(void)
{
    gpio_pin_set_in(&pin_scl, "\\_________________");
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_STRETCH);
    TEST_ASSERT_EQUAL_INT(0, stretch_timer);
    TEST_ASSERT_EQUAL_STRING("\\_________________", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^^^^^^^^^^^^^^^^^", gpio_pin_get_samples(&pin_sda));
}

static void test_repeated_start(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^\\_", gpio_pin_get_samples(&pin_sda));
}

static void test_stop(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_INT(stwi_stop(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\___/", gpio_pin_get_samples(&pin_sda));
}

static void test_stop_stretch(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_scl, "____");
    TEST_ASSERT_EQUAL_INT(stwi_stop(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_STRING("^^^\\____/^", gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\______/", gpio_pin_get_samples(&pin_sda));
}

static void test_read_byte_ack(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_sda, "/^^^\\___/^^^\\_______/^^^\\___");
    uint8_t byte = 0x00;
    TEST_ASSERT_EQUAL_INT(stwi_read_byte(&stwi, &byte, true), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_UINT8(0xA5, byte);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^^^\\___/^^^\\_______/^^^\\___/^^^\\___",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_read_byte_nack(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_sda, "/^^^\\___/^^^\\_______/^^^\\___");
    uint8_t byte = 0x00;
    TEST_ASSERT_EQUAL_INT(stwi_read_byte(&stwi, &byte, false), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_UINT8(0xA5, byte);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^^^\\___/^^^\\_______/^^^\\___/^^^^^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_read_byte_stretch(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    /* Slave device slows down the bus clock */
    gpio_pin_set_in(&pin_scl, "_/^\\___/^\\___/^\\___/^\\_/^\\_/^\\_/^\\_/^\\_/^\\");
    gpio_pin_set_in(&pin_sda, "/^^^\\_______/^^^\\_________/^^^\\___");
    uint8_t byte = 0x00;
    TEST_ASSERT_EQUAL_INT(stwi_read_byte(&stwi, &byte, true), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_UINT8(0xA5, byte);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\___/^\\___/^\\___/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^^^\\_______/^^^\\_________/^^^\\___/^^^\\___",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_write_byte_ack(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_sda, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___");
    TEST_ASSERT_EQUAL_INT(stwi_write_byte(&stwi, 0x5A), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\___/^^^^^^^\\___/^^^\\_______",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_write_byte_nack(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_INT(stwi_write_byte(&stwi, 0x5A), STWI_ERR_NACK);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\___/^^^^^^^\\___/^^^\\___/^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_write_byte_stretch(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    /* Slave device slows down the bus clock */
    gpio_pin_set_in(&pin_scl, "^^^^^^^^^^^____");
    gpio_pin_set_in(&pin_sda, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___");
    TEST_ASSERT_EQUAL_INT(stwi_write_byte(&stwi, 0x5A), STWI_ERR_OK);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\___/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\___/^^^^^^^^^\\___/^^^\\_______",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_write_2bytes(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_sda, "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___");
    TEST_ASSERT_EQUAL_INT(stwi_write_byte(&stwi, 0xC2), STWI_ERR_OK);   /* ACK was received */
    TEST_ASSERT_EQUAL_INT(stwi_write_byte(&stwi, 0xF7), STWI_ERR_NACK); /* NACK was received */
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^^^^^^^\\_______________/^^^\\_______/^^^^^^^^^^^^^^^\\___/"
                             "^^^^^^^^^^^^^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_read_2bytes(void)
{
    TEST_ASSERT_EQUAL_INT(stwi_start(&stwi), STWI_ERR_OK);
    gpio_pin_set_in(&pin_sda, "/^^^^^^^\\_______________/^^^\\____");
    uint8_t byte = 0x00;
    TEST_ASSERT_EQUAL_INT(stwi_read_byte(&stwi, &byte, true), STWI_ERR_OK); /* Send ACK */
    TEST_ASSERT_EQUAL_UINT8(0xC2, byte);
    gpio_pin_set_in(&pin_sda, "/^^^^^^^^^^^^^^^\\___/^^^^^^^^^^^^^^^");
    TEST_ASSERT_EQUAL_INT(stwi_read_byte(&stwi, &byte, false), STWI_ERR_OK); /* Send NACK */
    TEST_ASSERT_EQUAL_UINT8(0xF7, byte);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_/^^^^^^^\\_______________/^^^\\_______/^^^^^^^^^^^^^^^\\___/"
                             "^^^^^^^^^^^^^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_reg16(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 2 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Data 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Data 2 + ACK */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_16, 0xF1F2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\_______/^^^^^^^^^^^^^^^\\___"
                             "________/^^^\\___/^^^^^^^^^^^^^^^\\_______/^^^\\_________________"
                             "__/^^^\\_______/^^^\\_______________/^^^^^^^\\___/^^^\\__________"
                             "___/",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_reg8(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Data 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Data 2 + ACK */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\_______/^^^^^^^^^^^^^^^\\___"
                             "____/^^^\\___________________/^^^\\_______/^^^\\_______________/^"
                             "^^^^^^\\___/^^^\\_____________/",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_reg0(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Data 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Data 2 + ACK */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_0, 0x0,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\___________________/^^^\\__"
                             "_____/^^^\\_______________/^^^^^^^\\___/^^^\\_____________/",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_err_start(void)
{
    gpio_pin_set_in(&pin_scl, "\\_________________"); /* Clock stretch */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_START, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_write_err_addr(void)
{
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_ADDR, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\___/^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_err_reg(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Address + ACK */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_REG, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\_______/^^^^^^^^^^^^^^^\\___"
                             "____/^^^\\___/^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_err_data(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Data 1 + ACK */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_DATA, res.stage);
    /* 1 byte was sent */
    TEST_ASSERT_EQUAL_size_t(1, res.data_size);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\_______/^^^^^^^^^^^^^^^\\___"
                             "____/^^^\\___________________/^^^\\_______/^^^\\_______________/^"
                             "^^^^^^\\___/^^^\\_______/^^^",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_write_err_stop(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Data 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Data 2 + ACK */
    gpio_pin_set_in(&pin_scl, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Register 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Data 2 + ACK */
                              "\\_________________");                   /* Clock stretch */
    struct stwi_res res = stwi_dev_write(&stwi, 0x25, STWI_REG_8, 0xF2,
                                         (uint8_t *)"\x12\x34", 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    /* 2 bytes were sent */
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
}

static void test_dev_read_reg16(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 2 + ACK */
                              "^^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^^^^^"   /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___/^^^"); /* Data 2 + ACK */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_16, 0xF1F2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("\xBF\xFE", buff, 2);
    TEST_ASSERT_EQUAL_STRING("^^^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\_/^\\"
                             "_/^\\_/^\\_/^\\_/^\\_/^",
                             gpio_pin_get_samples(&pin_scl));
    TEST_ASSERT_EQUAL_STRING("^^\\_____/^^^\\_______/^^^\\___/^^^\\_______/^^^^^^^^^^^^^^^\\___"
                             "________/^^^\\___/^^^^^^^^^^^^^^^\\_______/^^^\\_______/^\\_____/^"
                             "^^\\_______/^^^\\___/^^^^^^^\\___/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^\\"
                             "___/^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___/^^^\\_/",
                             gpio_pin_get_samples(&pin_sda));
}

static void test_dev_read_reg8(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "^^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^^^^^"   /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___/^^^"); /* Data 2 + ACK */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("\xBF\xFE", buff, 2);
}

static void test_dev_read_reg0(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "^^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^^^^^"   /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___/^^^"); /* Data 2 + ACK */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_0, 0x00, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_OK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("\xBF\xFE", buff, 2);
}

static void test_dev_read_err_start(void)
{
    gpio_pin_set_in(&pin_scl, "\\_________________"); /* Clock stretch */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_START, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_read_err_addr(void)
{
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_ADDR, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_read_err_reg(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Address + ACK */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_REG, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_read_err_rep_start(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Register 1 + ACK */
    gpio_pin_set_in(&pin_scl, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Register 1 + ACK */
                              "\\_________________");                   /* Clock stretch */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_START, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_read_err_rep_addr(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"); /* Register 1 + ACK */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_NACK, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_ADDR, res.stage);
    TEST_ASSERT_EQUAL_size_t(0, res.data_size);
}

static void test_dev_read_err_data(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Repeated address + ACK */
                              "/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^^^^^"); /* Data 1 + ACK */
    gpio_pin_set_in(&pin_scl, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Register 1 + ACK */
                              "^^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Repeated address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Data 1 + ACK */
                              "\\_________________");                   /* Clock stretch */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_DATA, res.stage);
    /* 1 byte was received */
    TEST_ASSERT_EQUAL_size_t(1, res.data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("\xBF\x00", buff, 2);
}

static void test_dev_read_err_stop(void)
{
    gpio_pin_set_in(&pin_sda, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Register 1 + ACK */
                              "/^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___"   /* Repeated address + ACK */
                              "/^^^\\___/^^^^^^^^^^^^^^^^^^^^^^^^^^^"   /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^\\___/^^^"); /* Data 2 + ACK */
    gpio_pin_set_in(&pin_scl, "^^^^"                                    /* Start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Register 1 + ACK */
                              "^^^^"                                    /* Repeated start */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Repeated address + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Data 1 + ACK */
                              "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"    /* Data 2 + ACK */
                              "\\_________________");                   /* Clock stretch */
    uint8_t buff[2] = {};
    struct stwi_res res = stwi_dev_read(&stwi, 0x25, STWI_REG_8, 0xF2, buff, 2);
    TEST_ASSERT_EQUAL_INT(STWI_ERR_STRETCH, res.err);
    TEST_ASSERT_EQUAL_INT(STWI_STAGE_STOP, res.stage);
    /* 2 bytes were received */
    TEST_ASSERT_EQUAL_size_t(2, res.data_size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY("\xBF\xFE", buff, 2);
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_start);
    RUN_TEST(test_start_stretch);
    RUN_TEST(test_start_stretch_timeout);
    RUN_TEST(test_repeated_start);
    RUN_TEST(test_stop);
    RUN_TEST(test_stop_stretch);
    RUN_TEST(test_read_byte_ack);
    RUN_TEST(test_read_byte_nack);
    RUN_TEST(test_read_byte_stretch);
    RUN_TEST(test_write_byte_ack);
    RUN_TEST(test_write_byte_nack);
    RUN_TEST(test_write_byte_stretch);
    RUN_TEST(test_write_2bytes);
    RUN_TEST(test_read_2bytes);
    RUN_TEST(test_dev_write_reg16);
    RUN_TEST(test_dev_write_reg8);
    RUN_TEST(test_dev_write_reg0);
    RUN_TEST(test_dev_write_err_start);
    RUN_TEST(test_dev_write_err_addr);
    RUN_TEST(test_dev_write_err_reg);
    RUN_TEST(test_dev_write_err_data);
    RUN_TEST(test_dev_write_err_stop);
    RUN_TEST(test_dev_read_reg16);
    RUN_TEST(test_dev_read_reg8);
    RUN_TEST(test_dev_read_reg0);
    RUN_TEST(test_dev_read_err_start);
    RUN_TEST(test_dev_read_err_addr);
    RUN_TEST(test_dev_read_err_reg);
    RUN_TEST(test_dev_read_err_rep_start);
    RUN_TEST(test_dev_read_err_rep_addr);
    RUN_TEST(test_dev_read_err_data);
    RUN_TEST(test_dev_read_err_stop);
    return UNITY_END();
}
/*------------------------------------------------------------------------------------------------*/