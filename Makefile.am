AUTOMAKE_OPTIONS = foreign dist-bzip2 1.6

AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include
SUBDIRS = include src utils

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libosmo-modbus.pc

EXTRA_DIST = \
	     .version \
	     git-version-gen \
	     $(NULL)

@RELMAKE@

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version
