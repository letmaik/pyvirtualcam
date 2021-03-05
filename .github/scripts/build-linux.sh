#!/bin/bash
set -e -x

bash --version

cd /io

source .github/scripts/retry.sh

# List python versions
ls /opt/python

if [ $PYTHON_VERSION == "3.6" ]; then
    PYBIN="/opt/python/cp36-cp36m/bin"
elif [ $PYTHON_VERSION == "3.7" ]; then
    PYBIN="/opt/python/cp37-cp37m/bin"
elif [ $PYTHON_VERSION == "3.8" ]; then
    PYBIN="/opt/python/cp38-cp38/bin"
elif [ $PYTHON_VERSION == "3.9" ]; then
    PYBIN="/opt/python/cp39-cp39/bin"
else
    echo "Unsupported Python version $PYTHON_VERSION"
    exit 1
fi

# Install numpy/scipy deps
retry yum install -y lapack-devel blas-devel

# install compile-time dependencies
retry ${PYBIN}/pip install numpy==${NUMPY_VERSION}

# List installed packages
${PYBIN}/pip freeze

# Build pyvirtualcam wheel
rm -rf wheelhouse
retry ${PYBIN}/pip wheel . -w wheelhouse

# Bundle external shared libraries into wheel
auditwheel repair wheelhouse/pyvirtualcam*.whl -w wheelhouse

# Install package and test
${PYBIN}/pip install pyvirtualcam --no-index -f wheelhouse

retry ${PYBIN}/pip install -r dev-requirements.txt

pushd $HOME
${PYBIN}/pytest -v -s /io/test
popd

# Move wheel to dist/ folder for easier deployment
mkdir -p dist
mv wheelhouse/pyvirtualcam*manylinux*.whl dist/
