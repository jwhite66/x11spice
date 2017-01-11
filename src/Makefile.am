#This creates a make check-code-coverage target
#Uncomment if you've added the appropriate macros in ../configure.ac
#@CODE_COVERAGE_RULES@

bin_PROGRAMS = x11spice
ALL_XCB_CFLAGS=$(XCB_CFLAGS) $(DAMAGE_CFLAGS) $(XTEST_CFLAGS) $(SHM_CFLAGS) $(UTIL_CFLAGS) $(XKB_CFLAGS) $(XFIXES_CFLAGS)
ALL_XCB_LIBS=$(XCB_LIBS) $(DAMAGE_LIBS) $(XTEST_LIBS) $(SHM_LIBS) $(UTIL_LIBS) $(XKB_LIBS) $(XFIXES_LIBS)
CUSTOM_CFLAGS=-Wall -Wno-deprecated-declarations -Werror
AM_CFLAGS = $(CUSTOM_CFLAGS) $(ALL_XCB_CFLAGS) $(GTK_CFLAGS) $(SPICE_CFLAGS) $(SPICE_PROTOCOL_CFLAGS) $(GLIB2_CFLAGS) $(PIXMAN_CFLAGS) $(CODE_COVERAGE_CFLAGS)
x11spice_LDADD = $(ALL_XCB_LIBS) $(GTK_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(PIXMAN_LIBS) $(CODE_COVERAGE_LDFLAGS)
x11spice_SOURCES = \
    agent.c \
    agent.h \
    display.c \
    display.h \
    listen.c \
    listen.h \
    gui.c \
    gui.h \
    options.c \
    options.h \
    scan.c \
    scan.h \
    session.c \
    session.h \
    spice.c \
    local_spice.h \
    x11spice.h \
    main.c

dist_bin_SCRIPTS=x11spice_connected_gnome x11spice_disconnected_gnome

dist_man_MANS = data/x11spice.1

nobase_sysconf_DATA = xdg/x11spice/x11spice.conf

data_desktopdir = $(datadir)/applications
data_desktop_DATA = data/x11spice.desktop

data_icondir = $(datadir)/icons/hicolor/scalable/apps
data_icon_DATA = data/x11spice.svg

EXTRA_DIST = \
    xdg/x11spice/x11spice.conf \
    data/x11spice.desktop \
    data/x11spice.svg

if HAVE_GTEST
    SUBDIRS = tests
endif
