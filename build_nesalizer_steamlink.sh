#!/bin/bash
#
echo "Checking For SDK..."
TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	echo "Setting up SDK..."
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
fi
cd "${TOP}"

mkdir build
mkdir build/imgui

if [ -z "$1" ]
then
  	echo "Building NESalizer... (Release)"
	make || exit 2
else
    	echo "Building NESalizer... (Debug)"
	make CONF=DEBUG || exit 2
fi

echo "Packaging it for Steam-Link...."
export DESTDIR="${PWD}/output/nesalizer-steamlink"

rm output/nesalizer
rm output/nesalizer-steamlink/nesalizer
cp build/nesalizer output/nesalizer-steamlink
mv build/nesalizer output/
cd output

# Pack it up
name=$(basename ${DESTDIR})
tar zcvf $name.tgz $name || exit 3

echo "Build Complete! Check in /output/ directory."
