/*! \file rtu_transmit_fsm.c
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

#include <osmocom/core/fsm.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/select.h>

#include <osmocom/modbus/modbus.h>

#include "modbus_internal.h"
#include "rtu_transmit_fsm.h"
#include "rtu_internal.h"

#define X(x)	(1 << (x))

static const struct value_string rtu_transmit_event_names[] = {
	{ RTU_TRANSMIT_EV_START, 		"Start" },
	{ RTU_TRANSMIT_EV_T15_TIMEOUT,		"T1.5 Timeout" },
	{ RTU_TRANSMIT_EV_T35_TIMEOUT,		"T3.5 Timeout" },
	{ RTU_TRANSMIT_EV_CHAR_RECEIVED,	"CharReceived" },
	{ RTU_TRANSMIT_EV_DEMAND_OF_EMISSION,	"DemandOfEmission" },
	{ 0, NULL }
};

static const struct osmo_tdef_state_timeout rtu_transmit_fsm_timeouts[32] = {
	[RTU_TRANSMIT_ST_INITIAL] = { .T=35 /* actually armed during EV START */ },
	[RTU_TRANSMIT_ST_IDLE] = { },
	[RTU_TRANSMIT_ST_EMISSION] = { /* dynamic */ },
	[RTU_TRANSMIT_ST_RECEPTION] = { .T=15 },
	[RTU_TRANSMIT_ST_CTRL_WAIT] = { /* dynamic */ },
};

/* Transition to a state, using the T timer defined in assignment_fsm_timeouts.
 * The actual timeout value is in turn obtained from rtu->T_defs.
 * Assumes local variable fi exists. */
#define rtu_transmit_fsm_state_chg(fi, state) \
	osmo_tdef_fsm_inst_state_chg(fi, state, \
				     rtu_transmit_fsm_timeouts, \
				     ((struct osmo_modbus_conn_rtu*)(fi->priv))->T_defs, \
				     -1)

static void rearm_timer_with_factor(struct osmo_fsm_inst *fi, int T, long factor_us) {
	struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	unsigned long timeout_us = osmo_tdef_get(rtu->T_defs, T, OSMO_TDEF_US, -1);
	timeout_us += factor_us;
	fi->T = T;
	LOGPFSML(fi, LOGL_DEBUG, "Rearm T%d {%ld, %ld} (%ld)\n", T, timeout_us / 1000000, timeout_us % 1000000, factor_us);
	osmo_timer_schedule(&fi->timer, timeout_us / 1000000, timeout_us % 1000000);
}

static void rearm_timer(struct osmo_fsm_inst *fi, int T) {
	rearm_timer_with_factor(fi, T, 0);
}

static void rtu_transmit_fsm_st_initial_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void rtu_transmit_fsm_st_initial(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	//struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	switch (event) {
	case RTU_TRANSMIT_EV_START:
		rearm_timer(fi, 35);
		break;
	case RTU_TRANSMIT_EV_CHAR_RECEIVED:
		rearm_timer(fi, 35);
		break;
	case RTU_TRANSMIT_EV_T35_TIMEOUT:
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_IDLE);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void rtu_transmit_fsm_st_idle_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void rtu_transmit_fsm_st_idle(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	//struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	switch (event) {
	case RTU_TRANSMIT_EV_DEMAND_OF_EMISSION:
		//rtu->tx_msg = (struct msgb *)data;
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_EMISSION);
		break;
	case RTU_TRANSMIT_EV_CHAR_RECEIVED:
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_RECEPTION);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void rtu_transmit_fsm_st_emission_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	size_t char_len;

	/* Simply enable the write flag, fd will tell when we can send */
	rtu->ofd.when |= OSMO_FD_WRITE;

	char_len = msgb_length(rtu->tx_msg);
	long time_factor_us = rtu_chars2bits(char_len) * 1000000 / rtu->baudrate;
	rearm_timer_with_factor(fi, 35, time_factor_us);
}

static void rtu_transmit_fsm_st_emission(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	switch (event) {
	case RTU_TRANSMIT_EV_T35_TIMEOUT:
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_IDLE);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void rtu_transmit_fsm_st_reception_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
}

