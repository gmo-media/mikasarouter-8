#!/bin/bash

[[ -d ~/rpmbuild/SOURCES/ ]] || mkdir -p ~/rpmbuild/SOURCES/
package="mikasarouter-8"
version=$(grep "^Version:" ${package}.spec | awk '{print $2}')
dirname="${package}-${version}"
source="${dirname}.tar.gz"

cmake .
test -d ../$dirname && rm -rf ../$dirname
cp -r . ../$dirname
cd ../$dirname
git clean -df
rm -rf .git
#cmake -DINSTALL_LAYOUT=RPM -DWITH_STATIC=yes -DWITH_LIBEVENT="system" -DWITH_SSL="system" -DENABLE_TESTS=yes -DENABLE_GCOV=yes .
cd $OLDPWD
cp packaging/rpm-oel/mysqlrouter.{service,tmpfiles.d,conf,init} ~/rpmbuild/SOURCES/

tar -C .. -cvzf $source $dirname
mv $source ~/rpmbuild/SOURCES/

rpmbuild -bb --define="with_libevent system" --define="with_ssl system" ${package}.spec && rm -rf ../$dirname
