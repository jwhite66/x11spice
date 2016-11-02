Name:           x11spice
Version:        1.1
Release:        1%{?dist}
Summary:        Utility to share an x11 desktop via Spice
Group:          Applications/System
License:        GPLv3+
URL:            http://spice-space.org/
Source0:        http://people.freedesktop.org/~jwhite/%{name}/%{name}-%{version}.tar.gz
BuildRequires:  glib2-devel gtk2-devel libX11-devel spice-server-devel spice-protocol pixman-devel
BuildRequires:  libxcb-devel >= 1.11

%description
Utility to share x11 desktops via Spice.


%prep
%setup -q -n %{name}-%{version}


%build
%configure
make %{?_smp_mflags}

%install
%make_install

%files
%doc COPYING ChangeLog README
%{_bindir}/x11spice
%{_bindir}/x11spice_connected_gnome
%{_bindir}/x11spice_disconnected_gnome
%{_sysconfdir}/xdg/x11spice/x11spice.conf
%{_datadir}/applications/x11spice.desktop
%{_datadir}/icons/hicolor/scalable/apps/x11spice.svg
%{_mandir}/man1/%{name}*.1*


%changelog
* Wed Nov 02 2016 Jeremy White <jwhite@codeweavers.com> 1.1.0-1
- Fix issues uncovered by Coverity
- Invert the logic of view only; make it the default 
- Add optional audit calls
- Add callback capabilities
- Provide a connect / disconnect callback facility

* Fri Sep 02 2016 Jeremy White <jwhite@codeweavers.com> 1.0.0-1
- Initial package
