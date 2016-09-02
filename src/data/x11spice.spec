Name:           x11spice
Version:        1.0
Release:        1%{?dist}
Summary:        Utility to share an x11 desktop via Spice
Group:          Applications/System
License:        GPLv3+
URL:            http://spice-space.org/
Source0:        http://spice-space.org/download/releases/%{name}-%{version}.tar.gz
BuildRequires:  glib2-devel gtk2-devel libX11-devel spice-server-devel spice-protocol pixman-devel

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
%{_sysconfdir}/xdg/x11spice/x11spice.conf
%{_datadir}/applications/x11spice.desktop
%{_datadir}/icons/hicolor/scalable/apps/x11spice.svg
%{_mandir}/man1/%{name}*.1*


%changelog
* Fri Sep 02 2016 Jeremy White <jwhite@codeweavers.com> 1.0.0-1
- Initial package
/home/jwhite/x11spice/share/applications/x11spice.desktop
/home/jwhite/x11spice/share/icons/hicolor/scalable/apps/x11spice.svg
