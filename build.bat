@echo off

cd "C:\code\msbt"

mkdir bin
pushd bin
cl -Zi -Femsbt -EHsc ../src/main.cpp ../src/jsmn.c
popd