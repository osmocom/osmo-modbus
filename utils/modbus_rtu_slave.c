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

#define _GNU_SOURCE
#include <getopt.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>

#include <osmocom/core/select.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/application.h>
#include <osmocom/core/timer.h>

#include <osmocom/modbus/modbus.h>
#include <osmocom/modbus/modbus_rtu.h>

#define APP_NAME "OsmoModbusRTUslave"

static void *tall_ctx;
static struct osmo_modbus_conn *conn;
static uint16_t slave_address = 0x01;
static char device_path[256] = "/dev/ttyUSB0";
bool monitor;

static void print_help(void)
{
	printf("  -h --help			This text.\n");
	printf("  -V --version			Print the version of " APP_NAME "\n");
	printf("  -T --timestamp		Print a timestamp in the debug output.\n");
	printf("  -s  --serial-device PATH	Set serial device (RTU connection)\n");
	printf("  -a  --slave-addess ADDRESS	Set slave address to listen to\n");
	printf("  -m  --monitor			Enable monitor mode\n");
}

static void handle_options(int argc, char **argv)
{
	while (1) {
		int option_index = 0, c;
		static const struct option long_options[] = {
			{ "help", 0, 0, 'h' },
			{ "version", 0, 0, 'V' },
			{"timestamp", 0, 0, 'T'},
			{"serial-device", 1, 0, 's'},
			{"slave-address", 1, 0, 'a'},
			{"monitor", 0, 0, 'm'},
			{ NULL, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "hVTs:a:m", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_help();
			exit(0);
			break;
		case 'V':
			//print_version(1);
			exit(0);
			break;
		case 'T':
			log_set_print_timestamp(osmo_stderr_target, 1);
			log_set_print_extended_timestamp(osmo_stderr_target, 1);
			break;
		case 's':
			osmo_strlcpy(device_path, optarg, sizeof(device_path));
			break;
		case 'a':
			slave_address = atoi(optarg);
			break;
		case 'm':
			monitor = true;
			break;
		default:
			fprintf(stderr, "Error in command line options. Exiting\n");
			exit(1);
			break;
		}
	}

	if (argc > optind) {
		fprintf(stderr, "Unsupported positional arguments in command line\n");
		exit(2);
	}
}

static void signal_handler(int signal)
{
	fprintf(stdout, "signal %u received\n", signal);

	switch (signal) {
	case SIGINT:
	case SIGTERM:
		exit(0);
		break;
	case SIGABRT:
		osmo_generate_backtrace();
		/* in case of abort, we want to obtain a talloc report
		 * and then return to the caller, who will abort the process */
	case SIGUSR1:
		talloc_report_full(tall_ctx, stderr);
		break;
	case SIGUSR2:
		break;
	default:
		break;
	}
}

enum {
	DMAIN,
};

void _log_init(void *tall_ctx)
{
	unsigned own_logcats = 1;
	unsigned lib_logcats;
	lib_logcats = osmo_modbus_set_logging_category_offset(own_logcats);
	struct log_info_cat log_info_cat[own_logcats + lib_logcats];
	log_info_cat[DMAIN] = (struct log_info_cat){
		.name = "DMAIN",
		.description = "main",
		.color = "\033[1;32m",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	};
	log_info_cat[DLMODBUS] = (struct log_info_cat){
		.name = "DLMODBUS",
		.description = "Modbus Library",
		.color = "\033[1;33m",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	};
	log_info_cat[DLMODBUS_RTU] = (struct log_info_cat){
		.name = "DLMODBUS_RTU",
		.description = "Modbus Library (RTU)",
		.color = "\033[1;34m",
		.enabled = 1, .loglevel = LOGL_DEBUG,
	};

	const struct log_info log_info = {
		.cat = log_info_cat,
		.num_cat = ARRAY_SIZE(log_info_cat),
	};
	osmo_init_logging2(tall_ctx, &log_info);

	log_set_print_category_hex(osmo_stderr_target, 0);
	log_set_print_category(osmo_stderr_target, 1);
	log_set_print_filename2(osmo_stderr_target, LOG_FILENAME_BASENAME);
	osmo_fsm_log_addr(false);
}

int prim_cb(struct osmo_modbus_conn *conn, struct osmo_modbus_prim *prim, void *ctx)
{
	struct osmo_modbus_prim *resp;
	uint16_t *buf;
	int rc;

	LOGP(DMAIN, LOGL_INFO, "prim_cb()!\n");
	switch (OSMO_PRIM_HDR(&prim->oph)) {
	case OSMO_PRIM(OSMO_MODBUS_PRIM_RESPONSE_TIMEOUT, PRIM_OP_INDICATION):
		LOGP(DMAIN, LOGL_INFO, "Tx timeout!\n");
		break;
	case OSMO_PRIM(OSMO_MODBUS_PRIM_N_MULT_HOLD_REG, PRIM_OP_REQUEST):
		LOGP(DMAIN, LOGL_INFO, "Received OSMO_MODBUS_PRIM_N_MULT_HOLD_REG REQUEST!\n");
		LOGP(DMAIN, LOGL_INFO, "[addr=%u] Read %u registers: start from 0x%04x\n", prim->address,
		     prim->u.read_mult_hold_reg_req.num_reg,
		     prim->u.read_mult_hold_reg_req.first_reg);

		/* Avoid answering for requests not aimed at us if we enabled monitor mode */
		if (!monitor || prim->address == slave_address) {
			buf = calloc(prim->u.read_mult_hold_reg_req.num_reg, sizeof(uint16_t));
			memset(buf, 0x2b, prim->u.read_mult_hold_reg_req.num_reg * sizeof(uint16_t));
			resp = osmo_modbus_makeprim_mult_hold_reg_resp(slave_address,
								       prim->u.read_mult_hold_reg_req.num_reg,
								       buf);
			free(buf);
			rc = osmo_modbus_conn_submit_prim(conn, resp);
			if (rc < 0) {
			     LOGP(DMAIN, LOGL_INFO, "Failed submitting primitive: %d\n", rc);
			     exit(1);
			}
		}
		break;
	default:
		LOGP(DMAIN, LOGL_INFO, "Unhandled primitive operation %s on primitive %s\n",
		     get_value_string(osmo_prim_op_names, prim->oph.operation),
		     get_value_string(osmo_modbus_prim_type_names, prim->oph.primitive));
	}
	msgb_free(prim->oph.msg);
	return 0;
}


int main(int argc, char **argv)
{
	struct osmo_modbus_conn_rtu *rtu;
	int rc;

	tall_ctx = talloc_named_const(NULL, 1, APP_NAME);
	msgb_talloc_ctx_init(tall_ctx, 0);
	_log_init(tall_ctx);

	handle_options(argc, argv);

	signal(SIGINT, &signal_handler);
	signal(SIGTERM, &signal_handler);
	signal(SIGABRT, &signal_handler);
	signal(SIGUSR1, &signal_handler);
	signal(SIGUSR2, &signal_handler);
	osmo_init_ignore_signals();

	LOGP(DMAIN, LOGL_INFO, "Initializig modbus conn...\n");
	conn = osmo_modbus_conn_alloc(tall_ctx,
				      OSMO_MODBUS_ROLE_SLAVE,
				      OSMO_MODBUS_PROTO_RTU);
	osmo_modbus_conn_set_prim_cb(conn, prim_cb, NULL);
	osmo_modbus_conn_set_address(conn, slave_address);
	osmo_modbus_conn_set_monitor_mode(conn, monitor);
	rtu = osmo_modbus_conn_get_rtu(conn);
	osmo_modbus_conn_rtu_set_device(rtu, device_path);
	if ((rc = osmo_modbus_conn_connect(conn)) < 0) {
		LOGP(DMAIN, LOGL_INFO, "Connect to modbus serial device %s failed! %d\n", device_path, rc);
		exit(1);
	}

	while (1) {
		rc = osmo_select_main(0);
		if (rc < 0)
			exit(3);
	}
}
