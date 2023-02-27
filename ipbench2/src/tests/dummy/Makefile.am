AM_CFLAGS = $(WARNINGCFLAGS) 

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libdummy.la libdummy_target.la
plugin_CPPFLAGS=-I$(top_srcdir)/src

libdummy_target_la_SOURCES=dummy_target.c target_plugin.c dummy_target.h
libdummy_target_la_LIBADD = ../../lib/libipbench.la

libdummy_la_SOURCES=dummy.c plugin.c dummy.h
libdummy_la_LIBADD = ../../lib/libipbench.la

