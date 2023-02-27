AM_CPPFLAGS = -I$(top_srcdir)/src -DDATADIR=\"./tests/tbench/\" 
AM_CFLAGS=-Wall

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libtbench.la
libtbench_la_SOURCES=tbench.c dbench.c sockio.c util.c child.c socklib.c tbench_srv.c
