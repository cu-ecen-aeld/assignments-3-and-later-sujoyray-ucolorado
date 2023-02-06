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
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-


if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

# check if the directory exists, if not return error
if [ ! -d $OUTDIR ]; then
  echo "Directory doesn't exist"
  exit 1
fi

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
    echo "Building kernel"
    make mrproper
    make defconfig
    make -j24 all
    make dtbs
    cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/ 
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
echo "Creating directory"
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,conf,home/conf,lib64,lib,proc,sbin,sys,tmp,usr,var/log,usr/bin,usr/lib,usr/sbin}


cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    sudo make distclean
    make CONFIG_PREFIX=${OUTDIR}/rootfs defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j24  CONFIG_PREFIX=${OUTDIR}/rootfs
make CONFIG_PREFIX=${OUTDIR}/rootfs install

echo "Library dependencies"
sysroot_path=$(${CROSS_COMPILE}gcc -print-sysroot)


# TODO: Add library dependencies to rootfs
cd "$OUTDIR"/rootfs

# Copying all the library files to make the script future proof.
# Possible optimization could have been done by just adding
# necessary files.

sudo cp -r $sysroot_path/lib64/* lib64
sudo cp -r $sysroot_path/lib/* lib

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd $FINDER_APP_DIR
make clean
make

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh "$OUTDIR"/rootfs/home
cp writer "$OUTDIR"/rootfs/home
cp autorun-qemu.sh "$OUTDIR"/rootfs/home
cp finder-test.sh "$OUTDIR"/rootfs/home
cp conf/username.txt "$OUTDIR"/rootfs/home/conf
cp conf/assignment.txt "$OUTDIR"/rootfs/conf

# TODO: Chown the root directory
pushd ${OUTDIR}/rootfs
sudo chown -R root:root *
popd

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs/ && find . | cpio -H newc -o | gzip -9 > ./../initramfs.cpio.gz
