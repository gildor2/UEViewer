@echo off

rm typeinfo.exe
bash build.sh

rem typeinfo.exe "C:\GAMES\Unreal Anthology\UT2004\System\Core.u"
rem typeinfo.exe "C:\GAMES\Unreal Anthology\UT2004\System\Engine.u"
rem typeinfo.exe C:\!umodel-data\UnrealChampionship2\System\Core.u
rem typeinfo.exe C:\!umodel-data\GearsOfWar2_X360\Core.xxx
rem typeinfo.exe "C:\GAMES\UT3\UTGame\CookedPC\Core.u"
rem typeinfo.exe "C:\GAMES\UT3\UTGame\CookedPC\Engine.u"
typeinfo.exe ..\..\data\3\Batman2\Engine.upk
