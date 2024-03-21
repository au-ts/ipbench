CLEANFILES = *.o

SUBDIRS=lib tests pymod

AM_LDFLAGS=-Wl,--export-dynamic

#make sure these scripts get distributed
dist_bin_SCRIPTS=ipbench ipbenchd

SUFFIXES = .py

.py:
	cp $< $@
	chmod +x $@

noinst_HEADERS = ipbench.h

ipbench_headersdir = $(pkgincludedir)
ipbench_headers_HEADERS=plugin.h 
