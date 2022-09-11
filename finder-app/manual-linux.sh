#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # sudo apt install flex
    #sudo apt install bison
    make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} all
    # sudo apt install libssl-dev
    make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} dtbs
    
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir rootfs
cd rootfs
mkdir bin dev etc home lib proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log
#sudo chown -R root:root *

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
sudo env "PATH=$PATH" make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install
cd _install/

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
#make ARCH=$ARCH CROSS_COMPILE=${CROSS_COMPILE} INSTALL_MOD_PATH=${OUTDIR}/rootfs modules_install
cp /usr/lib/x86_64-linux-gnu/libm.so.6 ../rootfs/lib
cp /usr/lib/x86_64-linux-gnu/libresolv.so.2 ../rootfs/lib
cp /usr/lib/x86_64-linux-gnu/libc.so.6 ../rootfs/lib

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp writer ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs/*

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip initramfs.cpio

