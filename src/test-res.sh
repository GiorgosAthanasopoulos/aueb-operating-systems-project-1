#!/usr/bin/env bash
# test-res.sh
# @ Giorgos Athanasopoulos 2023
set -e

bin="bin"

if [[ ! -d $bin ]]; then
	mkdir $bin
fi

source="p3210265-pizzeria.c"
exe="p3210265-pizzeria"
exe_path="./$bin/$exe"
seed="1000"
ncust="100"
libs="-lpthread -lrt"

gcc ./$source -o $exe_path $libs
chmod +x $exe_path
$exe_path $seed $ncust
