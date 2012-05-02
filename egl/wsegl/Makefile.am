lib_LTLIBRARIES = libpvrwaylandWSEGL.la

libpvrwaylandWSEGL_la_CPPFLAGS =		\
	-DWSEGL_MODULE				\
	-I$(top_srcdir)/gdl			\
	-I$(top_srcdir)/egl/wayland

libpvrwaylandWSEGL_la_CFLAGS =			\
	$(GCC_CFLAGS)				\
	$(WAYLAND_CLIENT_CFLAGS)		\
	$(GDL_CFLAGS)				\
	$(GMA_CFLAGS)				\
	$(PVR2D_CFLAGS)				\
	$(IMGEGL_CFLAGS)

libpvrwaylandWSEGL_la_LDFLAGS = -module -avoid-version

libpvrwaylandWSEGL_la_LIBADD =			\
	../../gdl/libwayland-gdl.la		\
	../wayland/libwayland-egl.la		\
	$(WAYLAND_CLIENT_LIBS)			\
	$(GDL_LIBS)				\
	$(GMA_LIBS)				\
	$(PVR2D_LIBS)				\
	$(IMGEGL_LIBS)

libpvrwaylandWSEGL_la_SOURCES =			\
	wayland-wsegl.c				\
	wayland-wsegl.h				\
	buffer.c				\
	pf.c					\
	pixmap.c				\
	pixmap.h				\
	util.c
