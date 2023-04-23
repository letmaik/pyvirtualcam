#!/bin/bash
set -e -x

# List python versions
ls /opt/python

if [ $PYTHON_VERSION == "3.7" ]; then
    PYBIN="/opt/python/cp37-cp37m/bin"
elif [ $PYTHON_VERSION == "3.8" ]; then
    PYBIN="/opt/python/cp38-cp38/bin"
elif [ $PYTHON_VERSION == "3.9" ]; then
    PYBIN="/opt/python/cp39-cp39/bin"
elif [ $PYTHON_VERSION == "3.10" ]; then
    PYBIN="/opt/python/cp310-cp310/bin"
elif [ $PYTHON_VERSION == "3.11" ]; then
    PYBIN="/opt/python/cp311-cp311/bin"
else
    echo "Unsupported Python version $PYTHON_VERSION"
    exit 1
fi

# install compile-time dependencies
${PYBIN}/pip install numpy==${NUMPY_VERSION}

# List installed packages
${PYBIN}/pip freeze

# Build pyvirtualcam wheel
export LDFLAGS="-Wl,--strip-debug"
${PYBIN}/python setup.py bdist_wheel --dist-dir dist-tmp

# Bundle external shared libraries into wheel and fix the wheel tags
mkdir dist
auditwheel repair dist-tmp/pyvirtualcam*.whl -w dist
ls -al dist
