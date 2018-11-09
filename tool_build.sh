#!/bin/bash

set -e

curDir="$(pwd)"
codeDir="$curDir/tools"
buildDir="$curDir/gebouw"

swDir="$curDir/swsrc"

flags="-O0 -g -ggdb -Wall -Werror -pedantic"
exceptions="-Wno-unused-function -Wno-missing-braces"

mkdir -p "$buildDir"

pushd "$buildDir" > /dev/null

clang $flags $exceptions "$codeDir/opcode_generator.c" -o opcode-gen
./opcode-gen "$swDir/simple.turd"
# dot -Tsvg opcodes.dot -o opcodes.svg
dot -Tsvg testing.dot -o opcodes.svg

popd > /dev/null
