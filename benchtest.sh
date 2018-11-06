#!/bin/sh

curDir=$(pwd)
codeDir="$curDir/hwsrc"
testDir="$codeDir/testbenches"

buildDir="$curDir/gebouw/tbs"

mkdir -p "$buildDir"

cd "$buildDir" > /dev/null

    ghdl -a "$codeDir/constants_and_co.vhd"
    ghdl -a "$codeDir/alu.vhd"
    ghdl -a "$codeDir/controller.vhd"
    ghdl -a "$codeDir/io.vhd"
    ghdl -a "$codeDir/opcode_auto.vhd"
    ghdl -a "$codeDir/registers.vhd"
    ghdl -a "$codeDir/cpu.vhd"
    ghdl -a "$testDir/tb_cpu.vhd"

    ghdl -e tb_cpu
    ghdl -r tb_cpu --wave=cpu_wave.ghw

cd - > /dev/null
