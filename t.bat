@echo off
rm UnLoader.exe
bash build.sh
rem UnLoader.exe data/SkaarjPack_rc.u FlyM
rem UnLoader.exe data/HumanMaleA.ukx MercMaleB
rem UnLoader.exe data/MarineModel.ukx MarineMesh_Male
UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj SkeletalMesh
rem UnLoader.exe data/2K4_NvidiaIntro.ukx Intro2k4Skaarj
