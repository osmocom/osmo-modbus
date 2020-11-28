/*! \file modbus_rtu.h
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

#include <osmocom/core/select.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/tdef.h>

#include <osmocom/modbus/modbus.h>

struct osmo_modbus_conn_rtu;

struct osmo_modbus_conn_rtu* osmo_modbus_conn_rtu_alloc(struct osmo_modbus_conn* conn);

int osmo_modbus_conn_rtu_set_device(struct osmo_modbus_conn_rtu* rtu, const char* serial_dev);
const char *osmo_modbus_conn_rtu_get_device(const struct osmo_modbus_conn_rtu* rtu);
int osmo_modbus_conn_rtu_set_baudrate(struct osmo_modbus_conn_rtu* rtu, unsigned baudrate);
unsigned osmo_modbus_conn_rtu_get_baudrate(const struct osmo_modbus_conn_rtu* rtu);
