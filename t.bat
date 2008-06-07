@echo off

rm umodel.exe
bash build.sh

set ut_path=c:/games/unreal~1/ut2004
if exist %ut_path%/system/ut2004.exe goto path_ok
set ut_path=c:/games/unreal/ut2004
:path_ok
set run=umodel.exe -path=%ut_path%
set run2=umodel.exe -path=data
set run3=umodel.exe -path=data/SplinterCell
set run4=umodel.exe -path=data/SplinterCell2

if "%1%" == "" goto cont
echo %*
%run% %*
exit

:cont

goto scell
rem goto bugs

rem ---------------------------------------------------------------------------

rem %run% -export HumanMaleA MercMaleD
rem %run% -export HumanMaleA BipedMaleA
rem %run2% -export Bender BenderAnims
rem %run% -export HumanMaleA
rem %run% HumanMaleA MercMaleD
rem %run% HumanFemaleA MercFemaleB
rem %run2% Aida
rem %run% HumanFemaleA
rem %run% Aliens
%run% HumanMaleA
rem %run% Bot
rem %run% Jugg

rem %run% SkaarjPack_rc
rem %run% SkaarjPack_rc NaliCow
rem %run% AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
rem %run2% MarineModel MarineMesh_Male
rem %run2% TarjaAnim
rem %run2% test Male SkeletalMesh
rem %run% 2K4_NvidiaIntro Intro2k4Skaarj SkeletalMesh
rem %run2% Bender
rem %run% NewWeapons2004
goto exit


rem ---------------------------------------------------------------------------
rem		SplinterCell
rem ---------------------------------------------------------------------------
:scell
rem %run3% ESam
rem %run3% EFemale
rem %run3% ENPC
rem %run3% EminiV
rem %run4% EIntro
%run4% ESam
rem %run4% EDuck
rem %run4% EDog
rem %run4% ENPC
rem %run4% EFemale
goto exit

rem ---------------------------------------------------------------------------
rem 	BUGS
rem ---------------------------------------------------------------------------
:bugs
rem %run2% AyaneMesh ayane
%run2% Mechanatrix_t mech_two
rem %run2% TarjaAnim SuperFemale
rem %run2% soria soria
rem %run% NewWeapons2004 NewTranslauncher_1st
rem %run% HumanMaleA NightMaleB
rem %run% HumanFemaleA MercFemaleB
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

rem umodel.exe -list data/SplinterCell/EFemale.ukx

rem ---------------------------------------------------------------------------

:exit
