AM_CFLAGS = $(WARNINGCFLAGS)
plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libwrapper_target.la libwrapper_client.la
plugin_CPPFLAGS = -I$(top_srcdir)/src


libwrapper_target_la_SOURCES=target_wrapper.c target_wrapper_plugin.c target_wrapper.h common.c common.h
libwrapper_target_la_LIBADD = ../../lib/libipbench.la

libwrapper_client_la_SOURCES=client_wrapper.c client_wrapper_plugin.c client_wrapper.h common.c common.h
libwrapper_client_la_LIBADD = ../../lib/libipbench.la

EXTRA_DIST = README

