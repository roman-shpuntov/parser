#!/bin/bash

export LD_LIBRARY_PATH=`pwd`/json-parser.install/lib:$LD_LIBRARY_PATH

#gdb --args 
./parser
