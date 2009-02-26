#!/bin/bash

#------------------------------------------------------------------------------
#	Game tools
#------------------------------------------------------------------------------

function CheckDir()
{
	varname=$1
	shift
	while [ $# -gt 0 ]; do
		dir=$1
		if [ "$OSTYPE" == "linux-gnu" ]; then
			dir="/media/c/${dir:3}"
		fi
		if [ -d "$dir" ]; then
			eval $varname="$dir"
			return
		fi
		shift
	done
}

function run1()							# with path
{
	path=$1
	if ! [ "$path" ]; then
		echo "ERROR: game path was not found"
		exit
	fi
	shift
	echo "Starting umodel -path=\"$path\" $@"
	./umodel -path="$path" $@
}

function run()    { run1 "." $@;     }	# without path

# all following functions are called as "-func" argument
# example: test.sh -ut2 HumanMaleA

function u1()     { run1 "$U1" $*;  }
function ut1()    { run1 "$UT1" $*; }
function ut2()    { run1 "$UT2" $*; }
function ut3()    { run1 "$UT3" $*; }
function rund()   { run1 "data" $*; }
function scell()  { run1 "data/SplinterCell" $*;  }
function scell2() { run1 "data/SplinterCell2" $*; }
function l2()     { run1 "$L2" $*;  }

rm umodel.exe	#?? win32 only
./build.sh

# Unreal1
CheckDir U1 c:/games/unreal~1/UnrealGold c:/games/unreal/UnrealGold
# UT1
CheckDir UT1 c:/games/unreal~1/UnrealTournament c:/games/unreal/UnrealTournament
# UT2
CheckDir UT2 c:/games/unreal~1/ut2004 c:/games/unreal/ut2004
# UT3
CheckDir UT3 c:/games/ut3
# Lineage2
CheckDir L2 "C:/!umodel-data/--Lineage2--"

#------------------------------------------------------------------------------

# handle arguments
if [ $# -gt 0 ]; then
	# started with arguments
	# extract command from "-cmd" argument
	if [ "${1:0:1}" == "-" ]; then
		cmd=${1:1}
		shift
	else
		cmd=ut2			# default command; may be "run" ??
	fi
	eval $cmd $*
	exit
fi


#------------------------------------------------------------------------------
# no arguments

# select path here
case "" in

"")
#	ut3 C:/!UE3-u/CH_Necris.upk
#!!	ut3 C:/GAMES/UT3/UTGame/CookedPC/Characters/CH_AnimHuman.upk ImportMesh_Human_Male
#	run data/ut3/CH_AnimHuman.upk
#!!	run data/ut3/VH_Fury.upk K_VH_Fury #MI_VH_Fury_Blue
	run -path=C:/GAMES/UT3/UTGame/CookedPC/Characters CH_AnimHuman
#!!	run data/HumanMale.upk
#	run C:/!umodel-data/--GearsOfWar--/COG_MarcusFenix.upk Cine_COG_MarcusFenix
#	ut3 -nomesh C:/GAMES/UT3/UTGame/CookedPC/Maps/DM-Deck.ut3
#	run -list C:/!UE3-u/Core.u
#	ut2 HumanMaleA MercMaleD
#	l2 LineageNPCs2 Pumpkin_Head_man_m00
#	ut2 ONSVehicles-A RV
#	ut2 AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
#	ut2 AS_VehiclesFull_M
#	l2 Elf FElf_m000_f
	#!!! BUG !!! bad bone influences (all face meshes attached to wrong bone)
#	l2 Orc FOrc_m000_f

#	l2 Elf FElf_m007_u
#	run1 "C:/!umodel-data/--Land of the Dead--" DOTZAZombies
#	ut2 AW-2k4XP ShockShieldFinal
#	l2 LineageNPCs2
#	run "C:/!umodel-data/--Dark Sector 9--/PAWNAI.u" OBrien
	;;


#------------------------------------------------------------------------------
#	UT2004
#------------------------------------------------------------------------------
"ut2")
#	ut2 -export HumanMaleA MercMaleD
#	ut2 -export HumanMaleA BipedMaleA
#	rund -export Bender BenderAnims
#	ut2 -export HumanMaleA
#	ut2 HumanMaleA MercMaleD
#	ut2 HumanFemaleA MercFemaleB
	rund Aida
