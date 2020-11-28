/*! \file modbus_conn.h
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
#include <osmocom/modbus/modbus_prim.h>

struct osmo_modbus_conn_rtu;

enum osmo_modbus_proto_type {
	OSMO_MODBUS_PROTO_RTU,
};

enum osmo_modbus_conn_role {
	OSMO_MODBUS_ROLE_MASTER,
	OSMO_MODBUS_ROLE_SLAVE,
};

enum osmo_modbus_conn_timeout {
	OSMO_MODBUS_TO_TURNAROUND = 1,
	OSMO_MODBUS_TO_NORESPONSE = 2,
};

struct osmo_modbus_conn;
typedef int (*osmo_modbus_prim_cb)(struct osmo_modbus_conn *conn, struct osmo_modbus_prim *prim, void *ctx);

struct osmo_modbus_conn* osmo_modbus_conn_alloc(void *tall_ctx,
						enum osmo_modbus_conn_role role,
						enum osmo_modbus_proto_type type);
void osmo_modbus_conn_free(struct osmo_modbus_conn* conn);

int osmo_modbus_conn_connect(struct osmo_modbus_conn* conn);
bool osmo_modbus_conn_is_connected(struct osmo_modbus_conn* conn);
int osmo_modbus_conn_set_timeout(struct osmo_modbus_conn* conn,
				enum osmo_modbus_conn_timeout to_type,
				unsigned long val);
unsigned long osmo_modbus_conn_get_timeout(const struct osmo_modbus_conn* conn,
					  enum osmo_modbus_conn_timeout to_type);
int osmo_modbus_conn_set_address(struct osmo_modbus_conn* conn, uint16_t address);
uint16_t osmo_modbus_conn_get_address(const struct osmo_modbus_conn* conn);
void osmo_modbus_conn_set_prim_cb(struct osmo_modbus_conn* conn,
				  osmo_modbus_prim_cb prim_cb, void *ctx);
int osmo_modbus_conn_submit_prim(struct osmo_modbus_conn* conn,
				 struct osmo_modbus_prim *prim);
int osmo_modbus_conn_set_monitor_mode(struct osmo_modbus_conn* conn, bool enable);

struct osmo_modbus_conn_rtu *osmo_modbus_conn_get_rtu(struct osmo_modbus_conn *conn);
