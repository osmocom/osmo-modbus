#pragma once

#include <osmocom/core/msgb.h>

#include <osmocom/modbus/modbus_rtu.h>
#include <osmocom/modbus/modbus_prim.h>

struct osmo_modbus_conn_rtu {
	struct osmo_modbus_conn* conn; /* backpointer */
	char *dev_path;
	unsigned baudrate;
	struct osmo_fd ofd;
	struct msgb *rx_msg;
	bool rx_msg_ok; /* OK (true) or NOK (false) */ /* TODO: use msg->cb instead to store the OK/NOK */
	struct msgb *tx_msg;
	struct osmo_tdef *T_defs;
	struct osmo_fsm_inst *fi;
};

struct msgb* prim2rtu(struct osmo_modbus_prim *prim);
int rtu2prim(struct osmo_modbus_conn_rtu* rtu, struct msgb* msg, struct osmo_modbus_prim **prim);

/* 1 RTU char: start bit, 8 data bits, stop bit, and parity bit (or 2nd stop bit if no parity) */
static inline unsigned long rtu_chars2bits(unsigned long num_chars) {
	return num_chars*11;
}

uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);
