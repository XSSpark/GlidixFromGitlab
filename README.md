# Glidix
Glidix is an operating system for the x86_64 platform. The custom-made kernel and C library work together to implement the POSIX API, while cleaning up or removing the messy parts, and adding more high-level frameworks on top of that. It is a modernized, graphical operating system which tries to follow UNIX principles to a reasonable degree. It is designed to be easy to use for users, and easy to program for for developers.


## Disk image root
The `hdd-sysroot` directory contains files which will be placed into the root of the hard drive image generated by the distro build script, after everything has been installed. Use this for configuration files, such as users and groups (`/etc/passwd`, `/etc/group`, etc), as well as to overwrite any files installed by default.

## Building a distro
To build a distro and create a bootable hard drive image, run the `build-dist.sh` script in a directory outside of the repository, for example:

``../glidix/build-dist.sh``

This will build all components of the default distribution, and generate a `distro-out` directory with the resulting build, which contains the following files:

*`hdd.bin` is a 10GB bootable hard drive image. You can write it to a real disk and boot it.
*`glidix.vmdk` is created if the `VBoxManage` command is available (which comes with VirtualBox); this file points to `hdd.bin`, and is compatible with VirtualBox, such that it allows a VM to access the disk image directly.