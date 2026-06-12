AM_CFLAGS=$(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libiperfcpu.la
libiperfcpu_la_CPPFLAGS=-I$(top_srcdir)/src
libiperfcpu_la_LIBADD = ../../lib/libipbench.la
libiperfcpu_la_SOURCES=iperf3_cpu_target.c cpu_target.c cpu_target.h
