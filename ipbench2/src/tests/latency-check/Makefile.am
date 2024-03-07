AM_CPPFLAGS = $(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = liblatencycheck.la
liblatencycheck_la_CFLAGS = -I$(top_srcdir)/src
liblatencycheck_la_LIBADD = ../../lib/libipbench.la
liblatencycheck_la_SOURCES=latency.c ip.c raw.c latency.h
