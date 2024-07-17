#!/usr/bin/env bash
set -e # stop on first error
# doing flags as array allows to comment them out
CFLAGS=(
    #"-std=c89"
    "-Wall"
    "-Wextra"
    "-Wshadow"
    "-Wswitch-enum"
   #"-Wmissing-declarations"
    "-Wno-vla" # variable length arrays
    "-Wno-deprecated-declarations"
    "-Wno-misleading-indentation"
    "-pedantic"
    "-ggdb"
)
cc main.c "${CFLAGS[@]}" -o a.out
