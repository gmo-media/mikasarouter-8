# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
#
# MySQL MySQL Router is licensed under the terms of the GPLv2
# <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

%{!?src_base:                 %global src_base mikasarouter-8}

%global milestone    dmr

%global src_dir               %{src_base}-%{version}%{?milestone:-%{milestone}}

%if 0%{?suse_version} == 1315
%global dist            .sles12
%endif

%if 0%{?commercial}
%global license_type    Commercial
%global product_suffix  -commercial
%global project_edition -DPROJECT_EDITION=Commercial -DGPL=no
%else
%global license_type    GPLv2
%endif

%{!?with_systemd:       %global systemd 1}
%{?el6:                 %global systemd 0}

Summary:       MySQL Router
Name:          %{src_base}%{?product_suffix}
Version:       8.0.12
Release:       2%{?commercial:.1}%{?dist}
License:       Under %{?license_type} license as shown in the Description field.
Group:         Applications/Databases
URL:           https://dev.mysql.com/downloads/router/
Source0:       https://cdn.mysql.com/Downloads/router/%{src_base}-%{?commercial:commercial-}%{version}.tar.gz
Source1:       mysqlrouter.service
Source2:       mysqlrouter.tmpfiles.d
Source3:       mysqlrouter.init
Source4:       mysqlrouter.conf
BuildRequires: cmake
%{?el6:BuildRequires:  devtoolset-6-gcc}
%{?el6:BuildRequires:  devtoolset-6-gcc-c++}
%{?el7:BuildRequires:  devtoolset-6-gcc}
%{?el7:BuildRequires:  devtoolset-6-gcc-c++}
%if 0%{?commercial}
Provides:      mysql-router = %{version}-%{release}
Obsoletes:     mysql-router < %{version}-%{release}
%endif
%if 0%{?suse_version} >= 1210
BuildRequires: systemd-rpm-macros
%endif
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
%if 0%{?systemd}
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd
%else
Requires(post):   /sbin/chkconfig
Requires(preun):  /sbin/chkconfig
Requires(preun):  /sbin/service
%endif

%description
The MySQL(TM) Router software delivers a fast, multi-threaded way of
routing connections from MySQL Clients to MySQL Servers. MySQL is a
trademark of Oracle.

The MySQL software has Dual Licensing, which means you can use the
MySQL software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from Oracle and/or its affiliates if you do not wish to be
bound by the terms of the GPL. See the chapter "Licensing and Support"
in the manual for further info.

