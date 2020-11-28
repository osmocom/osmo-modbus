/* Figure 7: Master state diagram */
#pragma once

enum conn_master_state {
	CONN_MASTER_ST_DISCONNECTED,
	CONN_MASTER_ST_IDLE,
	CONN_MASTER_ST_WAIT_TURNAROUND_DELAY,
	CONN_MASTER_ST_WAIT_REPLY,
	//CONN_MASTER_STPROCESSING_REPLY,
};

enum conn_slave_state {
	CONN_SLAVE_ST_DISCONNECTED,
	CONN_SLAVE_ST_IDLE,
	CONN_SLAVE_ST_CHECK_REQUEST,
};

enum conn_event {
	CONN_EV_CONNECT,
	CONN_EV_SUBMIT_PRIM,
	CONN_EV_RECV_PRIM,
	_NUM_CONN_EV,
};

extern struct osmo_fsm conn_master_fsm;
extern struct osmo_fsm conn_slave_fsm;
