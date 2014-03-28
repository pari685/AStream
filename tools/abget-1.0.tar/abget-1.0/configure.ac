AC_PREREQ([2.59])
AC_INIT([abget], [1.0], [papadog@ics.forth.gr])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([abget.c])
#AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(foreign no-dependencies no-installinfo)

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_INSTALL

AC_DEFINE(CONF_FILE, "abget.conf", [configuration file])
CONF_FILE=abget.conf

# Debug
AC_MSG_CHECKING([if debug is enabled])
AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug], [Print debug messages]),
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac], [debug=false])
AM_CONDITIONAL(BINARY_DEBUG, test x$debug = xtrue)
if test x$debug = xtrue; then
	AC_DEFINE(DEBUG, 1, [Print debug messages])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# Planetlab raw socket interface
AC_MSG_CHECKING([if planetlab is enabled])
AC_ARG_ENABLE(planetlab,
AS_HELP_STRING([--enable-planetlab], [Planetlab raw socket interface]),
[case "${enableval}" in
	yes) planetlab=true ;;
	no)  planetlab=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-planetlab) ;;
esac], [planetlab=false])
AM_CONDITIONAL(BINARY_PLANETLAB, test x$planetlab = xtrue)
if test x$planetlab = xtrue; then
	AC_DEFINE(PLANETLAB, 1, [Enable Planetlab raw socket interface])
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

#if planetlab is not enabled, check for iptables
if test x$planetlab = xfalse; then
  AC_PATH_PROG(ac_abget_firewall, iptables, no, $PATH:/sbin:/usr/sbin:/usr/local/sbin:/usr/ucb)
  if test $ac_abget_firewall = no ; then
	AC_MSG_ERROR(iptables not found: abget requires the usage of a firewall)
  else
	FIREWALL=$ac_abget_firewall
	AC_DEFINE_UNQUOTED(IPTABLES, "${FIREWALL}", [iptables path])
  fi
fi

#
# Checks for libraries.
#

#Check for libmath
AC_CHECK_LIB(m, main)

# Check for libpcap
AC_MSG_CHECKING([for libpcap])
AC_ARG_WITH(libpcap,
AS_HELP_STRING([--with-libpcap=DIR], [Manual libpcap path configuration, in case the configuration script can't find it. Search for libpcap in DIR/lib, and for the header file in DIR/include]),
[ case "$withval" in
  yes|no)
     AC_MSG_RESULT(no)
     ;;
  *)
     AC_MSG_RESULT($withval)
     if test -f $withval/pcap.h -a -f $withval/libpcap.a; then
        owd=`pwd`
        if cd $withval; then withval=`pwd`; cd $owd; fi
	PCAPINC="-I$withval -I$withval/bpf"
	PCAPLIB="-L$withval -lpcap"
     else
        AC_MSG_ERROR(pcap.h or libpcap.a not found in $withval)
     fi
     ;;
  esac ],
[ if test -f ${prefix}/include/pcap.h; then
     PCAPINC="-I${prefix}/include"
     PCAPLIB="-L${prefix}/lib -lpcap"
  elif test -f /usr/include/pcap/pcap.h; then
     PCAPINC="-I/usr/include/pcap"
     PCAPLIB="-lpcap"
  elif test -f /usr/include/pcap.h; then
     PCAPLIB="-lpcap"
  else
     AC_MSG_RESULT(no)
     AC_MSG_ERROR([libpcap not found - go get it from http://www.tcpdump.org or use the --with-libpcap option if you have installed it in an unusual place])
  fi
  AC_MSG_RESULT(yes) ]
)
AC_SUBST(PCAPINC)
AC_SUBST(PCAPLIB)



# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h memory.h netdb.h netinet/in.h stdio.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h sys/stat.h unistd.h signal.h sys/ioctl.h net/if.h ])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_INT32_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_MALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero gethostbyaddr gethostbyname gethostname gettimeofday inet_ntoa memmove memset socket strchr strdup strrchr select])

# $prefix stores the value of the --prefix command line option, or NONE if
# the option wasn't set. In the latter case, set it to the default, so that
# we can use it to expand directories now.
if test "x$prefix" = "xNONE"; then
	prefix=$ac_default_prefix
fi

CONFDIR=`eval echo $sysconfdir/abget`
AC_DEFINE_UNQUOTED(CONFDIR, "${CONFDIR}", [configuration directory])
AC_DEFINE_UNQUOTED(CONF_FILENAME, "${CONFDIR}/${CONF_FILE}:abget.conf", [full path for configuration file])

CFLAGS=-g
if test "$GCC" = "yes"; then
	CFLAGS="$CFLAGS -W -Wall -Wcast-align -Wpointer-arith"
fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo ""
echo "Configuration (after install):"
echo "Install path:              ${prefix}"
echo "abget binary:              ${prefix}/bin"
echo "Configuration file:        ${CONFDIR}/${CONF_FILE}"
if test x$debug = xtrue; then
echo "option:                    debug mode"
fi
if test x$planetlab = xtrue; then
echo "option:                    Planetlab raw socket interface"
fi

