#!/bin/bash

function usage()
{
	cat <<EOF
Usage:
    test.sh [--64][--nobuild] [--exe=...] [--<game>] <umodel options>

Options:
    --64                    build 64-bit version
    --nobuild               prevent from umodel rebuilding
    --exe=<executable>      do not build when overrided (useful for testing with older exe from svn)
    --<game>                choose predefined game path; <game> = ut2|ut3|gow2 etc
    --debug                 start with -debug option
    --help                  display this help message
    --path=<path>			use instead of -path=... when path has spaces
    -path=<path>            set game path; will disable default game substitution
EOF
	exit
}

# Enable extended pattern matching (for (a|b|c)); see bash manpage, "Pattern matching".
shopt -s extglob

exe=umodel.exe
c_drive=
if [ "$OSTYPE" == "linux-gnu" ] || [ "$OSTYPE" == "linux" ]; then
	exe="umodel"
	# VirtualBox default C: drive mapping
	if [ -d "/media/sf_C_DRIVE" ]; then
		c_drive="/media/sf_C_DRIVE"			# VirtualBox
	elif [ -d "/mnt/hgfs/C" ]; then
		c_drive="/mnt/hgfs/C"				# VMware
	elif [ -d "/media/c" ]; then
		c_drive="/media/c"
	fi
fi

#function DBG() { echo "LOG: $*"; }
function DBG() { return; }

#------------------------------------------------------------------------------
#	Game tools
#------------------------------------------------------------------------------

foundPath=
debugOpt=

# Steam support:
# - add other paths inside (), separate with spaces. add trailing slash
# - use as
#   CheckDir "${steam[@]/%/Some Game Name}"
#   (this will append "Some Game Name" to all variants of steam path)
steam=("c:/Program Files (x86)/Steam/SteamApps/common/")

function CheckDir()
{
	[ "$foundPath" ] && return
	local checkedDirs=""
	while [ $# -gt 0 ]; do
		dir=$1
		DBG "... check $dir"
		# support Windows paths on Linux
		if [ "$c_drive" ] && [ "${dir:1:1}" == ":" ]; then
			dir="${c_drive}${dir:2}"
		fi
		if [ -z "$checkedDirs" ]; then
			checkedDirs=$dir
		else
			checkedDirs="$checkedDirs, $dir"
		fi
		if [ -d "$dir" ]; then
			foundPath=$dir
			return			# completed
		fi
		shift
	done
	# error
	echo "ERROR: the game was not found at ($checkedDirs)"
	exit
}

function console_title()
{
	# set console title if outout is not redirected to file
	# http://rosettacode.org/wiki/Check_output_device_is_a_terminal
	[ -t 1 ] && echo -en "\033]2;$@\007"
}

function run()
{
	if [ "$foundPath" ]; then
		console_title "$exe -path="$foundPath" $@"
		echo "Starting $exe -path="$foundPath" $@"
		./$exe -path="$foundPath" $debugOpt $@
	else
		console_title "$exe $debugOpt $@"
		echo "Starting $exe $debugOpt $@"
		./$exe $debugOpt $@
	fi
}

# This function accepts game path as the 1st argument.
# Brace expansion is performed when "{a,b,c}" is used inside. Note: when a few
# space-separated paths are provided without a braces ("path1 path2 path3") - this
# would not work because we cannot determine whether to split this to a few paths
# or use as a single path with spaces.
function run1()
{
	case "$1" in
	*,*)
		DBG "Group: $1"
		eval CheckDir $1		# 'eval' is required in order to perform brace expansion
		;;
	*)
		DBG "Single: $1"
		CheckDir "$1"			# no 'eval' required, check single path
		;;
	esac

	shift
	run $*
}


# all following functions are called as "--func" argument
# example: test.sh --ut2 HumanMaleA

function gui()
{
	run $*
}

