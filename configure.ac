# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.60)
AC_INIT([GrdWine],[0.5.6],[hotline@guardant.ru])
AC_CONFIG_SRCDIR([src/grdwine.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE

AC_ARG_ENABLE([win64],
        [AS_HELP_STRING([--enable-win64],[build a Win64 emulator on AMD64 (won't run Win32 binaries)])])
AC_ARG_WITH([wineso],
        [AC_HELP_STRING([--with-wineso=PATH],[PATH to install .dll.so for Wine @<:@LIBDIR/wine@:>@])],
        [wineso="${withval}"],[wineso="\$(libdir)/wine"])
AC_ARG_WITH([winepe],
        [AC_HELP_STRING([--with-winepe=PATH],[PATH to install .dll for Wine @<:@LIBDIR/wine@:>@])],
        [winepe="${withval}"],[wineso="\$(libdir)/wine"])
AC_ARG_WITH([wineincs],
        [AC_HELP_STRING([--with-wineincs=PATH],[PATH to Wine C header files @<:@INCLUDEDIR@:>@])],
        [wineincs="${withval}"],[wineincs="\$(includedir)"])

AC_SUBST([wineso])
AC_SUBST([winepe])
AC_SUBST([wineincs])

AC_CANONICAL_HOST

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

AC_CHECK_PROGS(WINEGCC,winegcc,none)
if test "$WINEGCC" = "none"
then
        AC_MSG_ERROR([no suitable winegcc found. Please install the 'winegcc' package.])
fi

case $host in
        x86_64*)
                if test "x$enable_win64" != "xyes" -a "$cross_compiling" != "yes"
                then
                        CC="$CC -m32"
                        AC_MSG_CHECKING([whether $CC works])
                        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],[AC_MSG_RESULT([yes])],
                                [AC_MSG_RESULT([no])
                                AC_MSG_ERROR([Cannot build a 32-bit program, you need to install 32-bit development libraries.])])
                        WINEGCC="$WINEGCC -m32"
                fi
        ;;
esac

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h stdlib.h stdarg.h string.h stddef.h stdio.h])
AC_HEADER_ASSERT
AC_HEADER_STAT
AC_HEADER_DIRENT
AC_CHECK_HEADERS([sys/ioctl.h sys/time.h sys/types.h fcntl.h unistd.h limits.h])
# AC_CHECK_HEADERS([linux/usbdevice_fs.h linux/hiddev.h])
AC_CHECK_HEADERS([linux/usbdevice_fs.h])
# AC_CHECK_HEADERS([windef.h winbase.h winnt.h wine/debug.h])
save_CFLAGS="$CFLAGS"
if test "x${wineincs}" = "x\$(includedir)"
then
        CFLAGS="-D__WINESRC__ ${CFLAGS}"
        AC_CHECK_HEADERS([wine/windows/windef.h \
                          wine/windows/winbase.h \
                          wine/windows/winnt.h \
                          wine/debug.h],[],
                         [AC_MSG_WARN([see the --with-wineincs option.])])
else
        CFLAGS="-D__WINESRC__ -I${wineincs} -I${wineincs}/wine/windows ${CFLAGS}"
        AC_CHECK_HEADERS([windef.h wine/debug.h],[],[AC_MSG_ERROR([Wine C header files not found.])])
fi
CFLAGS="$save_CFLAGS"

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

AC_CHECK_FUNCS(fcntl getenv getpid select sleep snprintf umask)

if test "x${GCC}" = "xyes"
then
        CFLAGS="-fno-strict-aliasing ${CFLAGS}"
fi

save_CFLAGS="$CFLAGS"
CFLAGS="-fPIC ${CFLAGS}"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],[[]])],[],[CFLAGS="$save_CFLAGS"])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
