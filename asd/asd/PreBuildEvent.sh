#!/bin/bash 

echo asd PreBuildEvent start

cd ../src
if [ -e stdafx.h.gch ]; then
	g++ -std=c++11 stdafx.h
fi

echo asd PreBuildEvent end

