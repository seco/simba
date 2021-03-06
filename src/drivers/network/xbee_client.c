/**
 * @section License
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2018, Erik Moqvist
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

#if CONFIG_XBEE_CLIENT_DEBUG_LOG_MASK > -1
#    define DLOG(level, msg, ...)                                       \
    log_object_print(&(self_p)->log, LOG_ ## level, OSTR(msg), ##__VA_ARGS__)
#else
#    define DLOG(level, msg, ...)
#endif

static uint8_t next_frame_id(struct xbee_client_t *self_p)
{
    self_p->frame_id++;

    if (self_p->frame_id == XBEE_FRAME_ID_NO_ACK) {
        self_p->frame_id++;
    }

    return (self_p->frame_id);
}

/**
 * @return zero(0) or negative error code.
 */
static int communicate(struct xbee_client_t *self_p,
                       struct xbee_frame_t *frame_p,
                       uint8_t *response_buf_p,
                       size_t *response_size_p)
{
    ssize_t res;
    uint8_t frame_id;
    struct time_t timeout;

    mutex_lock(&self_p->rpc.mutex);

    if (response_size_p != NULL) {
        mutex_lock(&self_p->rpc.rx.mutex);

        /* Set the frame id. */
        frame_id = next_frame_id(self_p);
        frame_p->data.buf[0] = frame_id;

        /* Response expected. */
        self_p->rpc.rx.frame_p = frame_p;
        self_p->rpc.rx.buf_p = response_buf_p;
        self_p->rpc.rx.size_p = response_size_p;

        /* Write the frame. */
        res = xbee_write(&self_p->driver, frame_p);

        /* Resumed when (or if) the response is received. */
        if (res == 0) {
            timeout.seconds = (CONFIG_XBEE_CLIENT_RESPONSE_TIMEOUT_MS / 1000);
            timeout.nanoseconds =
                (1000000L * (CONFIG_XBEE_CLIENT_RESPONSE_TIMEOUT_MS % 1000));
            res = cond_wait(&self_p->rpc.rx.cond,
                            &self_p->rpc.rx.mutex,
                            &timeout);

            if (res == 0) {
                res = self_p->rpc.rx.res;
            }

            self_p->rpc.rx.frame_p = NULL;
        }

        mutex_unlock(&self_p->rpc.rx.mutex);
    } else {
        /* Set the frame id. */
        frame_p->data.buf[0] = XBEE_FRAME_ID_NO_ACK;

        /* Write the frame. */
        res = xbee_write(&self_p->driver, frame_p);
    }

    mutex_unlock(&self_p->rpc.mutex);

    return (res);
}

static int execute_at_command(struct xbee_client_t *self_p,
                              const char *command_p,
                              const uint8_t *parameter_p,
                              size_t size,
                              uint8_t *response_parameter_p,
                              size_t *response_size_p)
{
    struct xbee_frame_t frame;
    size_t response_size;

    /* Initiate the AT command frame (frame id assigned later). */
    frame.type = XBEE_FRAME_TYPE_AT_COMMAND;
    frame.data.size = (size + 3);
    memcpy(&frame.data.buf[1], command_p, 2);
    memcpy(&frame.data.buf[3], parameter_p, size);

    if (response_size_p == NULL) {
        response_size = 0;
        response_size_p = &response_size;
    }

    return (communicate(self_p,
                        &frame,
                        response_parameter_p,
                        response_size_p));
}

static int handle_rx_packet_16_bit_address(struct xbee_client_t *self_p,
                                           struct xbee_frame_t *frame_p)
{
    ssize_t size;
    struct xbee_client_address_t address;

    size = frame_p->data.size;

    if (size < 4) {
        DLOG(DEBUG,
             "Expected RX Packet 16 Bit Address of at least 4 bytes "
             "but got %u.\r\n",
             size);
        return (-EPROTO);
    }

    address.type = xbee_client_address_type_16_bits_t;
    memcpy(&address.buf[0], &frame_p->data.buf[0], 2);
    size -= 4;
    queue_write(&self_p->chin, &size, sizeof(size));
    queue_write(&self_p->chin, &address, sizeof(address));
    queue_write(&self_p->chin, &frame_p->data.buf[4], size);

    return (0);
}

static int handle_rx_packet_64_bit_address(struct xbee_client_t *self_p,
                                           struct xbee_frame_t *frame_p)
{
    ssize_t size;
    struct xbee_client_address_t address;

    size = frame_p->data.size;

    if (size < 10) {
        DLOG(DEBUG,
             "Expected RX Packet 64 Bit Address of at least 10 bytes "
             "but got %u.\r\n",
             size);
        return (-EPROTO);
    }

