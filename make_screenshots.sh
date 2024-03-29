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

run_demo "1" 50
run_demo "2" 50
run_demo "3" 50
run_demo "4" 50
run_demo "5" 50
run_demo "6" 110
run_demo "7" 50
run_demo "8" 50
run_demo "9" 10000

mogrify -format png *.bmp

mv *.png ../Docs
rm *.bmp

popd

