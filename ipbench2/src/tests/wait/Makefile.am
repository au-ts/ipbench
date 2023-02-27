AM_CFLAGS=$(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libwait.la

libwait_CPPFLAGS = -I$(top_srcdir)/src
libwait_la_SOURCES=wait.c
libwait_la_LIBADD = ../../lib/libipbench.la
