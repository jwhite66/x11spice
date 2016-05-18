bin_PROGRAMS = x11spice
AM_CFLAGS = -Wall $(XLL_CFLAGS) $(GTK2_CFLAGS) $(SPICE_CFLAGS) $(GLIB2_CFLAGS) $(XDAMAGE_CFLAGS)
x11spice_LDADD = $(X11_LIBS) $(GTK2_LIBS) $(SPICE_LIBS) $(GLIB2_LIBS) $(XDAMAGE_LIBS)
x11spice_SOURCES = \
    display.c \
    gui.c \
    options.c \
    session.c \
    spice.c \
    main.c
