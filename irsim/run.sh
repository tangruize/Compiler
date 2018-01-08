#!/bin/bash

set -e

if [ $# -eq 1 ]; then
    SHELL_DIR=$(dirname `realpath $0`)
    $SHELL_DIR/../cmake-build-debug/CompilerLab3 $1 /tmp/output.ir
    PY_APP=$(realpath $SHELL_DIR/irsim.py)
    cd /tmp/
    python $PY_APP output.ir &
    python $PY_APP output.ir.orig.ir
    exit 0
fi

echo $0 cmm_file
exit 1