/*! \file modbus_prim.h
 * Osmocom modbus primitives */
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

#pragma once

#include <osmocom/core/prim.h>
/*! \brief Modbus primitives */
enum osmo_modbus_prim_type {
	OSMO_MODBUS_PRIM_RESPONSE_TIMEOUT,
	OSMO_MODBUS_PRIM_N_MULT_HOLD_REG,
};
extern const struct value_string osmo_modbus_prim_type_names[];

/* OSMO_MODBUS_PRIM_N_MULT_HOLD_REG */
struct osmo_modbus_read_mult_hold_reg_req_param {
	uint16_t first_reg;
	uint16_t num_reg;
	/* user data */
};

/* OSMO_MODBUS_PRIM_N_MULT_HOLD_RESP */
struct osmo_modbus_read_mult_hold_reg_resp_param {
	uint16_t num_reg;
	uint16_t registers[125];
	/* user data */
};

struct osmo_modbus_prim {
	struct osmo_prim_hdr oph;
	uint16_t address;
	union {
		struct osmo_modbus_read_mult_hold_reg_req_param read_mult_hold_reg_req;
		struct osmo_modbus_read_mult_hold_reg_resp_param read_mult_hold_reg_resp;
	} u;
};

struct osmo_modbus_prim *osmo_modbus_makeprim_timeout_resp(uint16_t address);
struct osmo_modbus_prim *osmo_modbus_makeprim_mult_hold_reg_req(uint16_t address, uint16_t first_reg, uint16_t num_reg);
struct osmo_modbus_prim *osmo_modbus_makeprim_mult_hold_reg_resp(uint16_t address, uint8_t num_reg, uint16_t *registers);
