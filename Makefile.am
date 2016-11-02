SUBDIRS = src

rpm: dist
	rpmbuild -ba --define "_sourcedir $$PWD" src/data/x11spice.spec