static void rtu_transmit_fsm_st_reception(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	switch (event) {
	case RTU_TRANSMIT_EV_CHAR_RECEIVED:
		rearm_timer(fi, 15);
		break;
	case RTU_TRANSMIT_EV_T15_TIMEOUT:
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_CTRL_WAIT);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void rtu_transmit_fsm_st_ctrlwait_onenter(struct osmo_fsm_inst *fi, uint32_t prev_state)
{
	struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	uint16_t exp_crc, got_crc;
	long time_factor_us;
	uint8_t *data;
	unsigned int len;

	/* T1.5 already triggered, which means to reach T3.5 we have to wait for
	 * "T2" aka 2 character timers */
	time_factor_us = -1 * (rtu_chars2bits(1500000) / rtu->baudrate);
	rearm_timer_with_factor(fi, 35, time_factor_us);

	OSMO_ASSERT(rtu->rx_msg);
	data = msgb_data(rtu->rx_msg);
	len = msgb_length(rtu->rx_msg);

	if (len < sizeof(uint16_t)) {
		LOGPFSML(fi, LOGL_INFO, "Cannot generate CRC, rx msg len: %d\n", len);
		rtu->rx_msg_ok = false;
		return;
	}

	/* Mark NOK if CRC fails */
	memcpy(&got_crc, &data[len - sizeof(uint16_t)], sizeof(uint16_t));
	exp_crc = crc16(data, len - sizeof(uint16_t));
	osmo_store16be(exp_crc, &exp_crc);
	rtu->rx_msg_ok = got_crc == exp_crc;
	LOGPFSML(fi, LOGL_DEBUG, "CRC: got=0x08%x vs exp=0x08%x: %s\n", got_crc, exp_crc,
		 rtu->rx_msg_ok ? "OK" : "NOK");
}

static void rtu_transmit_fsm_st_ctrlwait(struct osmo_fsm_inst *fi, uint32_t event, void *data)
{
	struct osmo_modbus_conn_rtu *rtu = (struct osmo_modbus_conn_rtu *)fi->priv;
	struct osmo_modbus_prim *prim = NULL;
	int rc;

	switch (event) {
	case RTU_TRANSMIT_EV_CHAR_RECEIVED:
		LOGP(DLMODBUS_RTU, LOGL_ERROR, "Char received while in state CTRL WAIT, marking rx msg as NOK\n");
		rtu->rx_msg_ok = false;
		break;
	case RTU_TRANSMIT_EV_T35_TIMEOUT:
		/* TODO: submit OK rx_msg to upper layers */
		if (rtu->rx_msg_ok) {
			rc = rtu2prim(rtu, rtu->rx_msg, &prim);
			if (rc == -ENODATA) { /* Not enough data yet, simply wait until more data is received */
				LOGP(DLMODBUS_RTU, LOGL_DEBUG, "Not enough rx data yet\n");
				rtu->rx_msg_ok = false;
			}
			if (rc < 0) {
				LOGP(DLMODBUS_RTU, LOGL_ERROR, "Rx Error!\n");
				rtu->rx_msg_ok = false;
			}
		} else {
			LOGP(DLMODBUS_RTU, LOGL_ERROR, "Dropping NOK message\n");
		}
		msgb_trim(rtu->rx_msg, 0);
		rtu_transmit_fsm_state_chg(fi, RTU_TRANSMIT_ST_IDLE);
		if (rtu->rx_msg_ok)
			osmo_modbus_conn_rx_prim(rtu->conn, prim);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static const struct osmo_fsm_state rtu_transmit_states[] = {
	[RTU_TRANSMIT_ST_INITIAL] = {
		.in_event_mask = X(RTU_TRANSMIT_EV_START) |
				 X(RTU_TRANSMIT_EV_CHAR_RECEIVED) |
				 X(RTU_TRANSMIT_EV_T35_TIMEOUT),
		.out_state_mask = X(RTU_TRANSMIT_ST_IDLE),
		.name = "INITIAL",
		.action = rtu_transmit_fsm_st_initial,
		.onenter = rtu_transmit_fsm_st_initial_onenter,
	},
	[RTU_TRANSMIT_ST_IDLE] = {
		.in_event_mask = X(RTU_TRANSMIT_EV_DEMAND_OF_EMISSION) |
				 X(RTU_TRANSMIT_EV_CHAR_RECEIVED),
		.out_state_mask = X(RTU_TRANSMIT_ST_EMISSION) |
				  X(RTU_TRANSMIT_ST_RECEPTION),
		.name = "IDLE",
		.action = rtu_transmit_fsm_st_idle,
		.onenter = rtu_transmit_fsm_st_idle_onenter,
	},
	[RTU_TRANSMIT_ST_EMISSION] = {
		.in_event_mask = X(RTU_TRANSMIT_EV_T35_TIMEOUT),
		.out_state_mask = X(RTU_TRANSMIT_ST_IDLE),
		.name = "EMISSION",
		.action = rtu_transmit_fsm_st_emission,
		.onenter = rtu_transmit_fsm_st_emission_onenter,
	},
	[RTU_TRANSMIT_ST_RECEPTION] = {
		.in_event_mask = X(RTU_TRANSMIT_EV_CHAR_RECEIVED) |
				 X(RTU_TRANSMIT_EV_T15_TIMEOUT),
		.out_state_mask = X(RTU_TRANSMIT_ST_CTRL_WAIT),
		.name = "RECEPTION",
		.action = rtu_transmit_fsm_st_reception,
		.onenter = rtu_transmit_fsm_st_reception_onenter,
	},
	[RTU_TRANSMIT_ST_CTRL_WAIT] = {
		.in_event_mask = X(RTU_TRANSMIT_EV_CHAR_RECEIVED) |
				 X(RTU_TRANSMIT_EV_T35_TIMEOUT),
		.out_state_mask = X(RTU_TRANSMIT_ST_IDLE),
		.name = "CTRL_WAIT",
		.action = rtu_transmit_fsm_st_ctrlwait,
		.onenter = rtu_transmit_fsm_st_ctrlwait_onenter,
	},
};

static int rtu_transmit_fsm_timer_cb(struct osmo_fsm_inst *fi)
{
	switch (fi->T) {
	case 15:
		osmo_fsm_inst_dispatch(fi, RTU_TRANSMIT_EV_T15_TIMEOUT, NULL);
		break;
	case 35:
		osmo_fsm_inst_dispatch(fi, RTU_TRANSMIT_EV_T35_TIMEOUT, NULL);
		break;
	}
	return 0;
}

struct osmo_fsm rtu_transmit_fsm = {
	.name = "RTU_TRANSMIT",
	.states = rtu_transmit_states,
	.num_states = ARRAY_SIZE(rtu_transmit_states),
	.timer_cb = rtu_transmit_fsm_timer_cb,
	.log_subsys = DLMODBUS_RTU_OFFSET,
	.event_names = rtu_transmit_event_names,
	//.cleanup = rtu_transmit_fsm_cleanup,
};

static __attribute__((constructor)) void rtu_transmit_fsm_init(void)
{
	OSMO_ASSERT(osmo_fsm_register(&rtu_transmit_fsm) == 0);
}
