#!/bin/bash
set -e -x

# List python versions
ls /opt/python

# Compute PYBIN from PYTHON_VERSION (e.g., "3.14" -> "cp314-cp314")
PYVER_NO_DOT=${PYTHON_VERSION//./}
PYBIN="/opt/python/cp${PYVER_NO_DOT}-cp${PYVER_NO_DOT}/bin"

if [ ! -d "$PYBIN" ]; then
    echo "Python version $PYTHON_VERSION not found at $PYBIN"
    exit 1
fi
PYVER=${PYTHON_VERSION//.}

# Install package and test
${PYBIN}/pip install ./dist/pyvirtualcam*cp${PYVER}*manylinux*.whl

${PYBIN}/pip install -r dev-requirements.txt

mkdir tmp_for_test
pushd tmp_for_test
# NOTE: TESTING DISABLED!
# The v4l2loopback kernel module cannot be installed as it depends
# on v4l2 (videodev) kernel support.
# videodev can either be built into the kernel directly or supported
# as loadable module.
# Azure (which is what GitHub Actions uses) only offers videodev
# in linux-modules-extra-azure starting from Ubuntu 20.10.
# See https://packages.ubuntu.com/search?suite=groovy&arch=any&mode=exactfilename&searchon=contents&keywords=videodev.ko
# However, GitHub Actions only uses LTS releases of Ubuntu and they
# are currently at 20.04 LTS. The next LTS will be 22.04 LTS
# which will come out in 2022.
#${PYBIN}/pytest -v -s /io/test
popd
