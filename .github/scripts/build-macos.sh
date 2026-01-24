#!/bin/bash
set -e -x

# General note:
# Apple guarantees forward, but not backward ABI compatibility unless
# the deployment target is set for the oldest supported OS.
# (https://trac.macports.org/ticket/54332#comment:2)

# Used by CMake, clang, and Python's distutils
export MACOSX_DEPLOYMENT_TARGET=$MACOS_MIN_VERSION

# The Python variant to install, see exception below.
export PYTHON_INSTALLER_MACOS_VERSION=$MACOS_MIN_VERSION

# Install Python
# Note: The GitHub Actions supplied Python versions are not used
# as they are built without MACOSX_DEPLOYMENT_TARGET/-mmacosx-version-min
# being set to an older target for widest wheel compatibility.
# Instead we install python.org binaries which are built with 10.6/10.9 target
# and hence provide wider compatibility for the wheels we create.
# See https://github.com/actions/setup-python/issues/26.
git clone https://github.com/multi-build/multibuild.git
pushd multibuild
set +x # reduce noise
source osx_utils.sh
get_macpython_environment $PYTHON_VERSION venv $PYTHON_INSTALLER_MACOS_VERSION
source venv/bin/activate
set -x
popd

# Upgrade pip and prefer binary packages
python -m pip install --upgrade pip
export PIP_PREFER_BINARY=1

# Install dependencies
pip install numpy==$NUMPY_VERSION wheel delocate setuptools

# List installed packages
pip freeze

# By default, wheels are tagged with the architecture of the Python
# installation, which would produce universal2 even if only building
# for x86_64. The following line overrides that behavior.
export _PYTHON_HOST_PLATFORM="macosx-${MACOS_MIN_VERSION}-${PYTHON_ARCH}"

export CC=clang
export CXX=clang++
export CFLAGS="-arch ${PYTHON_ARCH}"
export CXXFLAGS=$CFLAGS
export LDFLAGS=$CFLAGS
export ARCHFLAGS=$CFLAGS

# Build wheel
python setup.py bdist_wheel

# Note: The following is generic code and copied from the rawpy project.
# Strictly speaking it is not needed here since currently there are
# no shared library dependencies that have to be bundled.

delocate-listdeps --all --depending dist/*.whl # lists library dependencies
delocate-wheel --verbose --require-archs=${PYTHON_ARCH} dist/*.whl # copies library dependencies into wheel
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