    address.type = xbee_client_address_type_64_bits_t;
    memcpy(&address.buf[0], &frame_p->data.buf[0], 8);
    size -= 10;
    queue_write(&self_p->chin, &size, sizeof(size));
    queue_write(&self_p->chin, &address, sizeof(address));
    queue_write(&self_p->chin, &frame_p->data.buf[10], size);

    return (0);
}

static int handle_tx_status(struct xbee_client_t *self_p,
                            struct xbee_frame_t *frame_p)
{
    ssize_t size;
    int frame_id;
    int expected_frame_id;
    int status;

    size = frame_p->data.size;

    if (size != 2) {
        DLOG(DEBUG, "Expected TX Status of 2 bytes but got %u.\r\n", size);
        return (-EPROTO);
    }

    frame_id = frame_p->data.buf[0];
    status = frame_p->data.buf[1];

    mutex_lock(&self_p->rpc.rx.mutex);

    if (self_p->rpc.rx.frame_p != NULL) {
        expected_frame_id = self_p->rpc.rx.frame_p->data.buf[0];

        if (expected_frame_id == frame_id) {
            if (status != 0) {
                DLOG(WARNING,
                     "Negative response %d in TX Status for "
                     "frame id 0x%02x.\r\n",
                     status,
                     frame_id);
                status = -EPROTO;
            }

            self_p->rpc.rx.frame_p = NULL;
            self_p->rpc.rx.res = status;
            cond_signal(&self_p->rpc.rx.cond);
        } else {
            DLOG(DEBUG,
                 "Expected frame id 0x%02x in TX Status but "
                 "got 0x%02x.\r\n",
                 expected_frame_id,
                 frame_id);
        }
    } else {
        DLOG(DEBUG, "Unexpected TX Status received.\r\n");
    }

    mutex_unlock(&self_p->rpc.rx.mutex);

    return (0);
}

static int handle_at_command_response(struct xbee_client_t *self_p,
                                      struct xbee_frame_t *frame_p)
{
    ssize_t size;
    int frame_id;
    int expected_frame_id;
    int status;

    size = frame_p->data.size;

    if (size < 4) {
        DLOG(DEBUG,
             "Expected AT Command Response of at least 4 bytes "
             "but got %u.\r\n",
             size);
        return (-EPROTO);
    }

    frame_id = frame_p->data.buf[0];
    status = frame_p->data.buf[3];

    mutex_lock(&self_p->rpc.rx.mutex);

    if (self_p->rpc.rx.frame_p != NULL) {
        expected_frame_id = self_p->rpc.rx.frame_p->data.buf[0];

        if (expected_frame_id == frame_id) {
            if (status == 0) {
                size = MIN(size - 4, *self_p->rpc.rx.size_p);
                memcpy(self_p->rpc.rx.buf_p, &frame_p->data.buf[4], size);
            } else {
                DLOG(WARNING,
                     "Negative response %d in AT Command Response for "
                     "frame id 0x%02x.\r\n",
                     status,
                     frame_id);
                status = -EPROTO;
            }

            self_p->rpc.rx.frame_p = NULL;
            *self_p->rpc.rx.size_p = size;
            self_p->rpc.rx.res = status;
            cond_signal(&self_p->rpc.rx.cond);
        } else {
            DLOG(DEBUG,
                 "Expected frame id 0x%02x in AT Command Response but "
                 "got 0x%02x.\r\n",
                 expected_frame_id,
                 frame_id);
        }
    } else {
        DLOG(DEBUG, "Unexpected AT Command Response received.\r\n");
    }

    mutex_unlock(&self_p->rpc.rx.mutex);

    return (0);
}

int xbee_client_module_init()
{
#if CONFIG_XBEE_CLIENT_DEBUG_LOG_MASK > -1
    log_module_init();
#endif

    return (xbee_module_init());
}

int xbee_client_init(struct xbee_client_t *self_p,
                     void *chin_p,
                     void *chout_p,
                     void *buf_p,
                     size_t size,
                     int flags)
{
    xbee_init(&self_p->driver, chin_p, chout_p);
    queue_init(&self_p->chin, buf_p, size);

    if (flags & XBEE_CLIENT_NON_BLOCKING_READ) {
        chan_control(&self_p->chin, CHAN_CONTROL_NON_BLOCKING_READ);
    }

    self_p->frame_id = XBEE_FRAME_ID_NO_ACK;
    self_p->rpc.rx.frame_p = NULL;

    mutex_init(&self_p->rpc.mutex);
    mutex_init(&self_p->rpc.rx.mutex);
    cond_init(&self_p->rpc.rx.cond);

#if CONFIG_XBEE_CLIENT_DEBUG_LOG_MASK > -1
    log_object_init(&self_p->log,
                    "xbee_client",
                    CONFIG_XBEE_CLIENT_DEBUG_LOG_MASK);
#endif

    return (0);
}

