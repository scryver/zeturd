#!/bin/bash

set -e

curDir="$(pwd)"
codeDir="$curDir/tools"
buildDir="$curDir/gebouw"

flags="-O0 -g -ggdb -Wall -Werror -pedantic"
exceptions="-Wno-unused-function -Wno-missing-braces"

mkdir -p "$buildDir"

pushd "$buildDir" > /dev/null

clang $flags $exceptions "$codeDir/opcode_generator.c" -o opcode-gen

popd > /dev/null
