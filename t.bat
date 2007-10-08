@echo off
rm UnLoader.exe
bash build.sh
rem UnLoader.exe data/SkaarjPack_rc.u FlyM
rem UnLoader.exe data/HumanMaleA.ukx MercMaleB
rem UnLoader.exe data/HumanFemaleA.ukx MercFemaleB
rem UnLoader.exe data/AS_VehiclesFull_M.ukx SpaceFighter_Human SkeletalMesh
rem UnLoader.exe -path=data data/MarineModel.ukx MarineMesh_Male
rem UnLoader.exe data/test.ukx Male SkeletalMesh
UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj

rem UnLoader.exe -list data/SplinterCell/EFemale.ukx
