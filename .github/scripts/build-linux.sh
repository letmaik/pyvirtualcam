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

if [ ! -z "$GITHUB_ENV" ]; then 
    echo "CODEQL_PYTHON=$PYBIN/python" >> $GITHUB_ENV
    echo "PATH=$PYBIN:$PATH" >> $GITHUB_ENV
fi

# Upgrade pip and prefer binary packages
${PYBIN}/python -m pip install --upgrade pip
export PIP_PREFER_BINARY=1

# install compile-time dependencies
${PYBIN}/pip install numpy==${NUMPY_VERSION}
${PYBIN}/pip install setuptools

# List installed packages
${PYBIN}/pip freeze

# Build pyvirtualcam wheel
export LDFLAGS="-Wl,--strip-debug"
${PYBIN}/python setup.py bdist_wheel --dist-dir dist-tmp

# Bundle external shared libraries into wheel and fix the wheel tags
mkdir dist
auditwheel repair dist-tmp/pyvirtualcam*.whl -w dist
ls -al dist
