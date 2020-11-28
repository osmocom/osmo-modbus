/*! \file conn_slave_fsm.c
 * FSM for "Figure 14: RTU transmission mode state diagram" */
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
#include <stdbool.h>
#include <inttypes.h>

#include <osmocom/core/fsm.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/select.h>

#include <osmocom/modbus/modbus.h>

#include "modbus_internal.h"
#include "conn_fsm.h"

#define X(x)	(1 << (x))

static const struct value_string conn_slave_event_names[] = {
	{ CONN_EV_CONNECT, 		"Connect" },
	{ CONN_EV_SUBMIT_PRIM,		"SubmitPrim" },
	{ CONN_EV_RECV_PRIM,		"RxPrim" },
	{ 0, NULL }
};

#define conn_slave_fsm_state_chg(fi, state) \
	osmo_fsm_inst_state_chg(fi, state, 0, 0)

static void conn_slave_fsm_st_disconnected_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void conn_slave_fsm_st_disconnected(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	bool *connected;
	int rc;

	switch (event) {
	case CONN_EV_CONNECT:
		connected = (bool*)data;
		if ((rc = conn->proto_ops.connect(conn)) == 0) {
			*connected = true;
			conn_slave_fsm_state_chg(fi, CONN_SLAVE_ST_IDLE);
		} else {
			*connected = false;
		}
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void conn_slave_fsm_st_idle_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void conn_slave_fsm_st_idle(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	struct osmo_modbus_prim *prim;
	switch (event) {
	case CONN_EV_RECV_PRIM:
		prim = (struct osmo_modbus_prim *)data;
		/* check if addr is for us... */
		if (!conn->prim_cb || conn->address != prim->address) {
			LOGPFSML(fi, LOGL_DEBUG, "primitive not for us (addr=%" PRIu16 "), ignoring\n",
				 prim->address);
			/* We still want to deliver the prim if monitor mode
			   enabled, but not wait for a primback from upper
			   layers */
			if (conn->prim_cb && conn->slave.monitor)
				conn->prim_cb(conn, prim, conn->prim_cb_ctx);
			else
				msgb_free(prim->oph.msg);
			return;
		}
		conn_slave_fsm_state_chg(fi, CONN_SLAVE_ST_CHECK_REQUEST);
		/* Ideally this should go into st_check_request_onenter but then
		 * we need to store the prim pointer somewhere... */
		conn->prim_cb(conn, prim, conn->prim_cb_ctx);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void conn_slave_fsm_st_check_request_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void conn_slave_fsm_st_check_request(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	struct msgb *msg;
	struct osmo_modbus_prim *prim;

	switch (event) {
	case CONN_EV_SUBMIT_PRIM:
		if (llist_empty(&conn->msg_queue)) {
			LOGPFSML(fi, LOGL_INFO, "Write queue is empty!\n");
			OSMO_ASSERT(0);
		}

		msg = msgb_dequeue(&conn->msg_queue);
		prim = (struct osmo_modbus_prim *)msgb_data(msg);
		conn->proto_ops.tx_prim(conn, prim);
		msgb_free(msg);
		conn_slave_fsm_state_chg(fi, CONN_SLAVE_ST_IDLE);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static const struct osmo_fsm_state conn_slave_states[] = {
	[CONN_SLAVE_ST_DISCONNECTED]= {
		.in_event_mask = X(CONN_EV_CONNECT),
		.out_state_mask = X(CONN_SLAVE_ST_IDLE),
		.name = "DISCONNECTED",
		.action = conn_slave_fsm_st_disconnected,
		.onenter = conn_slave_fsm_st_disconnected_onenter,
	},
	[CONN_SLAVE_ST_IDLE] = {
		.in_event_mask = X(CONN_EV_RECV_PRIM),
		.out_state_mask = X(CONN_SLAVE_ST_CHECK_REQUEST),
		.name = "IDLE",
		.action = conn_slave_fsm_st_idle,
		.onenter = conn_slave_fsm_st_idle_onenter,
	},
	[CONN_SLAVE_ST_CHECK_REQUEST] = {
		.in_event_mask = X(CONN_EV_SUBMIT_PRIM),
		.out_state_mask = X(CONN_SLAVE_ST_IDLE),
		.name = "CHECK_REQUEST",
		.action = conn_slave_fsm_st_check_request,
		.onenter = conn_slave_fsm_st_check_request_onenter,
	},
};

struct osmo_fsm conn_slave_fsm = {
	.name = "conn_slave",
	.states = conn_slave_states,
	.num_states = ARRAY_SIZE(conn_slave_states),
	.log_subsys = DLMODBUS_OFFSET,
	.event_names = conn_slave_event_names,
	//.cleanup = conn_slave_fsm_cleanup,
};

static __attribute__((constructor)) void conn_slave_fsm_init(void)
{
	OSMO_ASSERT(osmo_fsm_register(&conn_slave_fsm) == 0);
}
