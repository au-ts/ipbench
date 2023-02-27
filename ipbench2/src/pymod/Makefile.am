#AM_CFLAGS=$(WARNINGCFLAGS) <- swig produces too many warnings for this

EXTRA_DIST = except.h ipbench.i.in

CLEANFILES = ipbench_client_wrap.c ipbench_target_wrap.c ipbench.py ipbench_client.i ipbench_target.i ipbench_client.py ipbench_target.py 

libpythondir = $(pythondir)
libpython_LTLIBRARIES = libipbenchClientPython.la libipbenchTargetPython.la

libipbenchClientPython_la_SOURCES = ipbench_client_wrap.c ipbench.c except.c 
libipbenchClientPython_la_LIBADD = @PYTHON_EXTRA_LIBS@ ../lib/libipbench.la
libipbenchClientPython_la_LDFLAGS = -export -version-info 1:0:1
libipbenchClientPython_la_CPPFLAGS = @SWIG_PYTHON_CPPFLAGS@ -DIPBENCH_TEST_CLIENT=1 -DIPBENCH_PLUGIN_DIR=\"$(pkglibdir)\" -I$(top_srcdir)/src

libipbenchTargetPython_la_SOURCES = ipbench_target_wrap.c ipbench.c except.c
libipbenchTargetPython_la_LIBADD = @PYTHON_EXTRA_LIBS@ ../lib/libipbench.la
libipbenchTargetPython_la_LDFLAGS = -export -version-info 1:0:1
libipbenchTargetPython_la_CPPFLAGS = @SWIG_PYTHON_CPPFLAGS@ -DIPBENCH_TEST_TARGET=1 -DIPBENCH_PLUGIN_DIR=\"$(pkglibdir)\" -I$(top_srcdir)/src

#we have the shared library, plus we have a .py file that wraps around 
#the shared library 
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(pythondir)
	cd $(DESTDIR)$(pythondir) && rm -f _ipbench_client.so && $(LN_S) libipbenchClientPython.so _ipbench_client.so
	cp ipbench_client.py $(DESTDIR)/$(pythondir)
	cd $(DESTDIR)$(pythondir) && rm -f _ipbench_target.so && $(LN_S) libipbenchTargetPython.so _ipbench_target.so
	cp ipbench_target.py $(DESTDIR)/$(pythondir)

ipbench_client.i: $(srcdir)/ipbench.i.in
	sed -e 's/\[type\]/client/' $(srcdir)/ipbench.i.in > ipbench_client.i

ipbench_client_wrap.c: ipbench_client.i
	$(SWIG) $(SWIG_PYTHON_OPT) -o $@ ipbench_client.i

ipbench_target.i: $(srcdir)/ipbench.i.in
	sed -e 's/\[type\]/target/' $(srcdir)/ipbench.i.in > ipbench_target.i

ipbench_target_wrap.c: ipbench_target.i
	$(SWIG) $(SWIG_PYTHON_OPT) -o $@ ipbench_target.i
