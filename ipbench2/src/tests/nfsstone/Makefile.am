AM_CFLAGS=$(WARNINGCFLAGS) -DLINUX -D_GNU_SOURCE

EXTRA_DIST=nfsstone-config.xml README

plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libnfsstones.la
plugin_CPPFLAGS=-I$(top_srcdir)/src
libnfsstones_la_LIBADD = ../../lib/libipbench.la
libnfsstones_la_SOURCES = nfsstones.c plugin.c nfsstones.h
