#!/usr/bin/env bash

bin="bin"

if [[ ! -d "$bin" ]]
then
    mkdir "$bin"
fi

source="p3210265-pizzeria.c"
exe="p3210265-pizzeria"
seed="1000"
ncust="100"

gcc "./$source" -o "./$bin/$exe"
chmod +x "./$bin/$exe"
"./$bin/$exe" "$seed" "$ncust"
