#!/bin/bash
set -e -x

PYVER=${PYTHON_VERSION//.}

# Create venv and activate
VENV=testsuite
python -m venv env/$VENV
source env/$VENV/bin/activate

# Install pyvirtualcam
pip install dist/pyvirtualcam*cp${PYVER}*macosx*.whl

# Test installed pyvirtualcam
pip install -r dev-requirements.txt
mkdir tmp_for_test
pushd tmp_for_test
python -u -m pytest -v -s ../test
popd

# Exit venv
deactivate
