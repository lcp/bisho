bin_PROGRAMS = bisho

bisho_SOURCES = \
	main.c \
	bisho-window.c bisho-window.h \
	bisho-pane.c bisho-pane.h \
	bisho-pane-flickr.c bisho-pane-flickr.h \
	bisho-pane-oauth.c bisho-pane-oauth.h \
	bisho-pane-username.c bisho-pane-username.h \
 	bisho-facebook-pane.c bisho-facebook-pane.h \
	bisho-utils.c bisho-utils.h \
	service-info.c service-info.h \
	mux-label.c mux-label.h \
	mux-expander.c mux-expander.h \
	mux-expanding-item.c mux-expanding-item.h

if ! HAVE_INFOBAR
bisho_SOURCES += gtkinfobar.h gtkinfobar.c
endif

bisho_CPPFLAGS = $(DEPS_CFLAGS) \
	-DLIBEXECDIR=\"@libexecdir@\" \
	-DLOCALEDIR=\""$(datadir)/locale"\"  \
	-Wall -Wmissing-declarations
bisho_LDADD = $(DEPS_LIBS)
