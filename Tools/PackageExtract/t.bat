@echo off

rm extract.exe
bash build.sh

extract.exe %*
