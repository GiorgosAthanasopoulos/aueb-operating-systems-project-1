#!/usr/bin/env bash

cwd="$(pwd)"
target="aueb-operating-systems-project-1"

if [[ "${cwd: -${target}}" == "$target" ]] 
then
    echo "Need to be inside root project directory in order to compile!"
fi

bin="bin"

if [[ ! -d "$bin" ]]
then
    mkdir "$bin"
fi

src="src"
source="p3210265.c"
exe="p3210265"
seed="1000"
ncust="100"

gcc "./$src/$source" -o "./$bin/$exe"
chmod +x "./$bin/$exe"
"./$bin/$exe" "$seed" "$ncust"
