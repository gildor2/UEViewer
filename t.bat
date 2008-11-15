@echo off

rm umodel.exe
bash build.sh

set ut1_path=c:/games/unreal~1/UnrealTournament
if exist %ut1_path%/system/UnrealTournament.exe goto path1_ok
set ut1_path=c:/games/unreal/UnrealTournament
:path1_ok

set ut2_path=c:/games/unreal~1/ut2004
if exist %ut2_path%/system/ut2004.exe goto path2_ok
set ut2_path=c:/games/unreal/ut2004
:path2_ok
set run=umodel.exe -path=%ut2_path%
set run2=umodel.exe -path=data
set run3=umodel.exe -path=data/SplinterCell
set run4=umodel.exe -path=data/SplinterCell2

if "%1%" == "" goto cont
echo %*
%run% %*
exit

:cont

rem umodel D:\22222\Runtime\Animations\TestAnims.ukx
rem umodel -path="C:\!umodel-data\--Land of the Dead--" DOTZAZombies
rem %run% AW-2k4XP ShockShieldFinal
rem umodel.exe -path="C:\!umodel-data\--Lineage2--" LineageNPCs2
rem umodel.exe "C:\!umodel-data\--Dark Sector 9--\PAWNAI.u" OBrien
rem %run% ONSVehicles-A
rem goto exit
rem goto bugs

rem ---------------------------------------------------------------------------

rem %run% -export HumanMaleA MercMaleD
rem %run% -export HumanMaleA BipedMaleA
rem %run2% -export Bender BenderAnims
rem %run% -export HumanMaleA
rem %run% HumanMaleA MercMaleD
rem %run% HumanFemaleA MercFemaleB
%run2% Aida
rem %run% HumanFemaleA
rem %run% Aliens
rem %run% HumanMaleA
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
%run3% ESam
rem %run3% EFemale
rem %run3% ENPC
rem %run3% EminiV
rem %run3% EDog
rem %run4% EIntro
rem %run4% ESam
rem %run4% EDuck
rem %run4% EDog
rem %run4% ENPC
rem %run4% EFemale

goto exit

rem ---------------------------------------------------------------------------
rem 	UNREAL1
rem ---------------------------------------------------------------------------
:unreal1
rem umodel.exe -path=c:/games/unreal/UnrealGold UnrealI
umodel.exe -path=c:/games/unreal/UnrealGold UPak pyramid
rem umodel.exe -path=c:/games/unreal/UnrealGold UnrealI WarlordM
goto exit

:ut1
set run=umodel.exe -path=%ut1_path%
%run% BotPack PylonM
rem %run% UnrealI
rem %run% SkeletalChars
goto exit
%run% BotPack FCommando
%run% BotPack Minigun2m
%run% BotPack ShockCoreM
%run% BotPack WHPick
%run% BotPack Eightm
%run% BotPack Rifle2m
%run% BotPack ctftrophyM
%run% BotPack cdgunmainM
%run% BotPack chainsawM
%run% BotPack muzzEF3
%run% BotPack muzzPF3
%run% BotPack Razor2
%run% BotPack stukkam
goto exit

:ut1x
set run=umodel.exe -path=data/UT1
%run% Grim
rem %run% ut2k4chars malcolm
rem %run% Homer
goto exit

:deusex
set run=umodel.exe -path=data/DeusEx
%run% DeusExCharacters
rem %run% -export DeusExCharacters Mutt
goto exit

:rune
set run=umodel.exe -path=data/Rune
%run% players Ragnar
rem %run% plants
rem %run% objects
rem %run% objects Skull
goto exit

rem ---------------------------------------------------------------------------
rem		TRIBES3
rem ---------------------------------------------------------------------------
:tribes3
set run=umodel.exe -path="C:/GAMES/Tribes - Vengeance/Content/Art"
%run% Vehicles
goto exit

rem ---------------------------------------------------------------------------
rem Harry Potter and the Prisoner of Azkaban
rem ---------------------------------------------------------------------------
:hp3
set run=umodel.exe -path=data/HP3
%run% HPCharacters
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
