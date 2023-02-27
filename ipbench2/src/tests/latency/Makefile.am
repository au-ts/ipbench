AM_CPPFLAGS = $(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = liblatency.la
liblatency_la_CFLAGS = -I$(top_srcdir)/src
liblatency_la_LIBADD = ../../lib/libipbench.la
liblatency_la_SOURCES=latency.c ip.c raw.c latency.h
