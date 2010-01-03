bin_PROGRAMS = bisho
bisho_SOURCES = \
	main.c \
	bisho-window.c bisho-window.h \
	bisho-pane-username.c bisho-pane-username.h \
	bisho-utils.c bisho-utils.h \
	mux-expander.c mux-expander.h \
	mux-expanding-item.c mux-expanding-item.h \
	bisho-pane.c bisho-pane.h \
	bisho-module.c bisho-module.h \
	service-info.c service-info.h \
	mux-label.c mux-label.h \
	bisho-webkit.c bisho-webkit.h
bisho_CPPFLAGS = $(DEPS_CFLAGS) \
	-DLIBEXECDIR=\"@libexecdir@\" \
	-DPKGLIBDIR=\"$(pkglibdir)\" \
	-DLOCALEDIR=\""$(datadir)/locale"\"  \
	-Wall -Wmissing-declarations
bisho_LDADD = $(DEPS_LIBS)

libbisho_ladir = $(pkgincludedir)
libbisho_la_HEADERS = \
	bisho-pane.h \
	service-info.h \
	mux-label.h
