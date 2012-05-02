lib_LTLIBRARIES = libwayland-gdl.la libwayland-gdl-server.la

include_HEADERS =				\
	wayland-gdl.h				\
	wayland-gdl-server.h

nodist_include_HEADERS =			\
	wayland-gdl-client-protocol.h		\
	wayland-gdl-server-protocol.h

nodist_libwayland_gdl_la_SOURCES =		\
	wayland-gdl-protocol.c

libwayland_gdl_server_la_CFLAGS = $(GCC_CFLAGS) $(GDL_CFLAGS)
libwayland_gdl_server_la_LIBADD = $(GDL_LIBS)
libwayland_gdl_server_la_SOURCES =		\
	wayland-gdl-server.c
nodist_libwayland_gdl_server_la_SOURCES =	\
	wayland-gdl-protocol.c			\
	wayland-gdl-server-protocol.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = wayland-gdl.pc

BUILT_SOURCES =					\
	wayland-gdl-client-protocol.h		\
	wayland-gdl-server-protocol.h		\
	wayland-gdl-protocol.c

CLEANFILES = $(BUILT_SOURCES)

@wayland_scanner_rules@
