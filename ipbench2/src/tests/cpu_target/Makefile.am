AM_CFLAGS=$(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libcpu_target.la
libcpu_target_la_CPPFLAGS=-I$(top_srcdir)/src
libcpu_target_la_LIBADD = ../../lib/libipbench.la
libcpu_target_la_SOURCES=cpu_target.c
