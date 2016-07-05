#This creates a make check-code-coverage target
@CODE_COVERAGE_RULES@

bin_PROGRAMS = x11spice
ALL_XCB_CFLAGS=$(XCB_CFLAGS) $(DAMAGE_CFLAGS) $(XTEST_CFLAGS) $(SHM_CFLAGS) $(UTIL_CFLAGS) $(XKB_CFLAGS)
ALL_XCB_LIBS=$(XCB_LIBS) $(DAMAGE_LIBS) $(XTEST_LIBS) $(SHM_LIBS) $(UTIL_LIBS) $(XKB_LIBS)
AM_CFLAGS = -Wall $(ALL_XCB_CFLAGS) $(GTK2_CFLAGS) $(SPICE_CFLAGS) $(SPICE_PROTOCOL_CFLAGS) $(GLIB2_CFLAGS) $(PIXMAN_CFLAGS) $(CODE_COVERAGE_CFLAGS)
x11spice_LDADD = $(ALL_XCB_LIBS) $(GTK2_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(PIXMAN_LIBS) $(CODE_COVERAGE_LDFLAGS)
x11spice_SOURCES = \
    display.c \
    auto.c \
    gui.c \
    options.c \
    session.c \
    spice.c \
    scan.c \
    main.c

SUBDIRS = tests

