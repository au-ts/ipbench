AM_CFLAGS=$(WARNINGCFLAGS)

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libcputarget_run_echo.la
libcputarget_run_echo_la_CPPFLAGS=-I$(top_srcdir)/src
libcputarget_run_echo_la_LIBADD = ../../lib/libipbench.la
libcputarget_run_echo_la_SOURCES=cputarget_runecho.c cpu_target.c cpu_target.h
