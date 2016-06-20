TESTS = x11spice_test
ALL_XCB_CFLAGS=$(XCB_CFLAGS) $(DAMAGE_CFLAGS) $(XTEST_CFLAGS) $(SHM_CFLAGS) $(UTIL_CFLAGS)
ALL_XCB_LIBS=$(XCB_LIBS) $(DAMAGE_LIBS) $(XTEST_LIBS) $(SHM_LIBS) $(UTIL_LIBS)
AM_CFLAGS = -Wall $(ALL_XCB_CFLAGS) $(GTK2_CFLAGS) $(SPICE_CFLAGS) $(SPICE_PROTOCOL_CFLAGS) $(GLIB2_CFLAGS) $(PIXMAN_CFLAGS)
AM_LDFLAGS = $(ALL_XCB_LIBS) $(GTK2_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(PIXMAN_LIBS)

x11spice_test_SOURCES = \
    tests.c \
    x11spice_test.c \
    xcb.c \
    xdummy.c \
    main.c

noinst_PROGRAMS = $(TESTS)

.PHONY: leakcheck.log
leakcheck.log: 
	VALGRIND="valgrind --leak-check=full --log-file=leakcheck.log" make check
