@echo off
rm UnLoader.exe
bash build.sh
set run=UnLoader.exe -path=c:/games/unreal/ut2004

rem goto bugs

%run% HumanFemaleA MercFemaleB
%run% SkaarjPack_rc FlyM
rem %run% SkaarjPack_rc NaliCow
rem %run% HumanMaleA MercMaleB
rem %run% AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
rem UnLoader.exe -path=data data/MarineModel.ukx MarineMesh_Male
rem UnLoader.exe data/test.ukx Male SkeletalMesh
rem UnLoader.exe -path=c:/games/unreal/ut2004 data/2K4_NvidiaIntro.ukx Intro2k4Skaarj SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj
rem %run% data/HumanMaleA.ukx MercMaleD
goto exit

rem --- TEXTURE BUGS ---
:bugs
rem %run% NewWeapons2004 NewTranslauncher_1st
rem %run% HumanMaleA NightMaleB
goto exit

rem --- CRASH ---
:crash
goto exit

rem --- IMPOSTOR ---
:impostor
%run% intro_crowd crowd_d1_a
%run% intro_crowd crowd_d2_a
%run% intro_crowd crowd_d3_a
%run% intro_crowd crowd_d5_a
goto exit


rem --- SHADERS ---
rem %run% SteamAnims Dropship
rem %run% Weapons BioRifle_1st


rem UnLoader.exe -list data/SplinterCell/EFemale.ukx

:exit
