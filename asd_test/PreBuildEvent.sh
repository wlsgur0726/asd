#!/bin/bash 

echo asd_test PreBuildEvent start

cd ../../../googletest/googletest/make/
make gtest.a
cp -f gtest.a libgtest.a

echo asd_test PreBuildEvent end
