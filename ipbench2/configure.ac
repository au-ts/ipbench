AC_PREREQ([2.69])

AC_INIT([ipbench],[2.1.1])
AC_CONFIG_MACRO_DIRS([m4])

AC_CONFIG_SRCDIR(src/ipbench.py)

AM_INIT_AUTOMAKE([foreign])
AC_DISABLE_STATIC

LT_INIT(dlopen)

AC_PROG_SWIG(4.0.0)
AM_PATH_PYTHON([3.9])
SWIG_PYTHON

AC_CONFIG_HEADERS(src/config.h)

AC_ARG_ENABLE(debug,
[  --enable-debug	debugging mode],
  if test "$enable_debug" = "yes"; then
    AC_DEFINE(DEBUG, [1], [Build with debugging information])
  fi)

#These test directories will be built by default
ipbench_tests="wait latency latency-check  dummy cpu_target nfsstone wrapper cpu_target_lukem nfs_latency discard_target"

AC_ARG_ENABLE([tests],
[  --enable-tests=TESTS		tests directories to build (comma separated, default is all tests)],
[enabled_tests=`echo "$enableval" | sed 's/,/ /g'`],
[enabled_tests=$ipbench_tests])

AC_SUBST([ENABLED_TEST_DIRS], $enabled_tests)

# Checks for programs.
AC_PROG_CC

# Check for gdbm
# AC_CHECK_LIB(gdbm, gdbm_open, , AC_MSG_ERROR([Please install gdbm libraries and development files]))
AC_CHECK_LIB(m, sqrt)

# Checks for header files.

AC_CHECK_HEADERS([gdbm.h stdio.h stdlib.h string.h fcntl.h termios.h errno.h ctype.h stdint.h sys/types.h sys/stat.h time.h ])

# Checks for some needed functions
AC_CHECK_FUNCS([gettimeofday socket strdup strerror strtod strtol strtoll])

AC_CHECK_HEADERS(sys/ioctl.h sys/sockio.h sys/socketio.h sys/filio.h \
 pty.h termios.h libutil.h util.h mcheck.h netinet/ether.h net/if_ether.h)

dnl Checks for struct ether_header.
AC_CHECK_HEADERS(net/if_ether.h net/ethernet.h)
AC_MSG_CHECKING([for struct ether_header])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#ifdef HAVE_NET_IF_ETHER_H
#include <net/if_ether.h>
#endif /* HAVE_NET_IF_ETHER_H */
#ifdef HAVE_NET_ETHERNET_H
#include <net/ethernet.h>
#endif /* HAVE_NET_ETHERNET_H */
]], [[
  struct ether_header the_ether_header;
  unsigned int ethertype;
  ethertype = the_ether_header.ether_type;
]])],[],[AC_MSG_ERROR(cannot find struct ether_header)])
AC_MSG_RESULT(yes)

dnl Checks for our network access type.
ipbench_raw_type=
AC_CHECK_HEADER(netpacket/packet.h, [ipbench_raw_type=linux])	
if test x$ipbench_raw_type = x; then
  AC_CHECK_HEADER(net/bpf.h, [ipbench_raw_type=bpf])
fi
case x$ipbench_raw_type in
xbpf)
  AC_CHECK_HEADERS(net/if_dl.h)
  AC_MSG_CHECKING([for AF_LINK support])
  AC_EGREP_CPP(_ipbench_has_af_link,
[
#include <sys/socket.h>
#ifdef AF_LINK
_ipbench_has_af_link
#endif
], [
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_AF_LINK, 1, [Have AF LINK])
], [
  AC_MSG_RESULT(no)
])
  ;;
xlinux)
  AC_CHECK_HEADERS(features.h)
  ;;
*)
  ;;
esac
AC_MSG_CHECKING([for raw Ethernet access method])
if test x$ipbench_raw_type = x; then
  AC_MSG_ERROR([can't find any raw Ethernet access method])
fi
AC_MSG_RESULT($ipbench_raw_type)

case x$ipbench_raw_type in
xbpf)
	AC_MSG_WARN([Configuring latency test without raw sockets; we dont support BPF raw sockets, yet!])
	AC_DEFINE(CONFIG_RAW_BPF, 1, [Have RAW Config])
	;;		     
xlinux)
	AC_MSG_RESULT([Configuring latency test with Linux RAW socket support])
	AC_DEFINE(CONFIG_RAW_LINUX, 1, [Have RAW Config])
	;;	
*)
	AC_MSG_WARN([Configuring latency test without raw socket support]);
	;;
esac


# setup some decent cflags
WARNINGCFLAGS="-Wall -Werror -O"
AC_SUBST(WARNINGCFLAGS)


# note, also add the test directory to the ipbench_tests variable above
AC_CONFIG_FILES([Makefile \
		    src/Makefile \
		    src/pymod/Makefile \
		    src/lib/Makefile \
		    src/tests/Makefile \
		    src/tests/dummy/Makefile \
		    src/tests/latency/Makefile \
		    src/tests/latency-check/Makefile \
		    src/tests/cpu_target/Makefile \
		    src/tests/nfsstone/Makefile \
		    src/tests/wait/Makefile \
		    src/tests/discard_target/Makefile \
		    src/tests/http/Makefile \
		    src/tests/tbench/Makefile \
		    src/tests/wrapper/Makefile \
		    src/tests/cpu_target_lukem/Makefile \
		    src/tests/nfs_latency/Makefile \
		    doc/Makefile \
		    doc/manual/Makefile])
AC_OUTPUT

