/*! \file conn_master_fsm.c
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

#include <osmocom/core/fsm.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/select.h>

#include <osmocom/modbus/modbus.h>

#include "modbus_internal.h"
#include "conn_fsm.h"

#define X(x)	(1 << (x))

static const struct value_string conn_master_event_names[] = {
	{ CONN_EV_CONNECT, 		"Connect" },
	{ CONN_EV_SUBMIT_PRIM,		"SubmitPrim" },
	{ CONN_EV_RECV_PRIM,		"RxPrim" },
	{ 0, NULL }
};

static const struct osmo_tdef_state_timeout conn_master_fsm_timeouts[32] = {
	[CONN_MASTER_ST_DISCONNECTED] = {},
	[CONN_MASTER_ST_IDLE] = {},
	[CONN_MASTER_ST_WAIT_TURNAROUND_DELAY] = { .T = OSMO_MODBUS_TO_TURNAROUND },
	[CONN_MASTER_ST_WAIT_REPLY] = { .T = OSMO_MODBUS_TO_NORESPONSE },
};

/* Transition to a state, using the T timer defined in assignment_fsm_timeouts.
 * The actual timeout value is in turn obtained from conn->T_defs.
 * Assumes local variable fi exists. */
#define conn_master_fsm_state_chg(fi, state) \
	osmo_tdef_fsm_inst_state_chg(fi, state, \
				     conn_master_fsm_timeouts, \
				     ((struct osmo_modbus_conn*)(fi->priv))->T_defs, \
				     -1)

static void conn_master_fsm_st_disconnected_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void conn_master_fsm_st_disconnected(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	bool *connected;
	int rc;

	switch (event) {
	case CONN_EV_CONNECT:
		connected = (bool*)data;
		if ((rc = conn->proto_ops.connect(conn)) == 0) {
			*connected = true;
			conn_master_fsm_state_chg(fi, CONN_MASTER_ST_IDLE);
		} else {
			*connected = false;
		}
		break;
	case CONN_EV_SUBMIT_PRIM:
		/* Do nothing, conn enqueued the message */
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void conn_master_fsm_st_idle_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	/* TODO: once we support broadcast messages, check msg and do that transition */
	if (!llist_empty(&conn->msg_queue))
		conn_master_fsm_state_chg(fi, CONN_MASTER_ST_WAIT_REPLY);

}

static void conn_master_fsm_st_idle(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	//struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	switch (event) {
	case CONN_EV_SUBMIT_PRIM:
		/* TODO: once we support broadcast messages, check msg and do that transition */
		conn_master_fsm_state_chg(fi, CONN_MASTER_ST_WAIT_REPLY);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void conn_master_fsm_st_wait_turnaround_delay_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	/* TODO: implement */
}

static void conn_master_fsm_st_wait_turnaround_delay(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	/* TODO: implement */
}

static void conn_master_fsm_st_wait_reply_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	struct msgb *msg;
	struct osmo_modbus_prim *prim;

	if (llist_empty(&conn->msg_queue)) {
		LOGPFSML(fi, LOGL_INFO, "Write queue is empty!\n");
		OSMO_ASSERT(0);
	}

	msg = msgb_dequeue(&conn->msg_queue);
	prim = (struct osmo_modbus_prim *)msgb_data(msg);
	conn->master.req_for_addr = prim->address;
	conn->proto_ops.tx_prim(conn, prim);
	msgb_free(msg);
}

static void conn_master_fsm_st_wait_reply(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	struct osmo_modbus_prim *prim;

	switch (event) {
		case CONN_EV_SUBMIT_PRIM:
			/* Do nothing, conn enqueued the message */
			break;
		case CONN_EV_RECV_PRIM:
			prim = (struct osmo_modbus_prim *)data;
			/* TODO: check if addr is for us... */
			/* TODO: check if msg received is a reply for our last request... */
			if (conn->prim_cb)
				conn->prim_cb(conn, prim, conn->prim_cb_ctx);
			else
				msgb_free(prim->oph.msg);
			conn_master_fsm_state_chg(fi, CONN_MASTER_ST_IDLE);
			break;
		default:
			OSMO_ASSERT(0);
	}
}

static const struct osmo_fsm_state conn_master_states[] = {
	[CONN_MASTER_ST_DISCONNECTED]= {
		.in_event_mask = X(CONN_EV_CONNECT) |
				 X(CONN_EV_SUBMIT_PRIM),
		.out_state_mask = X(CONN_MASTER_ST_IDLE),
		.name = "DISCONNECTED",
		.action = conn_master_fsm_st_disconnected,
		.onenter = conn_master_fsm_st_disconnected_onenter,
	},
	[CONN_MASTER_ST_IDLE] = {
		.in_event_mask = X(CONN_EV_SUBMIT_PRIM),
		.out_state_mask = X(CONN_MASTER_ST_WAIT_TURNAROUND_DELAY) |
				  X(CONN_MASTER_ST_WAIT_REPLY),
		.name = "IDLE",
		.action = conn_master_fsm_st_idle,
		.onenter = conn_master_fsm_st_idle_onenter,
	},
	[CONN_MASTER_ST_WAIT_TURNAROUND_DELAY] = {
		.in_event_mask = X(CONN_EV_SUBMIT_PRIM),
		.out_state_mask = X(CONN_MASTER_ST_IDLE),
		.name = "WAIT_TURNAROUND_DELAY",
		.action = conn_master_fsm_st_wait_turnaround_delay,
		.onenter = conn_master_fsm_st_wait_turnaround_delay_onenter,
	},
	[CONN_MASTER_ST_WAIT_REPLY] = {
		.in_event_mask = X(CONN_EV_SUBMIT_PRIM) |
				 X(CONN_EV_RECV_PRIM),
		.out_state_mask = X(CONN_MASTER_ST_IDLE),
		.name = "WAIT_REPLY",
		.action = conn_master_fsm_st_wait_reply,
		.onenter = conn_master_fsm_st_wait_reply_onenter,
	},
};

static int conn_master_fsm_timer_cb(struct osmo_fsm_inst *fi)
{
	struct osmo_modbus_conn *conn = (struct osmo_modbus_conn*)fi->priv;
	struct osmo_modbus_prim *prim;

	switch (fi->T) {
	case OSMO_MODBUS_TO_TURNAROUND:
		conn_master_fsm_state_chg(fi, CONN_MASTER_ST_IDLE);
		break;
	case OSMO_MODBUS_TO_NORESPONSE:
		prim = osmo_modbus_makeprim_timeout_resp(conn->master.req_for_addr);
		if (conn->prim_cb)
			conn->prim_cb(conn, prim, conn->prim_cb_ctx);
		else
			msgb_free(prim->oph.msg);
		conn_master_fsm_state_chg(fi, CONN_MASTER_ST_IDLE);
		break;
	}
	return 0;
}

struct osmo_fsm conn_master_fsm = {
	.name = "conn_master",
	.states = conn_master_states,
	.num_states = ARRAY_SIZE(conn_master_states),
	.timer_cb = conn_master_fsm_timer_cb,
	.log_subsys = DLMODBUS_OFFSET,
	.event_names = conn_master_event_names,
	//.cleanup = conn_master_fsm_cleanup,
};

static __attribute__((constructor)) void conn_master_fsm_init(void)
{
	OSMO_ASSERT(osmo_fsm_register(&conn_master_fsm) == 0);
}
