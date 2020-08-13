#!/bin/bash

PYTHON_PATH=`which python`
PYTHON_BIN_DIR=`dirname $PYTHON_PATH`
PYTHON_HOME=`dirname $PYTHON_BIN_DIR`

export LD_LIBRARY_PATH="$PYTHON_HOME/lib/hikvision:$LD_LIBRARY_PATH"

python setup.py build_ext --inplace
python mytest.py

#python app.py