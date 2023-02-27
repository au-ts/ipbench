AM_CFLAGS=$(WARNINGCFLAGS)

EXTRA_DIST = README histogram.sh

noinst_PROGRAMS = calibrate
calibrate_SOURCES = calibrate.c
calibrate_LDADD = -lpthread

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libcpu_target_lukem.la
plugin_CPPFLAGS= -I$(top_srcdir)/src
libcpu_target_lukem_la_LIBADD = ../../lib/libipbench.la -lpthread
libcpu_target_lukem_la_SOURCES=cpu_target_lukem.c cpu_target_lukem.h
