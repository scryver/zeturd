#!/bin/sh

set -e

curDir=$(pwd)
codeDir="$curDir/hwsrc"
testDir="$codeDir/testbenches"

buildDir="$curDir/gebouw"
tbDir="$curDir/gebouw/tbs"

if [ -z "$1" ]; then
  sourceFile="$curDir/swsrc/thurd.turd"
else
  sourceFile="$(readlink -f $1)"
fi

./tool_build.sh "$sourceFile"

mkdir -p "$tbDir"

cd "$tbDir" > /dev/null

    ghdl -a "$buildDir/gen_constants.vhd"
    ghdl -a "$codeDir/alu.vhd"
    ghdl -a "$buildDir/gen_controller.vhd"
    ghdl -a "$codeDir/io.vhd"
    ghdl -a "$buildDir/gen_opcodes.vhd"
    ghdl -a "$buildDir/gen_registers.vhd"
    ghdl -a "$buildDir/gen_cpu.vhd"
    ghdl -a "$testDir/tb_cpu.vhd"

    # ghdl -a "$codeDir/constants_and_co.vhd"
    # ghdl -a "$codeDir/alu.vhd"
    # ghdl -a "$codeDir/controller.vhd"
    # ghdl -a "$codeDir/io.vhd"
    # ghdl -a "$codeDir/opcode_auto.vhd"
    # ghdl -a "$codeDir/registers.vhd"
    # ghdl -a "$codeDir/cpu.vhd"
    # ghdl -a "$testDir/tb_cpu.vhd"

    ghdl -e tb_cpu
    ghdl -r tb_cpu --wave=cpu_wave.ghw

cd - > /dev/null