void *xbee_client_main(void *arg_p)
{
    int res;
    struct xbee_client_t *self_p;
    struct xbee_frame_t frame;

    self_p = arg_p;

    while (1) {
        if (chan_poll(self_p->driver.transport.chin_p, NULL) == NULL) {
            continue;
        }

        res = xbee_read(&self_p->driver, &frame);

        if (res != 0) {
            continue;
        }

        switch (frame.type) {

        case XBEE_FRAME_TYPE_RX_PACKET_16_BIT_ADDRESS:
            handle_rx_packet_16_bit_address(self_p, &frame);
            break;

        case XBEE_FRAME_TYPE_RX_PACKET_64_BIT_ADDRESS:
            handle_rx_packet_64_bit_address(self_p, &frame);
            break;

        case XBEE_FRAME_TYPE_TX_STATUS:
            handle_tx_status(self_p, &frame);
            break;

        case XBEE_FRAME_TYPE_AT_COMMAND_RESPONSE:
            handle_at_command_response(self_p, &frame);
            break;

        default:
            break;
        }
    }

    return (NULL);
}

ssize_t xbee_client_read_from(struct xbee_client_t *self_p,
                              void *buf_p,
                              size_t size,
                              struct xbee_client_address_t *address_p)
{
    size_t data_size;

    queue_read(&self_p->chin, &data_size, sizeof(data_size));
    size = MIN(size, data_size);
    queue_read(&self_p->chin, address_p, sizeof(*address_p));
    queue_read(&self_p->chin, buf_p, size);
    queue_ignore(&self_p->chin, data_size - size);

    return (size);
}

ssize_t xbee_client_write_to(struct xbee_client_t *self_p,
                             const void *buf_p,
                             size_t size,
                             int flags,
                             struct xbee_client_address_t *address_p)
{
    struct xbee_frame_t frame;
    size_t pos;
    size_t response_size;
    int res;

    switch (address_p->type) {

    case xbee_client_address_type_16_bits_t:
        frame.type = XBEE_FRAME_TYPE_TX_REQUEST_16_BIT_ADDRESS;
        memcpy(&frame.data.buf[1], &address_p->buf[0], 2);
        pos = 4;
        break;

    case xbee_client_address_type_64_bits_t:
        frame.type = XBEE_FRAME_TYPE_TX_REQUEST_64_BIT_ADDRESS;
        memcpy(&frame.data.buf[1], &address_p->buf[0], 8);
        pos = 10;
        break;

    default:
        return (-EINVAL);
    }

    frame.data.size = (pos + size);
    frame.data.buf[pos - 1] = 0;
    memcpy(&frame.data.buf[pos], buf_p, size);

    if (flags & XBEE_CLIENT_NO_ACK) {
        res = communicate(self_p, &frame, NULL, NULL);
    } else {
        response_size = 0;
        res = communicate(self_p, &frame, NULL, &response_size);
    }

    if (res != 0) {
        return (res);
    }

    return (size);
}

int xbee_client_pin_set_mode(struct xbee_client_t *self_p,
                             int pin,
                             int mode)
{
    return (xbee_client_pin_write(self_p, pin, 0));
}

int xbee_client_pin_write(struct xbee_client_t *self_p,
                          int pin,
                          int value)
{
    uint8_t u8_value;
    char command[] = "D0";

    command[1] += pin;

    if (value == 0) {
        self_p->pins.value &= ~(1 << pin);
        u8_value = XBEE_PIN_MODE_OUTPUT_LOW;
    } else {
        self_p->pins.value |= (1 << pin);
        u8_value = XBEE_PIN_MODE_OUTPUT_HIGH;
    }

    return (xbee_client_at_command_write_u8(self_p,
                                            &command[0],
                                            u8_value));
}

int xbee_client_pin_toggle(struct xbee_client_t *self_p,
                           int pin)
{
    int value;

    self_p->pins.value ^= (1 << pin);
    value = ((self_p->pins.value >> pin) & 0x1);

    return (xbee_client_pin_write(self_p, pin, value));
}

int xbee_client_pin_convert(struct xbee_client_t *self_p,
                            int pin)
{
    return (xbee_client_at_command_write(self_p, "IS", NULL, 0));
}

