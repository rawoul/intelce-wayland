AC_PREREQ([2.64])

AC_INIT([intelce-wayland], [0.1], [sdk@freebox.fr])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-std=gnu99 -Wall -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

# Checks for libraries.
PKG_CHECK_MODULES(WAYLAND_CLIENT, wayland-client)
PKG_CHECK_MODULES(WAYLAND_SERVER, wayland-server)
PKG_CHECK_MODULES(GDL, gdl)
PKG_CHECK_MODULES(GMA, gma)
PKG_CHECK_MODULES(IMGEGL, imgegl)
PKG_CHECK_MODULES(PVR2D, pvr2d srv_um)

WAYLAND_SCANNER_RULES(['$(top_srcdir)/protocol'])

AC_CONFIG_FILES([
    Makefile
    egl/Makefile
    egl/wayland/Makefile
    egl/wayland/wayland-egl.pc
    egl/wsegl/Makefile
    gdl/Makefile
    gdl/wayland-gdl.pc
    protocol/Makefile
    ])
AC_OUTPUT
