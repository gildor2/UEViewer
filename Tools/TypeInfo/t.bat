@echo off

rm typeinfo.exe
bash build.sh

::typeinfo.exe "C:\GAMES\Unreal Anthology\UT2004\System\Core.u"
::typeinfo.exe "C:\GAMES\Unreal Anthology\UT2004\System\Engine.u"
::typeinfo.exe C:\!umodel-data\UnrealChampionship2\System\Core.u
::typeinfo.exe C:\!umodel-data\GearsOfWar2_X360\Core.xxx
::typeinfo.exe "C:\GAMES\UT3\UTGame\CookedPC\Core.u"
::typeinfo.exe "C:\GAMES\UT3\UTGame\CookedPC\Engine.u"
::typeinfo.exe ..\..\data\3\Batman2\Engine.upk
::typeinfo.exe -text ..\..\data\3\Alice\AliceGame.u
::typeinfo.exe -lzo ..\..\data\3\Bioshock3\XCore.xxx
typeinfo ..\..\data\3\Thief\Core.u
