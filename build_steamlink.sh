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
echo "Cleaning Build Directory.."
make clean
echo "Building NESalizer..."
make $MAKE_J || exit 2
mv build/nesalizer .
echo "Build Complete!"
