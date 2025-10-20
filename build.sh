#!/bin/bash

cd $(dirname "$BASH_SOURCE[0]")

if [[ "$1" == python* ]]
then
    uv venv --clear --quiet --python "$1"
    shift 1
fi

INC=$(uv run --no-sync python -c 'import sysconfig; print(sysconfig.get_path("include"))')
LIB=$(readlink -f $INC/../../lib)
EXT=$(uv run --no-sync python -c 'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))')

SRC='./src/nanoprof/_sampler.c'
OBJ='./src/nanoprof/_sampler.o'
MOD="./src/nanoprof/_sampler$EXT"

CCFLAGS=(
    -arch arm64
    -mmacosx-version-min=11.0
    -fPIC
    -fwrapv
#   -O3
    -g
    -DNDEBUG
    -Wall
#   -std=gnu23
    -fcolor-diagnostics
    -fansi-escape-codes
    "$@"
)

LDFLAGS=(
    -arch arm64
    -mmacosx-version-min=11.0 
    -undefined dynamic_lookup
#   -lpython3.11
    -bundle
    -fcolor-diagnostics
    -fansi-escape-codes
)

rm -f ./src/nanoprof/*.so

cc "${CCFLAGS[@]}" -I$INC -c $SRC -o $OBJ
cc "${LDFLAGS[@]}" -L$LIB    $OBJ -o $MOD
