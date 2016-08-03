#This creates a make check-code-coverage target
@CODE_COVERAGE_RULES@

bin_PROGRAMS = x11spice
ALL_XCB_CFLAGS=$(XCB_CFLAGS) $(DAMAGE_CFLAGS) $(XTEST_CFLAGS) $(SHM_CFLAGS) $(UTIL_CFLAGS) $(XKB_CFLAGS) $(XFIXES_CFLAGS)
ALL_XCB_LIBS=$(XCB_LIBS) $(DAMAGE_LIBS) $(XTEST_LIBS) $(SHM_LIBS) $(UTIL_LIBS) $(XKB_LIBS) $(XFIXES_LIBS)
AM_CFLAGS = -Wall $(ALL_XCB_CFLAGS) $(GTK2_CFLAGS) $(SPICE_CFLAGS) $(SPICE_PROTOCOL_CFLAGS) $(GLIB2_CFLAGS) $(PIXMAN_CFLAGS) $(CODE_COVERAGE_CFLAGS)
x11spice_LDADD = $(ALL_XCB_LIBS) $(GTK2_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(PIXMAN_LIBS) $(CODE_COVERAGE_LDFLAGS)
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

dist_man_MANS = x11spice.1

nobase_sysconf_DATA = xdg/x11spice/x11spice.conf

EXTRA_DIST = xdg/x11spice/x11spice.conf

SUBDIRS = tests
