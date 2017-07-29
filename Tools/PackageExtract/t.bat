@echo off

rm extract.exe
bash build.sh

rem extract.exe "C:\!UE3-u\Core.u"
rem extract.exe "C:\!UE3-u\CH_Necris.upk"
rem extract.exe C:/!bioshock/Core.U
rem extract.exe "C:\GAMES\Unreal Anthology\UT2004\Animations\HumanMaleA.ukx"
rem extract.exe -list -filter=Texture2D D:\GAMES\UT3\UTGame\CookedPC\Characters\CH_All.upk
extract.exe %*
