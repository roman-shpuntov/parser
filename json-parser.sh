#!/bin/bash

CURDIR=`pwd`

git clone https://github.com/json-parser/json-parser.git
cd json-parser

./configure --prefix=$CURDIR/json-parser.install
make && make install
