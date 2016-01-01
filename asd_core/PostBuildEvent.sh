#!/bin/bash 
# ${1} : src file
# ${2} : target directory

echo asd PostBuildEvent start

mkdir -p "${2}"/
cp -f "${1}" "${2}"/

echo asd PostBuildEvent end
