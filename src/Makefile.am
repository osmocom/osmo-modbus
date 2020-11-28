AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include -I$(top_builddir)
AM_CFLAGS=-Wall $(LIBOSMOCORE_CFLAGS)

noinst_HEADERS = \
	modbus_internal.h \
	conn_fsm.h \
	rtu_transmit_fsm.h \
	rtu_internal.h \
	$(NULL)

lib_LTLIBRARIES = libosmo-modbus.la

# This is _NOT_ the library release version, it's an API version.
# Please read Chapter 6 "Library interface versions" of the libtool
# documentation before making any modification
LIBVERSION=0:0:0

libosmo_modbus_la_SOURCES = \
	conn.c \
	conn_master_fsm.c \
	conn_slave_fsm.c \
	conn_rtu.c \
	rtu_transmit_fsm.c \
	prim.c \
	$(NULL)

libosmo_modbus_la_LDFLAGS = -version-info $(LIBVERSION) -no-undefined -export-symbols-regex '^(osmo_|DLMODBUS)'
libosmo_modbus_la_LIBADD = $(LIBOSMOCORE_LIBS)