#	ut2 HumanFemaleA
#	ut2 Aliens
#	ut2 HumanMaleA
#	ut2 Bot
#	ut2 Jugg

#	ut2 SkaarjPack_rc
#	ut2 SkaarjPack_rc NaliCow
#	ut2 AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
#	rund MarineModel MarineMesh_Male
#	rund TarjaAnim
#	rund test Male SkeletalMesh
#	ut2 2K4_NvidiaIntro Intro2k4Skaarj SkeletalMesh
#	rund Bender
#	ut2 NewWeapons2004
	;;


#------------------------------------------------------------------------------
#	SplinterCell
#------------------------------------------------------------------------------
"scell")
	scell ESam
	scell EFemale
	scell ENPC
	scell EminiV
	scell EDog
	scell2 EIntro
	scell2 ESam
	scell2 EDuck
	scell2 EDog
	scell2 ENPC
	scell2 EFemale
	;;

#------------------------------------------------------------------------------
# 	UNREAL1
#------------------------------------------------------------------------------
"unreal1")
	u1 UnrealI
#	u1 UPak pyramid
#	u1 UnrealI WarlordM
	;;

"ut1")
	ut1 BotPack PylonM
#	ut1 UnrealI
#	ut1 SkeletalChars
#	ut1 BotPack FCommando
#	ut1 BotPack Minigun2m
#	ut1 BotPack ShockCoreM
#	ut1 BotPack WHPick
#	ut1 BotPack Eightm
#	ut1 BotPack Rifle2m
#	ut1 BotPack ctftrophyM
#	ut1 BotPack cdgunmainM
#	ut1 BotPack chainsawM
#	ut1 BotPack muzzEF3
#	ut1 BotPack muzzPF3
#	ut1 BotPack Razor2
#	ut1 BotPack stukkam
	;;

"ut1x")
	p=data/UT1
	run1 $p Grim
	run1 $p ut2k4chars malcolm
	run1 $p Homer
	;;

"deusex")
	p=data/DeusEx
	run1 $p DeusExCharacters
#	run1 $p -export DeusExCharacters Mutt
	;;

"rune")
	p=data/Rune
	run1 $p players Ragnar
#	run1 $p plants
#	run1 $p objects
#	run1 $p objects Skull
	;;

#------------------------------------------------------------------------------
#	TRIBES3
#------------------------------------------------------------------------------
"tribes3")
	run1 "C:/GAMES/Tribes - Vengeance/Content/Art" Vehicles
	;;

#------------------------------------------------------------------------------
#	Harry Potter and the Prisoner of Azkaban
#------------------------------------------------------------------------------
"hp3")
	run1 data/HP3 HPCharacters
	;;


#------------------------------------------------------------------------------
#	BUGS
#------------------------------------------------------------------------------
"bugs")
#	rund AyaneMesh ayane
#	rund Mechanatrix_t mech_two
	rund TarjaAnim SuperFemale
	rund soria soria
#	ut2 NewWeapons2004 NewTranslauncher_1st
#	ut2 HumanMaleA NightMaleB
#	ut2 HumanFemaleA MercFemaleB
	;;


#------------------------------------------------------------------------------
#	CRASH
#------------------------------------------------------------------------------
"crash")
	;;


#------------------------------------------------------------------------------
#	SHADERS
#------------------------------------------------------------------------------
"shaders")
	ut2 StreamAnims Dropship
	ut2 Weapons BioRifle_1st SkeletalMesh
	;;


#------------------------------------------------------------------------------
#	MATERIALS
#------------------------------------------------------------------------------
"materials")
	ut2 PlayerSkins MercFemaleBBodyA
	ut2 PlayerSkins MercFemaleBHeadA
	;;


esac