function u1()
{
	CheckDir {c,d,e}:/games/{unreal,Unreal\ Anthology}/UnrealGold
	run $*
}
function ut1()
{
	CheckDir {c,d,e}:/games/{unreal,Unreal\ Anthology}/UnrealTournament data/1/UT
	run $*
}
function ut2()
{
	CheckDir {c,d,e}:/games/{unreal,Unreal\ Anthology}/ut2004 data/2/UT2004
	run $*
}
function ut3()
{
	CheckDir "${steam[@]/%/Unreal Tournament 3}" {c,d,e}:/games/ut3/UTGame/CookedPC data/3/UT3
	run $*
}
function ut4()
{
	CheckDir C:/3-UnrealEngine/UnrealTournamentDev/UnrealTournament/Content/Paks
#	CheckDir C:/Projects/Epic/UnrealTournament4/UnrealTournament/Saved/Cooked/WindowsNoEditor/UnrealTournament/Content
	run -game=ue4.16 $*
}
function paragon()
{
	CheckDir C:/3-UnrealEngine/Paragon/OrionGame/Content/Paks
	run -game=paragon $*
}
function fortnite()
{
	CheckDir C:/3-UnrealEngine/Fortnite/FortniteGame/Content/Paks
	run -game=ue4.19 $*
}
function ue3()
{
	CheckDir C:/Projects/Epic/UnrealEngine3/UDKGame/Content
	run $*
}
function ue4()
{
	CheckDir C:/Projects/Epic/UnrealEngine4
	run $*
}
function gears4()
{
	CheckDir data/4/GOW4
	run -game=gears4 $*
}
function gow()    { run1 "data/3/GearsOfWar" $*;          }
function gow2()   { run1 "data/3X/GearsOfWar2_X360" $*;   }
function gow3()   { run1 "data/3X/GOW3_beta_X360" $*;     }
function ib()     { run1 "data/3i/InfinityBlade" -ios $*; }
function uc2()    { run1 "data/2X/UnrealChampionship2" $*;}
function l2()     { run1 "data/2/Lineage2" $*;            }
function bio()
{
	CheckDir {c,e}:/GAMES/BioShock data/2/Bioshock
	run $*
}
function bio1r()
{
	CheckDir "${steam[0]/%/BioShock Remastered/ContentBaked/pc}" "data/2/Bioshock_remaster"
	run $*
}
function bio2r()
{
	CheckDir "${steam[0]/%/BioShock 2 Remastered/ContentBaked/pc}" "data/2/Bioshock2_remaster"
	run $*
}
function alice()
{
	CheckDir "E:/GAMES/Alice Madness Returns/Alice2/AliceGame/CookedPC"
	run $*
}
function mass()   { run1 "C:/GAMES/Mass Effect/BioGame/CookedPC" $*; }
function mass3()  { run1 "data/3/MassEffect3" $*; }
function scell()  { run1 "data/SplinterCell" $*;  }
function scell2() { run1 "data/SplinterCell2" $*; }
function bat2()
{
	CheckDir "${steam[@]/%/Batman Arkham City GOTY/BmGame}"
	CheckDir "E:/GAMES/Batman Arkham City/BmGame" data/3/Batman2
	run $*
}
function bat3()
{
	CheckDir "${steam[@]/%/Batman Arkham Origins/SinglePlayer/BMGame}"
	CheckDir "E:/GAMES/Batman Arkham City/BmGame" data/3/Batman2
	run $*
}
function tr4()    { run1 "data/3/Tribes4" $*;     }
function thief()  { run1 "${steam[@]/%/Thief/ThiefGame/CookedPCNG}" $*; }
function ark()    { run1 "${steam[@]/%/ARK/ShooterGame/Content}" -game=ue4.6 $*; }
function lawbr    { run1 "${steam[@]/%/LawBreakers/ShooterGame/Content/Paks}" -game=lawbr $*; }

function rund()   {	run1 "data" $*; }


#------------------------------------------------------------------------------

cmd=""
buildopt=
nobuild=
path=0

# parse our command line options
for arg in "$@"; do		# using quoted $@ will allow to correctly separate arguments like [ --path="some string with spaces" -debug ]
#	echo "ARG=$arg"
	case $arg in
	--help)
		usage
		;;
	--64)
		buildopt=vc-win64
		;;
	--nobuild)
		nobuild=1
		;;
	--exe=*)
		exe=${arg:6}
		nobuild=1
		;;
	--debug)
		debugOpt=-debug
		;;
	--path=*)
		path=1
		foundPath=${arg:7}
		DBG "--path '$foundPath'"
		;;
	--*)
		cmd=${arg:2}
		;;
	*)
		DBG "other arg $arg, breaking"
		break
	esac
	DBG "processed $arg"
	shift
done

# rebuild umodel when not desired opposite
if [ -z "$nobuild" ]; then
	console_title "Building umodel ..."
	./build.sh $buildopt || exit 1
fi

# find whether -path= is used or not
for arg in $*; do
	case $arg in
	-path=*)
		DBG "'-path' is specified"
		path=1
		;;
	esac
done

