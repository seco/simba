#
# @section License
#
# The MIT License (MIT)
#
# Copyright (c) 2017-2018, Erik Moqvist
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use, copy,
# modify, merge, publish, distribute, sublicense, and/or sell copies
# of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# This file is part of the Simba project.
#

NAME = bmp280_suite
TYPE = suite
BOARD ?= linux

CDEFS += \
	CONFIG_BMP280=1 \
	CONFIG_BMP280_COVERTION_TIMEOUT_MS=3 \
	CONFIG_BMP280_DEBUG_LOG_MASK=LOG_ALL \
	CONFIG_I2C=1 \
	CONFIG_SPI=1

STUB = $(addprefix $(SIMBA_ROOT)/src/drivers/sensors/bmp280.c:, \
	 i2c_* \
	 spi_*)

KERNEL_SRC += errno.c
DRIVERS_SRC = sensors/bmp280.c network/spi.c network/i2c.c

SRC += $(addprefix $(SIMBA_ROOT)/tst/stubs/, \
	drivers/network/i2c_mock.c \
	drivers/network/spi_mock.c)

include $(SIMBA_ROOT)/make/app.mk
