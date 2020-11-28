AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS=-Wall -g $(LIBOSMOCORE_CFLAGS) $(COVERAGE_FLAGS)
AM_LDFLAGS=$(COVERAGE_LDFLAGS)

bin_PROGRAMS = modbus_rtu_master modbus_rtu_slave crc16_rtu_gen

modbus_rtu_master_SOURCES = modbus_rtu_master.c
modbus_rtu_master_LDADD = $(top_builddir)/src/libosmo-modbus.la \
			 $(LIBOSMOCORE_LIBS) \
			 $(NULL)

modbus_rtu_slave_SOURCES = modbus_rtu_slave.c
modbus_rtu_slave_LDADD = $(top_builddir)/src/libosmo-modbus.la \
			 $(LIBOSMOCORE_LIBS) \
			 $(NULL)

crc16_rtu_gen_SOURCES = crc16_rtu_gen.c
crc16_rtu_gen_LDADD = $(top_builddir)/src/libosmo-modbus.la \
			 $(LIBOSMOCORE_LIBS) \
			 $(NULL)
