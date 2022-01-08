#!/bin/bash

export PKG_CONFIG_PATH=`pwd`/json-parser.install/share/pkgconfig
export CFLAGS=`pkg-config --cflags json-parser`
export LDFLAGS=`pkg-config --libs json-parser`

make clean
make
