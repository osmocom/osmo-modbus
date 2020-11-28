/*! \file conn_rtu.c
 * modbus connection RTU specifics */
/*
 * Copyright (C) 2020  Pau Espin Pedrol <pespin@espeweb.net>
 *
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* https://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>

#include <osmocom/core/serial.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/bits.h>

#include <osmocom/modbus/modbus.h>
#include <osmocom/modbus/modbus_rtu.h>

#include "modbus_internal.h"
#include "rtu_internal.h"
#include "rtu_transmit_fsm.h"

#define RTU_DEFAULT_BAUDRATE 9600

#define LOGPRTU(rtu, subsys, level, fmt, args ...) \
	LOGP(subsys, level, "(addr=%" PRIu16 ",dev=%s) " fmt, (rtu)->conn->address, (rtu)->dev_path, ## args)

static struct msgb *modbus_rtu_msgb_alloc(void)
{
	return msgb_alloc(MODBUS_MSGB_SIZE, "");
}

struct osmo_tdef g_rtu_tdefs[] = {
	{ .T=15, .default_val=1, .unit = OSMO_TDEF_US, .desc="Timeout for RTU transmission T1.5, in microseconds" },
	{ .T=35, .default_val=1, .unit = OSMO_TDEF_US, .desc="Timeout for RTU transmission T3.5, in microseconds" },
	{}
};


/* High-Order Byte Table */
static const uint8_t table_crc_hi[] = {
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
	0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
	0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Low-Order Byte Table  */
static const uint8_t table_crc_lo[] = {
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
	0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
	0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
	0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
	0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
	0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
	0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
	0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
	0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
	0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
	0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
	0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
	0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
	0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
	0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
	0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
	0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
	0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

/* CRC Generation Function */
uint16_t crc16(uint8_t *data, uint16_t data_len)
{
	uint8_t crc_hi = 0xFF; /* Initialized high CRC byte */
	uint8_t crc_lo = 0xFF; /* Initialized low CRC byte */
	unsigned int idx; /* will index into CRC lookup */

	/* pass through message buffer */
	while (data_len--) {
		idx = crc_hi ^ *data++; /* calculate the CRC  */
		crc_hi = crc_lo ^ table_crc_hi[idx];
		crc_lo = table_crc_lo[idx];
	}

	return (crc_hi << 8 | crc_lo);
}

struct msgb* prim2rtu(struct osmo_modbus_prim *prim)
{
	struct msgb *msg = modbus_rtu_msgb_alloc();
	uint8_t *code;
	size_t len;

	msgb_put_u8(msg, (uint8_t)prim->address);
	code = msgb_put(msg, 1); /* fll later */
	switch (OSMO_PRIM_HDR(&prim->oph)) {
	case OSMO_PRIM(OSMO_MODBUS_PRIM_N_MULT_HOLD_REG, PRIM_OP_REQUEST):
		*code = OSMO_MODBUS_FUNC_READ_MULT_HOLD_REG;
		msgb_put_u16(msg, prim->u.read_mult_hold_reg_req.first_reg);
		msgb_put_u16(msg, prim->u.read_mult_hold_reg_req.num_reg);
		break;
	case OSMO_PRIM(OSMO_MODBUS_PRIM_N_MULT_HOLD_REG, PRIM_OP_RESPONSE):
		*code = OSMO_MODBUS_FUNC_READ_MULT_HOLD_REG;
		msgb_put_u8(msg, prim->u.read_mult_hold_reg_resp.num_reg * 2);
		len = prim->u.read_mult_hold_reg_resp.num_reg * sizeof(uint16_t);
		memcpy(msgb_put(msg, len),
		       prim->u.read_mult_hold_reg_resp.registers, len);
		break;
	default:
		OSMO_ASSERT(0);
	}
	msgb_put_u16(msg, crc16(msgb_data(msg), msgb_length(msg)));
	return msg;
}

/* Address (1Byte) + Function Code (1Byte) */
#define RTU_HDR_LEN 2
#define RTU_CRC_LEN 2

/* Returns size used if succeeded, returns -ENODATA if data missing to parse message */
int rtu2prim(struct osmo_modbus_conn_rtu* rtu, struct msgb* msg, struct osmo_modbus_prim **prim)
{
	uint8_t *data = msgb_data(msg);
	size_t len = msgb_length(msg);
	uint8_t address;
	uint8_t exp_len_nocrc;
	uint16_t exp_crc;
	uint8_t byte_count;
	enum osmo_modbus_function_code code;

	if (len < RTU_HDR_LEN) {
		return -ENODATA;
	}

	address = data[0];
	code = (enum osmo_modbus_function_code)data[1];

	switch (code) {
	case OSMO_MODBUS_FUNC_READ_MULT_HOLD_REG:
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_INFO, "Received OSMO_MODBUS_FUNC_READ_MULT_HOLD_REG: %s\n", osmo_hexdump(data, len));
		if (len < RTU_HDR_LEN + 4)
			return -ENODATA;
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_DEBUG, "Received total %zu bytes: %s\n", len, osmo_hexdump(data, len));
		/* Let's first try to decode Response */
		byte_count = data[RTU_HDR_LEN];
		exp_len_nocrc = RTU_HDR_LEN + 1 + byte_count;
		if (len >= (exp_len_nocrc + RTU_CRC_LEN)) {
			exp_crc = crc16(data, exp_len_nocrc);
			osmo_store16be(exp_crc, &exp_crc);
			if (memcmp(&exp_crc, &data[exp_len_nocrc], RTU_CRC_LEN) == 0) {
				/* Its a response: */
				uint16_t * registers = (uint16_t*)&data[RTU_HDR_LEN + 1]; /* FIXME: copy to temp buffer to fix misalignment */
				*prim = osmo_modbus_makeprim_mult_hold_reg_resp(address, byte_count/2, registers);
				return exp_len_nocrc + RTU_CRC_LEN;
			}
		}
		/* try to decode Request */
		exp_len_nocrc = RTU_HDR_LEN + 2 + 2;
		if (len >= exp_len_nocrc + RTU_CRC_LEN) {
			exp_crc = crc16(data, exp_len_nocrc);
			osmo_store16be(exp_crc, &exp_crc);
			if (memcmp(&exp_crc, &data[exp_len_nocrc], RTU_CRC_LEN) == 0) {
				/* Its a response: */
				uint16_t first_reg, num_reg;
				first_reg = osmo_load16be(&data[RTU_HDR_LEN]);
				num_reg = osmo_load16be(&data[RTU_HDR_LEN + 2]);
				*prim = osmo_modbus_makeprim_mult_hold_reg_req(address, first_reg, num_reg);
				return exp_len_nocrc + RTU_CRC_LEN;
			}
		}
		/* Either CRC error or we miss data... */
		return -ENODATA;
	default:
		return -EINVAL;
	}
}

