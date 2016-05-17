@echo off

cd "C:\code\msbt"

mkdir bin
pushd bin
cl -Zi -Femsbt ../src/main.cpp
popd