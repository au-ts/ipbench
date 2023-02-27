AM_CFLAGS = $(WARNINGCFLAGS)
AUTOMAKE_OPTIONS = subdir-objects
plugindir=$(libdir)/ipbench
plugin_LTLIBRARIES = libnfs_latency.la
plugin_CPPFLAGS=-I$(top_srcdir)/src

LIBNFS_FILES=libnfs/callback.c libnfs/mount.c libnfs/nfs.c libnfs/portmap.c libnfs/rpc.c libnfs/xdr.c libnfs/callback.h libnfs/nfs.h libnfs/nfsrpc.h libnfs/rpc.h libnfs/xdr.h

libnfs_latency_la_SOURCES=nfs_glue.c nfs_latency.c plugin.c nfs_glue.h nfs_latency.h $(LIBNFS_FILES)
libnfs_latency_la_LIBADD = ../../lib/libipbench.la
libnfs_latency_la_CPPFLAGS =   -DIPBENCH_TEST_CLIENT