The MySQL web site (http://www.mysql.com/) provides the latest news
and information about the MySQL software. Also please see the
documentation and the manual for more information.

%package     -n %{src_base}-%{?product_suffix}-devel
Summary:        Development header files and libraries for MySQL Router
Group:          Applications/Databases
%if 0%{?commercial}
Provides:       mysql-router-devel = %{version}-%{release}
Obsoletes:      mysql-router-devel < %{version}-%{release}
%endif
%description -n %{src_base}-%{?product_suffix}-devel
This package contains the development header files and libraries
necessary to develop MySQL Router applications.

%prep
%setup -q -n %{name}-%{version}

%build
mkdir release && pushd release
%{?el6:export CC=/opt/rh/devtoolset-6/root/usr/bin/gcc}
%{?el6:export CXX=/opt/rh/devtoolset-6/root/usr/bin/g++}
%{?el7:export CC=/opt/rh/devtoolset-6/root/usr/bin/gcc}
%{?el7:export CXX=/opt/rh/devtoolset-6/root/usr/bin/g++}
cmake .. -DINSTALL_LAYOUT=RPM \
  -DWITH_STATIC=yes -DWITH_MYSQL="%{with_mysql}" %{?project_edition} \
  -DWITH_LIBEVENT="%{with_libevent}" \
  -DWITH_SSL="%{with_ssl}" \
  -DENABLE_TESTS=yes -DENABLE_GCOV=yes
make %{?_smp_mflags} VERBOSE=1
make test || true
popd

%install
rm -rf %{buildroot}
pushd release
%{?el6:export CC=/opt/rh/devtoolset-6/root/usr/bin/gcc}
%{?el6:export CXX=/opt/rh/devtoolset-6/root/usr/bin/g++}
%{?el7:export CC=/opt/rh/devtoolset-6/root/usr/bin/gcc}
%{?el7:export CXX=/opt/rh/devtoolset-6/root/usr/bin/g++}
make DESTDIR=%{buildroot} install

install -d -m 0755 %{buildroot}/%{_localstatedir}/log/mysqlrouter
install -d -m 0755 %{buildroot}/%{_localstatedir}/run/mysqlrouter

%if 0%{?systemd}
install -D -p -m 0644 %{SOURCE1} %{buildroot}%{_unitdir}/mysqlrouter.service
install -D -p -m 0644 %{SOURCE2} %{buildroot}%{_tmpfilesdir}/mysqlrouter.conf
%else
install -D -p -m 0755 %{SOURCE3} %{buildroot}%{_sysconfdir}/init.d/mysqlrouter
%endif
install -D -p -m 0644 %{SOURCE4} %{buildroot}%{_sysconfdir}/mysqlrouter/mysqlrouter.conf

# remove some unwanted files
rm -rf %{buildroot}%{_includedir}
rm -rf %{buildroot}/%{_libdir}/libmysqlharness.a

%clean
rm -rf %{buildroot}

%pre
/usr/sbin/groupadd -r mysqlrouter >/dev/null 2>&1 || :
/usr/sbin/useradd -M -N -g mysqlrouter -r -d /var/lib/mysqlrouter -s /bin/false \
    -c "MySQL Router" mysqlrouter >/dev/null 2>&1 || :
%if 0%{?suse_version}
%service_add_pre mysqlrouter.service
%endif

%post
/sbin/ldconfig
[ -e /var/log/mysqlrouter ] && chown -R mysqlrouter. /var/log/mysqlrouter
%if 0%{?systemd}
%if 0%{?suse_version}
%service_add_post mysqlrouter.service
/usr/bin/systemd-tmpfiles --create %{_tmpfilesdir}/mysqlrouter.conf >/dev/null 2>&1 || :
%else
%systemd_post mysqlrouter.service
%endif # suse_version
%else
/sbin/chkconfig --add mysqlrouter
%endif # systemd

%preun
%if 0%{?systemd}
%if 0%{?suse_version}
%service_del_preun mysqlrouter.service
%else
%systemd_preun mysqlrouter.service
%endif # suse_version
%else
if [ "$1" = 0 ]; then
    /sbin/service mysqlrouter stop >/dev/null 2>&1 || :
    /sbin/chkconfig --del mysqlrouter
fi
%endif # systemd

%postun
/sbin/ldconfig
%if 0%{?systemd}
%if 0%{?suse_version}
%service_del_postun mysqlrouter.service
%else
%systemd_postun_with_restart mysqlrouter.service
%endif # suse_version
%else
if [ $1 -ge 1 ]; then
    /sbin/service mysqlrouter condrestart >/dev/null 2>&1 || :
fi
%endif # systemd

%files
%defattr(-, root, root, -)
%doc License.txt README.txt doc/sample_mysqlrouter.conf
%dir %{_sysconfdir}/mysqlrouter
%config(noreplace) %{_sysconfdir}/mysqlrouter/mysqlrouter.conf
%{_bindir}/mysqlrouter
%{_bindir}/mysqlrouter_plugin_info
%if 0%{?systemd}
%{_unitdir}/mysqlrouter.service
%{_tmpfilesdir}/mysqlrouter.conf
%else
%{_sysconfdir}/init.d/mysqlrouter
%endif
%{_libdir}/libmysql*.so.*
%{_libdir}/libmysqlharness.so
%{_libdir}/libmysqlrouter.so
%dir %{_libdir}/mysqlrouter
%{_libdir}/mysqlrouter/*.so
%dir %attr(755, mysqlrouter, mysqlrouter) %{_localstatedir}/log/mysqlrouter
%dir %attr(755, mysqlrouter, mysqlrouter) %{_localstatedir}/run/mysqlrouter

%changelog
* Fri Oct 20 2017 Andrzej Religa <andrzej.religa@oracle.com> - 8.0.4
- Added sample config file to the rpm package

* Fri Sep 01 2017 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 8.0.3-0.1
- Updated for 8.0.3 dmr release

* Tue Jun 06 2017 Andrzej Religa <andrzej.religa@oracle.com> - 2.1.4-1
- added mysqlrouter_plugin_info binary

* Mon Mar 20 2017 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.1.3-1
- Updated for 2.1.3 GA release

* Thu Feb 23 2017 Pawel Mroszczyk <pawel.mroszczyk@oracle.com> - 2.1.2-0.1-rc
- Updated for 2.1.2 rc release: changed file/dir owner to mysqlrouter:mysqlrouter

* Mon Jan 30 2017 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.1.2-0.1-rc
- Updated for 2.1.2 rc release

* Tue Nov 29 2016 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.1.1-0.1
- Updated for 2.1.1 labs release

* Sat Sep 10 2016 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.1.0-0.1
- Updated for 2.1.0 labs release

* Thu Nov 26 2015 Balasubramanian Kandasamy <balasubramanian.kandasamy@oracle.com> - 2.0.3-1
- Add support for el6
- Fix group and buildreq

* Thu Oct 15 2015 Geert Vanderkelen <geert.vanderkelen@oracle.com> - 2.0.2-1
- Added pre and postun scripts for sles
- Making version a variable and using _libdir
- Adding -DWITH_MYSQL and building libmysqlclient statically in

* Wed Aug 19 2015 Geert Vanderkelen <geert.vanderkelen@oracle.com> - 2.0.1-1
- Initial version
