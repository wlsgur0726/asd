#!/bin/bash 

echo asd_redis PreBuildEvent start

cd ../../hiredis/
make static

echo asd_redis PreBuildEvent end