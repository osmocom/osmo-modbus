/*! \file modbus.h
 * Osmocom modbus */
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

#include <osmocom/core/logging.h>

#include <osmocom/modbus/modbus_prim.h>
#include <osmocom/modbus/modbus_conn.h>
#include <osmocom/modbus/modbus_rtu.h>

extern int DLMODBUS;
extern int DLMODBUS_RTU;
/* Overwrite with whatever number is wanted by the APP */
unsigned int osmo_modbus_set_logging_category_offset(int offset);

enum osmo_modbus_function_code {
	OSMO_MODBUS_FUNC_READ_MULT_HOLD_REG = 0x03,
};
