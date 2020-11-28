#pragma once

#include <osmocom/modbus/modbus.h>

#define MODBUS_MSGB_SIZE 256

enum {
	DLMODBUS_OFFSET,
	DLMODBUS_RTU_OFFSET,
};

struct osmo_modbus_conn {
	enum osmo_modbus_conn_role role;
	enum osmo_modbus_proto_type proto_type;
	uint16_t address;
	osmo_modbus_prim_cb prim_cb;
	void *prim_cb_ctx;
	struct llist_head msg_queue;

	/* role: master or slave */
	union {
		struct {
			uint16_t req_for_addr; /* Address of request tgt in progress */
		} master;
		struct {
			bool monitor; /* Is monitor mode enabled ? */
		} slave;
	};
	struct osmo_tdef *T_defs;
	struct osmo_fsm_inst *fi;

	/* proto private data + specific operations */
	void *proto;
	struct {
		int (*connect)(struct osmo_modbus_conn* conn);
		bool (*is_connected)(struct osmo_modbus_conn* conn);
		int (*tx_prim)(struct osmo_modbus_conn* conn, struct osmo_modbus_prim *prim);
		void (*free)(struct osmo_modbus_conn* conn);
	} proto_ops;
};

void osmo_modbus_conn_rx_prim(struct osmo_modbus_conn* conn, struct osmo_modbus_prim *prim);
