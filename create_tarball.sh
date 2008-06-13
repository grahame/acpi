#!/bin/sh

# start this script inside the acpi source tree to create a releasable tarball

version=`grep AM_INIT_AUTOMAKE configure.ac |sed -e 's/AM_INIT_AUTOMAKE(acpi, //' -e 's/)//'`
major=`echo $version |cut -f1 -d"."`
minor=`echo $version |cut -f2 -d"."`
if [ -d ../acpi-$major.$minor ]
then
	echo "target directory exists"
	exit 1
fi

cp -a . ../acpi-$major.$minor
cd ../acpi-$major.$minor
autoreconf -i
cd ..
tar --exclude CVS --exclude autom4te.cache -zcf acpi-$major.$minor.tar.gz acpi-$major.$minor && rm -r acpi-$major.$minor
