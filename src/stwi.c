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

#include "stwi.h"

struct stwi_res stwi_dev_write(struct stwi const *bus,
                               uint8_t addr,
                               stwi_reg_size_t reg_size,
                               uint16_t reg,
                               uint8_t const *buff,
                               size_t size)
{
    struct stwi_res res = {};
    /* Generate start condition */
    res.stage = STWI_STAGE_START;
    STWI_ASSERT(!(res.err = stwi_start(bus)), return res;);
    /* Send device address with WRITE bit */
    res.stage = STWI_STAGE_ADDR;
    STWI_ASSERT(!(res.err = stwi_write_byte(bus, addr << 1 | 0x00)), return res;);
    /* Send register high byte */
    res.stage = STWI_STAGE_REG;
    if (reg_size == STWI_REG_16)
    {
        STWI_ASSERT(!(res.err = stwi_write_byte(bus, reg >> 8 & 0xFF)), return res;);
    }
    /* Send register low byte */
    if (reg_size != STWI_REG_0)
    {
        STWI_ASSERT(!(res.err = stwi_write_byte(bus, reg & 0xFF)), return res;);
    }
    /* Send data */
    res.stage = STWI_STAGE_DATA;
    while (size--)
    {
        STWI_ASSERT(!(res.err = stwi_write_byte(bus, *buff++)), return res;);
        res.data_size++;
    }
    /* Generate stop condition */
    res.stage = STWI_STAGE_STOP;
    STWI_ASSERT(!(res.err = stwi_stop(bus)), return res;);
    return res;
}

struct stwi_res stwi_dev_read(struct stwi const *bus,
                              uint8_t addr,
                              stwi_reg_size_t reg_size,
                              uint16_t reg,
                              uint8_t *buff,
                              size_t size)
{
    struct stwi_res res = {};
    /* Generate start condition */
    res.stage = STWI_STAGE_START;
    STWI_ASSERT(!(res.err = stwi_start(bus)), return res;);
    /* Send device address with WRITE bit */
    res.stage = STWI_STAGE_ADDR;
    STWI_ASSERT(!(res.err = stwi_write_byte(bus, addr << 1 | 0x00)), return res;);
    /* Send register high byte */
    res.stage = STWI_STAGE_REG;
    if (reg_size == STWI_REG_16)
    {
        STWI_ASSERT(!(res.err = stwi_write_byte(bus, (uint8_t)(reg >> 8 & 0xFF))), return res;);
    }
    /* Send register low byte */
    if (reg_size != STWI_REG_0)
    {
        STWI_ASSERT(!(res.err = stwi_write_byte(bus, (uint8_t)(reg & 0xFF))), return res;);
    }
    /* Generate repeated start */
    res.stage = STWI_STAGE_START;
    STWI_ASSERT(!(res.err = stwi_start(bus)), return res;);
    /* Send device address with READ bit */
    res.stage = STWI_STAGE_ADDR;
    STWI_ASSERT(!(res.err = stwi_write_byte(bus, addr << 1 | 0x01)), return res;);
    /* Receive data */
    res.stage = STWI_STAGE_DATA;
    while (size--)
    {
        STWI_ASSERT(!(res.err = stwi_read_byte(bus, buff++, size > 0)), return res;);
        res.data_size++;
    }
    /* Generate stop condition */
    res.stage = STWI_STAGE_STOP;
    STWI_ASSERT(!(res.err = stwi_stop(bus)), return res;);
    return res;
}