#!/bin/bash 

echo asd_test PreBuildEvent start

cd ../../../gtest-1.7.0/make/
make gtest.a
cp -f gtest.a libgtest.a

echo asd_test PreBuildEvent end

