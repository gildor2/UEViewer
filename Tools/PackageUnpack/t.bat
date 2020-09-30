@echo off

rm decompress.exe
bash build.sh

decompress %*
