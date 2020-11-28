/*! \file prim.c
 * modbus primitives */
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

#include <unistd.h>
#include <inttypes.h>

#include <osmocom/core/msgb.h>

#include <osmocom/modbus/modbus_prim.h>
#include <osmocom/modbus/modbus.h>

#define MODBUS_SAP 0

static struct msgb *modbus_prim_msgb_alloc(const char* desc)
{
	return msgb_alloc(sizeof(struct osmo_modbus_prim), desc);
}

const struct value_string osmo_modbus_prim_type_names[] = {
	{ OSMO_MODBUS_PRIM_RESPONSE_TIMEOUT, 	"Response Timeout" },
	{ OSMO_MODBUS_PRIM_N_MULT_HOLD_REG,	"N Multiple Holding Registers" },
	{ 0, NULL }
};

struct osmo_modbus_prim *osmo_modbus_makeprim_timeout_resp(uint16_t address)
{
	struct msgb *msg = modbus_prim_msgb_alloc(__func__);
	struct osmo_modbus_prim *prim;

	prim = (struct osmo_modbus_prim *) msgb_put(msg, sizeof(*prim));
	osmo_prim_init(&prim->oph, MODBUS_SAP,
			OSMO_MODBUS_PRIM_RESPONSE_TIMEOUT,
			PRIM_OP_INDICATION, msg);
	prim->address = address;
	return prim;
}

struct osmo_modbus_prim *osmo_modbus_makeprim_mult_hold_reg_req(uint16_t address, uint16_t first_reg, uint16_t num_reg)
{
	struct msgb *msg = modbus_prim_msgb_alloc(__func__);
	struct osmo_modbus_prim *prim;
	struct osmo_modbus_read_mult_hold_reg_req_param *param;

	prim = (struct osmo_modbus_prim *) msgb_put(msg, sizeof(*prim));
	osmo_prim_init(&prim->oph, MODBUS_SAP,
			OSMO_MODBUS_PRIM_N_MULT_HOLD_REG,
			PRIM_OP_REQUEST, msg);
	prim->address = address;
	param = &prim->u.read_mult_hold_reg_req;
	param->first_reg = first_reg;
	param->num_reg = num_reg;
	return prim;
}

struct osmo_modbus_prim *osmo_modbus_makeprim_mult_hold_reg_resp(uint16_t address, uint8_t num_reg, uint16_t *registers)
{
	struct msgb *msg = modbus_prim_msgb_alloc(__func__);
	struct osmo_modbus_prim *prim;
	struct osmo_modbus_read_mult_hold_reg_resp_param *param;

	prim = (struct osmo_modbus_prim *) msgb_put(msg, sizeof(*prim));
	osmo_prim_init(&prim->oph, MODBUS_SAP,
			OSMO_MODBUS_PRIM_N_MULT_HOLD_REG,
			PRIM_OP_RESPONSE, msg);
	prim->address = address;
	param = &prim->u.read_mult_hold_reg_resp;
	param->num_reg = num_reg;
	memcpy(param->registers, registers, num_reg * sizeof(uint16_t));
	return prim;
}
