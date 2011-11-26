@echo off

rm decompress.exe
bash build.sh

set decfile=C:\!umodel-data\Borderlands\W_Startup_INT.upk
::set C:\!umodel-data\Bioshock\Core.U
::set C:\!umodel-data\Lineage2\animations\LineageDecos.ukx

if not "%1"=="" set decfile="%1"
decompress %decfile%