int rtu_read(struct osmo_modbus_conn_rtu* rtu)
{
	uint8_t *buf = msgb_data(rtu->rx_msg);
	int offset = msgb_length(rtu->rx_msg);
	int rc;

	LOGPRTU(rtu, DLMODBUS_RTU, LOGL_DEBUG, "Read cb (buf=%d)\n", msgb_tailroom(rtu->rx_msg));
	rc = read(rtu->ofd.fd, buf + offset, msgb_tailroom(rtu->rx_msg));
	if (rc < 0) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_ERROR, "read() failed %d: %s\n", rc, strerror(errno));
		return rc;
	} else if (rc == 0) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_NOTICE, "read() 0 bytes\n");
		return 0;
	}
	LOGPRTU(rtu, DLMODBUS_RTU, LOGL_DEBUG, "Received %d bytes: %s\n", rc, osmo_hexdump(buf + offset, rc));
	msgb_put(rtu->rx_msg, rc);
	LOGPRTU(rtu, DLMODBUS_RTU, LOGL_DEBUG, "Received total %d bytes: %s\n", rc, osmo_hexdump(buf, msgb_length(rtu->rx_msg)));
	osmo_fsm_inst_dispatch(rtu->fi, RTU_TRANSMIT_EV_CHAR_RECEIVED, NULL);
	return 0;
}

