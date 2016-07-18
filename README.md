x11spice
========

  x11spice connects a running X server as a Spice server.

It owes a debt to the excellent x11vnc project, from the libvncserver project.
That project proved that this could be done, and done well.


Building
--------

  If you are reading this, you are likely looking at the source code.  In
that case, you should perform the following steps to build x11spice:

  ./autogen.sh
  make

In theory, autogen.sh will invoke configure and will check your system
for all necessary dependencies.

A further:
  sudo make install
will install the utility on to your system.  Note that the author usually
does this with a:
  ./autogen.sh --prefix=/some/non/root/path
  make && make install
  /some/non/root/path/bin/x11spice

Configuration
-------------
x11spice will use, in this order:
  - An x11spice file in the system configuration directory (e.g. /etc/xdg/x11spice)
  - An x11spice file in the users's config directory (e.g. ~/.config/x11spice)
  - Command line parameters

Some options are only available in the configuration file, in an attempt to
make the command line usage somewhat more simple.

The etc/x11spice.example file contains a commented example with all the
configuration examples described.


Usage
-----
The general idea is to invoke x11spice while connected to your X server.

You will need to select a port to listen to and a password.  There are
a variety of options to help with that selection.  The simplest command:
  x11spice --auto 5900-5910 --generate-password
should start a session and give you the port to use as well as a one time
password you can use to connect.

Refer to the x11spice man page for more details.

History
-------
x11spice was initially created by Jeremy White <jwhite@codeweavers.com>
in July of 2016.
