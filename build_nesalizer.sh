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

#echo "Cleaning Build Directory.."
#make clean

if [ -z "$1" ]
then
  	echo "Building NESalizer... (Release)"
	make || exit 2
else
    	echo "Building NESalizer... (Debug)"
	make CONF=DEBUG || exit 2
fi

mv build/nesalizer .
echo "Build Complete!"
