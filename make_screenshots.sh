#!/bin/bash

unameOut="$(uname -s)"
case "${unameOut}" in
    CYGWIN*|MINGW*)
        DIR=Release/
        EXT=.exe
        ;;
esac

case "$PWD" in
    /mnt/c/*)
        DIR=Release/
        EXT=.exe
        ;;
esac

function run_demo {
    ../_bin/${DIR}Demo$1${EXT} --autoexit_delay $2 --autoexit_savesshot demo$1_sshot.bmp
}

mkdir -p _tmp
pushd _tmp

run_demo "1" 2
run_demo "2" 2
run_demo "3" 2
run_demo "4" 2
run_demo "5" 2
run_demo "6" 3.5

mogrify -format png *.bmp

mv *.png ../Docs
rm *.bmp

popd