int rtu_write(struct osmo_modbus_conn_rtu* rtu)
{
	struct msgb *msg;
	int rc;

	LOGPRTU(rtu, DLMODBUS_RTU, LOGL_DEBUG, "Write cb!\n");

	rtu->ofd.when &= ~OSMO_FD_WRITE;

	if (!rtu->tx_msg) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_NOTICE, "Write cb but no Tx Msg!\n");
		return 0;
	}
	msg = rtu->tx_msg;
	rtu->tx_msg = NULL;

	LOGPRTU(rtu, DLMODBUS_RTU, LOGL_INFO, "Writing: %s\n", msgb_hexdump(msg));
	rc = write(rtu->ofd.fd, msgb_data(msg), msgb_length(msg));
	if (rc < 0) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_ERROR, "write() failed %d: %s\n", rc, strerror(errno));
	} else if (rc != msgb_length(msg)) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_ERROR, "Wrote only %d / %d bytes!\n", rc, msgb_length(msg));
	}
	msgb_free(msg);
	return 0;
}

static int rtu_ofd_cb(struct osmo_fd *ofd, unsigned int flags)
{
	struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu*)ofd->data;
	int rc = 0;

	if (flags & OSMO_FD_READ) {
		rc = rtu_read(rtu);
		if (rc == -EBADF)
			goto err_badfd;
	}

	/* FIXME: what to do here? Move to disconnected state?*/
	if (flags & OSMO_FD_EXCEPT) {
		/* FIXME: what to do here? Move to disconnected state?*/
		/* if (rc == -EBADF)
			goto err_badfd; */
	}

	if (flags & OSMO_FD_WRITE) {
		rc = rtu_write(rtu);
		if (rc == -EBADF)
			goto err_badfd;
	}

err_badfd:
	return rc;
}

static int osmo_modbus_conn_rtu_connect(struct osmo_modbus_conn* conn)
{
	struct osmo_modbus_conn_rtu* rtu = (struct osmo_modbus_conn_rtu*) conn->proto;
	speed_t speed;
	int fd;
	int flags;

	if (!rtu->dev_path || rtu->dev_path[0] == '\0')
		return -EINVAL;

	if (osmo_serial_speed_t(rtu->baudrate, &speed) < 0) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_ERROR, "Failed to get speed_t from baudrate\n");
		return -EINVAL;
	}

	fd = osmo_serial_init(rtu->dev_path, speed);
	if (fd < 0)
		return fd;

	osmo_fd_setup(&rtu->ofd, fd, OSMO_FD_READ, rtu_ofd_cb, rtu, 0);
	if (osmo_fd_register(&rtu->ofd) != 0) {
		LOGPRTU(rtu, DLMODBUS_RTU, LOGL_ERROR, "Failed to register the serial\n");
		return -EINVAL;
	}

	/* Set serial socket to non-blocking mode of operation */
	flags = fcntl(rtu->ofd.fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(rtu->ofd.fd, F_SETFL, flags);

	return osmo_fsm_inst_dispatch(rtu->fi, RTU_TRANSMIT_EV_START, NULL);
}

static bool osmo_modbus_conn_rtu_is_connected(struct osmo_modbus_conn* conn)
{
	struct osmo_modbus_conn_rtu* rtu = (struct osmo_modbus_conn_rtu*) conn->proto;
	return rtu->ofd.fd >= 0;
}

static int osmo_modbus_conn_rtu_tx_prim(struct osmo_modbus_conn* conn, struct osmo_modbus_prim *prim)
{
	struct osmo_modbus_conn_rtu* rtu = (struct osmo_modbus_conn_rtu*) conn->proto;

	OSMO_ASSERT(!rtu->tx_msg);
	rtu->tx_msg = prim2rtu(prim);

	return osmo_fsm_inst_dispatch(rtu->fi, RTU_TRANSMIT_EV_DEMAND_OF_EMISSION, NULL);
}

