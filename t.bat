@echo off
rm UnLoader.exe
bash build.sh
rem UnLoader.exe data/SkaarjPack_rc.u FlyM
UnLoader.exe data/HumanMaleA.ukx MercMaleB
rem UnLoader.exe -path=data data/MarineModel.ukx MarineMesh_Male
rem UnLoader.exe data/test.ukx Male SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj
