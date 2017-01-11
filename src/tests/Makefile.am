TESTS = x11spice_test
ALL_XCB_CFLAGS=$(XCB_CFLAGS) $(DAMAGE_CFLAGS) $(XTEST_CFLAGS) $(SHM_CFLAGS) $(UTIL_CFLAGS)
ALL_XCB_LIBS=$(XCB_LIBS) $(DAMAGE_LIBS) $(XTEST_LIBS) $(SHM_LIBS) $(UTIL_LIBS)
AM_CFLAGS = -Wall $(ALL_XCB_CFLAGS) $(GTK_CFLAGS) $(SPICE_CFLAGS) $(SPICE_PROTOCOL_CFLAGS) $(GLIB2_CFLAGS) $(PIXMAN_CFLAGS)
AM_LDFLAGS = $(ALL_XCB_LIBS) $(GTK_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(PIXMAN_LIBS)

x11spice_test_SOURCES = \
    tests.c \
    tests.h \
    x11spice_test.c \
    x11spice_test.h \
    xcb.c \
    xcb.h \
    xdummy.c \
    xdummy.h \
    util.c \
    util.h \
    main.c

noinst_PROGRAMS = $(TESTS)

.PHONY: leakcheck.log callgrind.out.x
leakcheck.log: 
	VALGRIND="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --suppressions=options.supp --suppressions=gui.supp --log-file=leakcheck.log" make check

callgrind.out.x:
	VALGRIND="valgrind --tool=callgrind --log-file=callgrind.out.log" make check

clean-local:
	rm -f callgrind.out.*