static void osmo_modbus_conn_rtu_free(struct osmo_modbus_conn* conn)
{
	struct osmo_modbus_conn_rtu* rtu = (struct osmo_modbus_conn_rtu*) conn->proto;

	osmo_fsm_inst_free(rtu->fi);
	rtu->fi = NULL;

	if (rtu->ofd.fd >= 0) {
		osmo_fd_unregister(&rtu->ofd);
		close(rtu->ofd.fd);
		rtu->ofd.fd = -1;
	}

	msgb_free(rtu->tx_msg);

	talloc_free(rtu);
}

static void recalc_baudrate_timers(struct osmo_modbus_conn_rtu* rtu)
{
	if (rtu->baudrate <= 19200) {
		osmo_tdef_set(rtu->T_defs, 15, rtu_chars2bits(1500000) / rtu->baudrate, OSMO_TDEF_US);
		osmo_tdef_set(rtu->T_defs, 35, rtu_chars2bits(3500000) / rtu->baudrate, OSMO_TDEF_US);
	} else {
		/* 2.5.1.1 MODBUS Message RTU Framing: Fixed values used for higher baudrates */
		osmo_tdef_set(rtu->T_defs, 15, 750, OSMO_TDEF_US);
		osmo_tdef_set(rtu->T_defs, 35, 1750, OSMO_TDEF_US);
	}
}

static void update_fi_name(struct osmo_modbus_conn_rtu* rtu)
{
	osmo_fsm_inst_update_id_f_sanitize(rtu->fi, '-', "%s_%" PRIu16,
					    rtu->dev_path ? : "unknown",
					    rtu->conn->address);
}

struct osmo_modbus_conn_rtu* osmo_modbus_conn_rtu_alloc(struct osmo_modbus_conn* conn)
{
	struct osmo_modbus_conn_rtu* rtu = talloc_zero(conn, struct osmo_modbus_conn_rtu);
	rtu->conn = conn;
	rtu->baudrate = 9600;
	rtu->ofd.fd = -1;
	rtu->rx_msg = modbus_rtu_msgb_alloc();
	rtu->T_defs = talloc_zero_size(rtu, sizeof(g_rtu_tdefs));
	memcpy(rtu->T_defs, g_rtu_tdefs, sizeof(g_rtu_tdefs));
	osmo_tdefs_reset(rtu->T_defs);
	recalc_baudrate_timers(rtu);

	rtu_transmit_fsm.log_subsys = DLMODBUS_RTU; /* Update after app set the correct value */
	rtu->fi = osmo_fsm_inst_alloc(&rtu_transmit_fsm, rtu, rtu, LOGL_INFO, NULL);

	conn->proto_ops.connect = osmo_modbus_conn_rtu_connect;
	conn->proto_ops.is_connected = osmo_modbus_conn_rtu_is_connected;
	conn->proto_ops.tx_prim = osmo_modbus_conn_rtu_tx_prim;
	conn->proto_ops.free = osmo_modbus_conn_rtu_free;

	return rtu;
}

int osmo_modbus_conn_rtu_set_device(struct osmo_modbus_conn_rtu* rtu, const char* serial_dev)
{
	osmo_talloc_replace_string(rtu, &rtu->dev_path, serial_dev);
	update_fi_name(rtu);
	return 0;
}

const char *osmo_modbus_conn_rtu_get_device(const struct osmo_modbus_conn_rtu* rtu)
{
	return rtu->dev_path;
}

int osmo_modbus_conn_rtu_set_baudrate(struct osmo_modbus_conn_rtu* rtu, unsigned baudrate)
{
	speed_t speed;
	if (osmo_serial_speed_t(baudrate, &speed) < 0)
		return -EINVAL;

	rtu->baudrate = baudrate;
	if (osmo_modbus_conn_rtu_is_connected(rtu->conn))
		return osmo_serial_set_baudrate(rtu->ofd.fd, speed);
	recalc_baudrate_timers(rtu);
	update_fi_name(rtu);
	return 0;
}

unsigned osmo_modbus_conn_rtu_get_baudrate(const struct osmo_modbus_conn_rtu* rtu)
{
	return rtu->baudrate;
}
