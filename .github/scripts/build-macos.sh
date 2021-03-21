#!/bin/bash
set -e -x

source .github/scripts/retry.sh

# General note:
# Apple guarantees forward, but not backward ABI compatibility unless
# the deployment target is set for the oldest supported OS.
# (https://trac.macports.org/ticket/54332#comment:2)

# Used by CMake, clang, and Python's distutils
export MACOSX_DEPLOYMENT_TARGET=$MACOS_MIN_VERSION

# The Python variant to install, see exception below.
export PYTHON_INSTALLER_MACOS_VERSION=$MACOS_MIN_VERSION

# Work-around issue building on newer XCode versions.
# https://github.com/pandas-dev/pandas/issues/23424#issuecomment-446393981
if [ $PYTHON_VERSION == "3.5" ]; then
    # No 10.9 installer available, use 10.6.
    # The resulting wheel platform tags still have 10.6 (=target of Python itself),
    # even though technically the wheel should only be run on 10.9 upwards.
    # This is fixed manually below by renaming the wheel.
    # See https://github.com/pypa/wheel/issues/312.
    export PYTHON_INSTALLER_MACOS_VERSION=10.6
fi

# Install Python
# Note: The GitHub Actions supplied Python versions are not used
# as they are built without MACOSX_DEPLOYMENT_TARGET/-mmacosx-version-min
# being set to an older target for widest wheel compatibility.
# Instead we install python.org binaries which are built with 10.6/10.9 target
# and hence provide wider compatibility for the wheels we create.
# See https://github.com/actions/setup-python/issues/26.
git clone https://github.com/matthew-brett/multibuild.git
pushd multibuild
set +x # reduce noise
source osx_utils.sh
get_macpython_environment $PYTHON_VERSION venv $PYTHON_INSTALLER_MACOS_VERSION
source venv/bin/activate
set -x
popd

# Install dependencies
retry pip install numpy==$NUMPY_VERSION wheel delocate

# List installed packages
pip freeze

export CC=clang
export CXX=clang++
export CFLAGS="-arch x86_64"
export CXXFLAGS=$CFLAGS
export LDFLAGS=$CFLAGS
export ARCHFLAGS=$CFLAGS

# Build wheel
python setup.py bdist_wheel

# Fix wheel platform tag, see above for details.
if [ $PYTHON_VERSION == "3.5" ]; then
    filename=$(ls dist/*.whl)
    mv -v "$filename" "${filename/macosx_10_6_intel/macosx_10_9_x86_64}"
fi

delocate-listdeps --all --depending dist/*.whl # lists library dependencies
delocate-wheel --verbose --require-archs=x86_64 dist/*.whl # copies library dependencies into wheel
delocate-listdeps --all --depending dist/*.whl # verify

# Dump target versions of dependend libraries.
# Currently, delocate does not support checking those.
# See https://github.com/matthew-brett/delocate/issues/56.
set +x # reduce noise
echo "Dumping LC_VERSION_MIN_MACOSX (pre-10.14) & LC_BUILD_VERSION"
mkdir tmp_wheel
pushd tmp_wheel
unzip ../dist/*.whl
echo pyvirtualcam/*.so
otool -l pyvirtualcam/*.so | grep -A 3 LC_VERSION_MIN_MACOSX || true
otool -l pyvirtualcam/*.so | grep -A 4 LC_BUILD_VERSION || true
for file in pyvirtualcam/.dylibs/*.dylib; do
    echo $file
    otool -l $file | grep -A 3 LC_VERSION_MIN_MACOSX || true
    otool -l $file | grep -A 4 LC_BUILD_VERSION || true
done
popd
set -x

# Install pyvirtualcam
pip install dist/*.whl

# Test installed pyvirtualcam
retry pip install -r dev-requirements.txt
# make sure it's working without any required libraries installed
rm -rf $LIB_INSTALL_PREFIX
mkdir tmp
pushd tmp
python -u -m pytest -v -s ../test
popd
