/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2018, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the Simba project.
 */

#include "simba.h"

int board_pin_string_to_device_index(const char *str_p)
{
    long pin;

    if (strncmp(&str_p[0], "d", 1) == 0) {
        if (std_strtol(&str_p[1], &pin) == NULL) {
            return (-ENODEV);
        }

        if (pin == 0) {
            pin = 16;
        } else if (pin == 1) {
            pin = 5;
        } else if (pin == 2) {
            pin = 4;
        } else if (pin == 3) {
            pin = 0;
        } else if (pin == 4) {
            pin = 2;
        } else if (pin == 5) {
            pin = 14;
        } else if (pin == 6) {
            pin = 12;
        } else if (pin == 7) {
            pin = 13;
        } else if (pin == 8) {
            pin = 15;
        } else if (pin == 9) {
            pin = 3;
        } else if (pin == 10) {
            pin = 1;
        } else {
            return (-ENODEV);
        }
    } else if (strcmp(str_p, "led") == 0) {
        pin = 16;
    } else {
        return (-ENODEV);
    }

    return (pin);
}
