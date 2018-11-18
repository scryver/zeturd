#!/bin/bash

set -e

curDir="$(pwd)"
codeDir="$curDir/tools"
buildDir="$curDir/gebouw"

swDir="$curDir/swsrc"

if [ -z "$1" ]; then
  sourceFile="$swDir/simple.turd"
else
  sourceFile="$(readlink -f $1)"
fi

flags="-O0 -g -ggdb -Wall -Werror -pedantic"
exceptions="-Wno-unused-function -Wno-missing-braces"

mkdir -p "$buildDir"

pushd "$buildDir" > /dev/null

clang $flags $exceptions "$codeDir/opcode_generator.c" -o opcode-gen
./opcode-gen "$sourceFile"
dot -Tsvg tokens.dot -o tokens.svg
dot -Tsvg ast.dot -o ast.svg
dot -Tsvg opcodes.dot -o opcodes.svg
dot -Tsvg testing.dot -o testing.svg
dot -Tsvg program_preop.dot -o program_preop.svg
dot -Tsvg program_postop.dot -o program_postop.svg

popd > /dev/null
