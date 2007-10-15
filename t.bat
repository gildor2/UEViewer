@echo off

rm UnLoader.exe
bash build.sh

set ut_path=c:/games/unreal~1/ut2004
if exist %ut_path%/system/ut2004.exe goto path_ok
set ut_path=c:/games/unreal/ut2004
:path_ok
set run=UnLoader.exe -path=%ut_path%

rem goto materials

rem ---------------------------------------------------------------------------

rem %run% HumanFemaleA
rem %run% HumanFemaleA MercFemaleB
%run% SkaarjPack_rc
rem %run% SkaarjPack_rc NaliCow
rem %run% HumanMaleA MercMaleB
rem %run% AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
rem UnLoader.exe -path=data data/MarineModel.ukx MarineMesh_Male
rem UnLoader.exe data/test.ukx Male SkeletalMesh
rem UnLoader.exe -path=c:/games/unreal/ut2004 data/2K4_NvidiaIntro.ukx Intro2k4Skaarj SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj
rem %run% data/HumanMaleA.ukx MercMaleD
goto exit


rem ---------------------------------------------------------------------------
rem 	BUGS
rem ---------------------------------------------------------------------------
:bugs
rem %run% NewWeapons2004 NewTranslauncher_1st
rem %run% HumanMaleA NightMaleB
goto exit


rem ---------------------------------------------------------------------------
rem		CRASH
rem ---------------------------------------------------------------------------
:crash
goto exit


rem ---------------------------------------------------------------------------
rem		SHADERS
rem ---------------------------------------------------------------------------
:shaders
%run% StreamAnims Dropship
%run% Weapons BioRifle_1st SkeletalMesh
goto exit


rem ---------------------------------------------------------------------------
rem		MATERIALS
rem ---------------------------------------------------------------------------
:materials
rem %run% PlayerSkins MercFemaleBBodyA
%run% PlayerSkins MercFemaleBHeadA
goto exit


rem ---------------------------------------------------------------------------

rem UnLoader.exe -list data/SplinterCell/EFemale.ukx

rem ---------------------------------------------------------------------------

:exit