# execute command
if [ $# -gt 0 ] || [ "$cmd" ] || [ $path != 0 ]; then
	# started with arguments
	# extract command from "--cmd" argument
	if [ -z "$cmd" ]; then
		if [ $path == 0 ]; then
			cmd=ut2					# no command and/or path were specified, run "ut2"
		else
			cmd=run
		fi
	fi
	DBG "cmd=$cmd"
	declare -a args
	while [ $# -gt 0 ]; do
		value=${1//\\//}			# replace backslashes with forward slashes
#		if [[ "$value" == @(* *) ]]; then
#			value="\"$value\""
#		fi
#		echo "[$1] -> [$value]"
		args[${#args[@]}]="$value"	# add value to array
		shift
	done
	eval $cmd ${args[*]}			# execute command
	exit
fi


#------------------------------------------------------------------------------
# no arguments

# select path here
case "" in

"")
	run ../Epic/UnrealProjects/ShooterGame/Saved/Cooked/WindowsNoEditor/ShooterGame/Content/Characters/Textures/Chr_FPS_D.uasset
#	run1 "C:/Projects/Epic/UnrealTournament4/UnrealTournament/Content" T_CH_Imm_MalcolmBody_D.uasset
#	run1 data/3/MarvelHeroes UC__MarvelPlayer_Thor_SF.upk -game=mh thor_avengers_diff
#	run1 data/3X/Injustice -noanim CHAR_Ares.xxx -meshes
#	run1 data/2/SplinterCell4 -game=scell -noanim ESam.ukx
#	run1 data/3/Dishonored Startup.upk -meshes -nostat Heart -anim=Heart_as
#	run1 data/3X/Borderlands2 Startup.xxx GestaltDef_AssaultRifle_GestaltSkeletalMesh -anim=AssaultRifle
#	run1 "data/3/Transformers Fall of Cybertron" -meshes L08b_SpitterIntro_DSN_m.xxx RB_GrimlockALT_SKEL -anim=Grimlock_ALT_ANIM
#	bat2 -meshes -pkg=BmGame -pkg=Playable_Catwoman_SF Playable_Catwoman_Std_SF -anim=CW_Combat -obj=Catwoman_Whip_Skin
#	run1 data/3/RiseOfTheArgonauts -meshes -pkg=Jason_Agility_AnimSet.upk Jason_Art.upk
#	gow2 -meshes GearGame.xxx
#	run1 data/3i/EpicCitadel -ios CastleMap.xxx T_FX_Flare_02
#	run1 data/2/BattleTerritoryOnline/new2/AnimPackages WP_SMG_P90_NEWPACKAGE.ebkx P90
#	ib BOSS_10ft_SnS_Thane_SF.xxx
#!!	run1 "data/.possible/Splinter Cell Conviction" Conviction.umd
	# Transformers
#	run1 "data/3/Transformers War for Cybertron" -meshes A1_IAC_DecoPlaza_Design_m.xxx
	# SWRC
#	run1 data/RepublicCommando -noanim BactaDispensers.ukx
#	run1 data/RepublicCommando -noanim Vehicles.ukx
	# Materials
#	ut2 AS_Vehicles_M.ukx minigun_turret
#	ut2 StreamAnims.ukx Dropship
#	ut2 StreamAnims.ukx victim3
#	ut2 AS_VehiclesFull_M.ukx SpaceFighter_Skaarj SkeletalMesh
#	ut2 AS_VehiclesFull_M.ukx ASTurret_MotherShip2 SkeletalMesh
#	ut2 ONSVehicles-A.ukx PRVchassis SkeletalMesh
#	ut2 ONSVehicles-A.ukx AttackCraft SkeletalMesh

	#!! BAD INFLUENCES
#	ut2 -meshes XanRobots.ukx	# note: no errors on LOD models

	# Mass Effect
	# bug: MatInstConst -> MatInstConst -> Mat => loose transparency
#	rund BIOG_HMF_HED_PROMorph_R.upk HMF_HED_PROLash_Mat_1a
#	rund BIOG_HMF_HED_PROMorph_R.upk #HMF_HED_PROCustom_Military_Norm
#	run1 data/MassEffect2 -meshes BioP_Char.pcc

	# Bioshosk
#	bio 0-Lighthouse.bsm MetalVents_NormalMap
#	bio 0-Lighthouse.bsm WallTech_NormalMap
#	bio 0-Lighthouse.bsm ripple_normalmap
#	bio 0-Lighthouse.bsm Rock_NormalMap

#	run1 C:/!umodel-data/MirrorsEdge CH_TKY_Crim_Fixer.upk SK_TKY_Crim_Fixer
#	ut2 HumanMaleA MercMaleD
#	ut3 -meshes CH_AnimHuman
#	ut3 VH_Goliath.upk S_VH_Goliath_FuelCan01b
#	ut3 GP_Conquest SM_Processing_Plant
#	ut3 GP_Onslaught S_GP_Ons_Powernode_Spawn
	# Army of Two
#	run1 C:/!umodel-data/.possible/ArmyOfTwo_X360 -noanim -meshes AO2MPPlayerChar1_SF.xxx
#	run1 data/ArmyOfTwoThe40ThDay -nostat 03_map_penhippos_VFX.xxx
	# MK vs DC
#	run1 C:/!umodel-data/.possible/MKvsDC_X360 -noanim -meshes CHAR_Batman
	# A51
#	run1 C:/!umodel-data/Area51Blacksite a52start
#	run1 C:/!umodel-data/Area51Blacksite enginefonts
#	run1 C:/!umodel-data/Area51Blacksite -noanim -meshes ep7_wrecked_p

	#!! BUGS with GOW animations
#	gow -meshes COG_GasTanker.upk
#	gow -meshes Geist_Beast.upk
#	gow -meshes Locust_Seeder.upk
	#!! CheckBoneTree -> "Strange skeleton" (IK bones?)
#	run1 "C:/!umodel-data/Mass Effect" -list Engine.u
#!!	ut2 -meshes StreamlineMeshes.usx
#!!	ut2 -meshes ONSDeadVehicles-SM.usx
#!!	ut2 -meshes DOM-CBP2-Gerroid.ut2		#!! CRASH
#-	gow -meshes Neutral_Stranded_01.upk NPC01_COG
	#!! CRASH with UE3 animations (assert)
#-	gow -meshes Geist_Reaver.upk
	#!! CRASH with TLR animations (assert) -- have 6 offsets per bone (time keys?)
#	run C:/1/CA0000_MOT_0.upk
#	run C:/1/CA0001_MOT_0.upk
#	run C:/1/CB0220_MOT_0.upk

#!!	ut2 -export -md5 -all HumanMaleA MercMaleD
#	gow2 -meshes geargamehorde_SF

	# anims
#	gow2 -meshes SP_Maria_Cine_Outro
#	gow2 -meshes SP_Rescue_P #T_COG_BOT_Screen_Anya01
#	gow2 -notex -nomesh SP_Rescue_P #COG_Baird_Hair_UV_MERGE_Mat
#	gow2 -meshes SP_Maria_1_S #AnimSetMarcus_CamSkel_Heavy_Mortar

	# check texture formats
#	gow2 SP_Leviathan_BF T_Barge_Grad
#	gow2 SP_Rescue_P T_LightBeam_Falloff_02

#	ut3 CH_AnimHuman #ImportMesh_Human_Male
#	ut3 -meshes CH_TwinSouls_Cine #SK_CH_TwinSouls_Crowd_01
#	run data/ut3/CH_AnimHuman.upk
#	run data/ut3/VH_Fury.upk K_VH_Fury #MI_VH_Fury_Blue
#!!	run data/HumanMale.upk
#	gow COG_MarcusFenix #Cine_COG_MarcusFenix
#	ut3 DM-Deck	#!! CRASH !!
#	ut2 HumanMaleA MercMaleD
#	l2 LineageNPCs2 Pumpkin_Head_man_m00
#	ut2 ONSVehicles-A RV
#	ut2 AS_VehiclesFull_M SpaceFighter_Human SkeletalMesh
#	ut2 AS_VehiclesFull_M
#	l2 Elf FElf_m000_f
	#!!! BUG !!! bad bone influences (all face meshes attached to wrong bone)
#	l2 Orc FOrc_m000_f

#	l2 Elf FElf_m007_u
#	run1 "C:/!umodel-data/Land of the Dead" DOTZAZombies
#	ut2 AW-2k4XP ShockShieldFinal
#	l2 LineageNPCs2
#	run "C:/!umodel-data/Dark Sector 9/PAWNAI.u" OBrien
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
#	Unreal Championship
#------------------------------------------------------------------------------
"uc2")
#	uc2 -noanim T_CharacterSkins
#	uc2 K_Male.ukx
#	uc2 S_NK_Architecture.usx
	uc2 S_FX.usx
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
#	Bioshock
#------------------------------------------------------------------------------
"bio")
	# Bioshock
#	bio -list core
#!!	bio -nomesh 0-Lighthouse #GraniteColorScumbleTight_DIF
#!!	bio -noanim -nostat -notex 0-Lighthouse PustuleBirth_Mesh
	bio ShockGame WP_PistolMesh
	bio 0-Lighthouse ProtectorRosie SkeletalMesh
#	bio 1-Medical.bsm Agg_BabyJane
#!!	bio -notex -noanim -nostat 1-Medical.bsm Agg_Doctor_Mesh
#!!	bio -nomesh -notex 5-Ryan.bsm
	#!! LazyArray erors:
#	bio 1-Medical.bsm ChemThrow_Pickup_Kero_Diffuse
#	bio 1-Medical.bsm ChemThrow_Pickup_IonGel_Diffuse
	#!! SkeletalMesh errors
#	bio 1-Welcome PlaneCrash_Mesh
#	bio 5-Hephaestus LavaFlowSwitch_MESH
	#!! Bad skeleton
#	bio 3-Arcadia CatMESH
	#!! Crash
#	bio 0-Lighthouse lighthouse_tunnel
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
