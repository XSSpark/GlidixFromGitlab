#! /bin/bash
#	Glidix distro builder
#
#	Copyright (c) 2021, Madd Games.
#	All rights reserved.
#	
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions are met:
#	
#
#	* Redistributions of source code must retain the above copyright notice, this
#	  list of conditions and the following disclaimer.
#	
#	* Redistributions in binary form must reproduce the above copyright notice,
#	  this list of conditions and the following disclaimer in the documentation
#	  and/or other materials provided with the distribution.
#	
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
#	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
#	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
#	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

srcdir_rel="`dirname $0`"
srcdir="`realpath $srcdir_rel`"

if [ "$srcdir" = "." ]
then
	echo "ERROR: This script must be ran outside the source directory!"
	exit 1
fi

# notify <message>
#
# Prints a generic message to the console.
function notify {
	echo "[build-dist.sh] $1"
}

# build <srcdir-name> <build-dir-name> <options-as-a-single-string>
#
# Build the specified submodule.
function build {
	notify "Building $1 in $2..."

	mkdir -p $2
	if [ ! -f "$2/Makefile" ] || [ "$2/Makefile" -ot "$srcdir/$1/configure" ]
	then
		(cd "$2" && "$srcdir/$1/configure" $3) || exit 1
	fi

	(cd "$2" && make) || exit 1
}

# --- BEGIN BUILD PROCESS ---
notify "Building from source directory: $srcdir"

build dist-hdd-maker build-hdd-maker ''
build gxboot gxboot-build "--host=x86_64-glidix"
build kernel kernel-build "--host=x86_64-glidix"

notify "Generating kernel symbols..."
nm kernel-build/kernel/boot/initrd-sysroot/kernel.so | grep " T " | awk '{ print $1" "$3 }' > kernel.sym

notify "Creating image sysroot..."
rm -rf build-sysroot
mkdir build-sysroot || exit 1

notify "Copying source code into image sysroot..."
mkdir -p build-sysroot/usr/src || exit 1
cp -RT "$srcdir" build-sysroot/usr/src || exit 1
rm -rf build-sysroot/usr/src/.git || exit 1

notify "Installing all packages in image sysroot..."
(cd gxboot-build && DESTDIR=../build-sysroot make install) || exit 1
(cd kernel-build && DESTDIR=../build-sysroot make install) || exit 1

notify "Creating the initrd..."
mkdir -p build-sysroot/boot || exit 1
(cd build-sysroot/boot/initrd-sysroot && tar -cf vmglidix.tar * && mv vmglidix.tar ../vmglidix.tar) || exit 1

notify "Copying the static sysroot into image sysroot..."
cp -RT "$srcdir/hdd-sysroot" build-sysroot || exit 1

notify "Creating the HDD image..."
mkdir -p distro-out
build-hdd-maker/dist-hdd-maker/bin/dist-hdd-maker || exit 1

if (command -v "VBoxManage" >/dev/null 2>&1) && [ ! -f "distro-out/glidix.vmdk" ]
then
	notify "VBoxManage command exists, so creating VMDK..."
	(cd distro-out && VBoxManage internalcommands createrawvmdk -filename glidix.vmdk -rawdisk hdd.bin) || exit 1
fi

notify "Build completed successfully !!!"