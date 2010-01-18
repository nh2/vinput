#!/bin/sh

cd `dirname $0`
rm mpx-client
g++ *.cpp $CPPFLAGS -lXi -Wall -o mpx-client -D MPX
