/*! \file conn.c
 * modbus connection */
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

#include <errno.h>
#include <inttypes.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>

#include <osmocom/modbus/modbus.h>
#include <osmocom/modbus/modbus_prim.h>
#include <osmocom/modbus/modbus_rtu.h>

#include "modbus_internal.h"
#include "conn_fsm.h"

#define LOGPCONN(conn, subsys, level, fmt, args ...) \
	LOGP(subsys, level, "(addr=%u) " fmt, (conn)->address, ## args)

int DLMODBUS = DLMODBUS_OFFSET;
int DLMODBUS_RTU = DLMODBUS_RTU_OFFSET;
unsigned int osmo_modbus_set_logging_category_offset(int offset)
{
	DLMODBUS = offset + DLMODBUS_OFFSET;
	DLMODBUS_RTU = offset + DLMODBUS_RTU_OFFSET;
	return 2;
}

struct osmo_tdef g_conn_tdefs[] = {
	{ .T=OSMO_MODBUS_TO_TURNAROUND, .default_val=100, .unit = OSMO_TDEF_MS, .desc="Turnaround Delay Expiration Timeout" },
	{ .T=OSMO_MODBUS_TO_NORESPONSE, .default_val=200, .unit = OSMO_TDEF_MS, .desc="Response Timeout" },
	{}
};

static void update_fi_name(struct osmo_modbus_conn* conn)
{
	if (conn->role == OSMO_MODBUS_ROLE_SLAVE) {
		osmo_fsm_inst_update_id_f_sanitize(conn->fi, '-', "addr-%" PRIu16,
						   conn->address);
	}
}

struct osmo_modbus_conn* osmo_modbus_conn_alloc(void *tall_ctx,
						enum osmo_modbus_conn_role role,
						enum osmo_modbus_proto_type type)
{
	struct osmo_modbus_conn* conn = talloc_zero(tall_ctx, struct osmo_modbus_conn);
	conn->role = role;
	conn->proto_type = type;
	INIT_LLIST_HEAD(&conn->msg_queue);

	switch (type) {
	case OSMO_MODBUS_PROTO_RTU:
		conn->proto = (void *)osmo_modbus_conn_rtu_alloc(conn);
		break;
	default:
		goto err;
	}

	if (!conn->proto)
		goto err;

	conn->T_defs = talloc_zero_size(conn, sizeof(g_conn_tdefs));
	memcpy(conn->T_defs, g_conn_tdefs, sizeof(g_conn_tdefs));
	osmo_tdefs_reset(conn->T_defs);

	if (conn->role == OSMO_MODBUS_ROLE_MASTER) {
		conn->address = 0x00;
		conn_master_fsm.log_subsys = DLMODBUS; /* Update after app set the correct value */
		conn->fi = osmo_fsm_inst_alloc(&conn_master_fsm, conn, conn, LOGL_INFO, NULL);
	} else {
		conn->address = 0x01;
		conn_master_fsm.log_subsys = DLMODBUS; /* Update after app set the correct value */
		conn->fi = osmo_fsm_inst_alloc(&conn_slave_fsm, conn, conn, LOGL_INFO, NULL);
		osmo_fsm_inst_update_id_f_sanitize(conn->fi, '-', "addr-%" PRIu16,
						   conn->address);
	}

	return conn;
err:
	talloc_free(conn);
	return NULL;
}

void osmo_modbus_conn_free(struct osmo_modbus_conn* conn)
{
	if (conn->proto_ops.free)
		conn->proto_ops.free(conn);
	conn->proto = NULL;

	osmo_fsm_inst_free(conn->fi);
	conn->fi = NULL;

	while (!llist_empty(&conn->msg_queue)) {
		struct msgb *msg = msgb_dequeue(&conn->msg_queue);
		msgb_free(msg);
	}

	talloc_free(conn);
}

int osmo_modbus_conn_connect(struct osmo_modbus_conn* conn)
{
	LOGPCONN(conn, DLMODBUS, LOGL_INFO, "Connecting...\n");
	bool connected = false;
	int rc;

	rc = osmo_fsm_inst_dispatch(conn->fi, CONN_EV_CONNECT, &connected);
	if (rc)
		return rc;
	return connected ? 0 : -ENOTCONN;
}

bool osmo_modbus_conn_is_connected(struct osmo_modbus_conn* conn)
{
	LOGPCONN(conn, DLMODBUS, LOGL_INFO, "Connecting...\n");
	if ((conn->role == OSMO_MODBUS_ROLE_MASTER && conn->fi->state == CONN_MASTER_ST_DISCONNECTED) ||
	    (conn->role == OSMO_MODBUS_ROLE_SLAVE && conn->fi->state == CONN_SLAVE_ST_DISCONNECTED))
		return false;
	return conn->proto_ops.is_connected(conn);
}

int osmo_modbus_conn_set_address(struct osmo_modbus_conn* conn, uint16_t address)
{
	/*TODO: for RTU it's only 1 byte, check that */
	conn->address = address;
	update_fi_name(conn);
	return 0;
}

uint16_t osmo_modbus_conn_get_address(const struct osmo_modbus_conn* conn)
{
	return conn->address;
}

int osmo_modbus_conn_set_timeout(struct osmo_modbus_conn* conn,
				enum osmo_modbus_conn_timeout to_type,
				unsigned long val)
{
	return osmo_tdef_set(conn->T_defs, to_type, val, OSMO_TDEF_MS);
}

unsigned long osmo_modbus_conn_get_timeout(const struct osmo_modbus_conn* conn,
					  enum osmo_modbus_conn_timeout to_type)
{
	return osmo_tdef_get(conn->T_defs, to_type, OSMO_TDEF_MS, (unsigned long)-1);
}

void osmo_modbus_conn_set_prim_cb(struct osmo_modbus_conn* conn,
				osmo_modbus_prim_cb prim_cb, void *ctx)
{
	conn->prim_cb = prim_cb;
	conn->prim_cb_ctx = ctx;
}

int osmo_modbus_conn_submit_prim(struct osmo_modbus_conn* conn,
				 struct osmo_modbus_prim *prim)
{
	int rc;

	LOGPCONN(conn, DLMODBUS, LOGL_INFO, "Submitting prim operation '%s' on primitive '%s'\n",
		 get_value_string(osmo_prim_op_names, prim->oph.operation),
		 get_value_string(osmo_modbus_prim_type_names, prim->oph.primitive));
	if ((conn->role == OSMO_MODBUS_ROLE_MASTER && prim->oph.operation != PRIM_OP_REQUEST) ||
	    (conn->role == OSMO_MODBUS_ROLE_SLAVE && prim->oph.operation != PRIM_OP_RESPONSE)) {
		LOGPCONN(conn, DLMODBUS, LOGL_INFO, "Primitive %s not possible in role %d\n",
			 get_value_string(osmo_prim_op_names, prim->oph.operation), conn->role);
		msgb_free(prim->oph.msg);
		return -EINVAL;
	}

	msgb_enqueue(&conn->msg_queue, prim->oph.msg);
	rc = osmo_fsm_inst_dispatch(conn->fi, CONN_EV_SUBMIT_PRIM, NULL);
	return rc;
}

int osmo_modbus_conn_set_monitor_mode(struct osmo_modbus_conn* conn, bool enable)
{
	if (conn->role == OSMO_MODBUS_ROLE_MASTER)
		return -EINVAL;
	conn->slave.monitor = enable;
	return 0;
}

struct osmo_modbus_conn_rtu *osmo_modbus_conn_get_rtu(struct osmo_modbus_conn *conn)
{
	switch (conn->proto_type) {
	case OSMO_MODBUS_PROTO_RTU:
		return (struct osmo_modbus_conn_rtu *)conn->proto;
	default:
		OSMO_ASSERT(0);
	}
}

void osmo_modbus_conn_rx_prim(struct osmo_modbus_conn* conn, struct osmo_modbus_prim *prim)
{
	int rc;
	LOGPCONN(conn, DLMODBUS, LOGL_INFO, "Received primitive operation '%s' on primitive '%s' on addr %" PRIu16 "\n",
		 get_value_string(osmo_prim_op_names, prim->oph.operation),
		 get_value_string(osmo_modbus_prim_type_names, prim->oph.primitive),
	 	 prim->address);
	rc = osmo_fsm_inst_dispatch(conn->fi, CONN_EV_RECV_PRIM, prim);
	if (rc) {
		msgb_free(prim->oph.msg);
	}
}
