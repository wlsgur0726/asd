#!/bin/bash 

echo asd PreBuildEvent start

cd ../src
g++ -I../include -std=c++11 stdafx.h

echo asd PreBuildEvent end