ssize_t xbee_client_at_command_read(struct xbee_client_t *self_p,
                                    const char *command_p,
                                    uint8_t *parameter_p,
                                    size_t size)
{
    int res;

    res = execute_at_command(self_p,
                             command_p,
                             NULL,
                             0,
                             parameter_p,
                             &size);

    if (res != 0) {
        return (res);
    }

    return (size);
}

int xbee_client_at_command_write(struct xbee_client_t *self_p,
                                 const char *command_p,
                                 const uint8_t *parameter_p,
                                 size_t size)
{
    return (execute_at_command(self_p,
                               command_p,
                               parameter_p,
                               size,
                               NULL,
                               0));
}

int xbee_client_at_command_read_u8(struct xbee_client_t *self_p,
                                   const char *command_p,
                                   uint8_t *parameter_p)
{
    ssize_t res;

    res = xbee_client_at_command_read(self_p,
                                      command_p,
                                      parameter_p,
                                      sizeof(*parameter_p));

    return (res == sizeof(*parameter_p) ? 0 : res);
}

int xbee_client_at_command_write_u8(struct xbee_client_t *self_p,
                                    const char *command_p,
                                    uint8_t parameter)
{
    ssize_t res;

    res = xbee_client_at_command_write(self_p,
                                       command_p,
                                       &parameter,
                                       sizeof(parameter));

    return (res == sizeof(parameter) ? 0 : res);
}

int xbee_client_at_command_read_u16(struct xbee_client_t *self_p,
                                    const char *command_p,
                                    uint16_t *parameter_p)
{
    ssize_t res;
    uint8_t buf[2];

    res = xbee_client_at_command_read(self_p,
                                      command_p,
                                      &buf[0],
                                      sizeof(buf));

    *parameter_p = ((buf[0] << 8) | buf[1]);

    return (res == sizeof(buf) ? 0 : res);
}

int xbee_client_at_command_write_u16(struct xbee_client_t *self_p,
                                     const char *command_p,
                                     uint16_t parameter)
{
    ssize_t res;
    uint8_t buf[2];

    buf[0] = (parameter >> 8);
    buf[1] = parameter;

    res = xbee_client_at_command_write(self_p,
                                       command_p,
                                       &buf[0],
                                       sizeof(buf));

    return (res == sizeof(buf) ? 0 : res);
}

int xbee_client_at_command_read_u32(struct xbee_client_t *self_p,
                                    const char *command_p,
                                    uint32_t *parameter_p)
{
    ssize_t res;
    uint8_t buf[4];

    res = xbee_client_at_command_read(self_p,
                                      command_p,
                                      &buf[0],
                                      sizeof(buf));

    *parameter_p = (((uint32_t)buf[0] << 24)
                    | ((uint32_t)buf[1] << 16)
                    | ((uint32_t)buf[2] << 8)
                    | (uint32_t)buf[3]);

    return (res == sizeof(buf) ? 0 : res);
}

int xbee_client_at_command_write_u32(struct xbee_client_t *self_p,
                                     const char *command_p,
                                     uint32_t parameter)
{
    ssize_t res;
    uint8_t buf[4];

    buf[0] = (parameter >> 24);
    buf[1] = (parameter >> 16);
    buf[2] = (parameter >> 8);
    buf[3] = parameter;

    res = xbee_client_at_command_write(self_p,
                                       command_p,
                                       &buf[0],
                                       sizeof(buf));

    return (res == sizeof(buf) ? 0 : res);
}

int xbee_client_print_address(void *chan_p,
                              struct xbee_client_address_t *address_p)
{
    switch (address_p->type) {

    case xbee_client_address_type_invalid_t:
        std_fprintf(chan_p, OSTR("Address: invalid\r\n"));
        break;

    case xbee_client_address_type_16_bits_t:
        std_fprintf(chan_p,
                    OSTR("Address: 0x%02x%02x\r\n"),
                    address_p->buf[0],
                    address_p->buf[1]);
        break;

    case xbee_client_address_type_64_bits_t:
        std_fprintf(chan_p,
                    OSTR("Address: 0x%02x%02x%02x%02x%02x%02x%02x%02x\r\n"),
                    address_p->buf[0],
                    address_p->buf[1],
                    address_p->buf[2],
                    address_p->buf[3],
                    address_p->buf[4],
                    address_p->buf[5],
                    address_p->buf[6],
                    address_p->buf[7]);
        break;

    default:
        std_fprintf(chan_p, OSTR("Address: unknown\r\n"));
        break;

    }

    return (0);
}
