#!/usr/bin/perl -w

@dirs =
(
	"../../Epic/UnrealEngine4-latest/Engine/Source/Runtime/Core/Public/UObject",
	"../../Epic/UnrealEngine4/Engine/Source/Runtime/Core/Public/UObject",
);

@files =
(
	"ObjectVersion.h",
	"FrameworkObjectVersion.h",
	"EditorObjectVersion.h",
	"../../../Engine/Public/SkeletalMeshTypes.h",	# for newer UE4 (post UE4.18)
	"../../../Engine/Private/SkeletalMesh.cpp",		# for older UE4 version (UE4.18 and less)
	"../../../Engine/Public/SkeletalMeshLegacyCustomVersions.h", # UE4.27 and newer
	"CoreObjectVersion.h",
	"RenderingObjectVersion.h",
	"AnimObjectVersion.h",
	"AnimPhysObjectVersion.h",
	"ReleaseObjectVersion.h",
	"EnterpriseObjectVersion.h",
);

sub DBG() {0}

$dump = 0;
$latest = 0;

sub getline0
{
	while ($line = <IN>)
	{
		# remove CR/LF
		$line =~ s/\r//;
		$line =~ s/\n//;
		# remove comments
		$line =~ s/\s*\/\/.*//;				# // ...
		$line =~ s/\s*\/\*.*\*\/\s*//;		# /* ... */
		# remove leading and trailing spaces (again)
		$line =~ s/^\s*//;
		$line =~ s/\s*$//;
		# replace all multiple spaces with a single one
		$line =~ s/\s\s+/ /g;
		# ignore empty lines
		next if $line eq "";
		return 1;
	}
	return 0;
}

sub ParseVersionFile
{
	my $verfionsCpp = $_[0]."/".$_[1];
	open(IN, $verfionsCpp) || return; #die "can't open file $verfionsCpp\n";

	my $namespace = "";
	while (getline0())
	{
		if ($line =~ /struct\s+\w+/)
		{
			(undef, $namespace) = $line =~ /^struct \s+ ([A-Z]+_API\s+)? (\w+)/x;
			next;
		}
		if ($line =~ /^enum\s*\w+/)
		{
			last;
		}
		print("SKIP: $line\n") if DBG;
	}
	return if !defined($line) || $line eq "";

	my ($enumName) = $line =~ /enum\s+(\w+)/;
	my (undef, $shortFileName) = $_[1] =~ /(.*\/)?([\w\.]+)/;
	my $fullEnumName = ($namespace eq "") ? $enumName : "$namespace\:\:$enumName";
	print("[$shortFileName : $fullEnumName]\n") if $dump || $latest;

	if ($line !~ ".*\{")
	{
		getline0();
		if ($line !~ /\{/)
		{
			print("Unable to parse $_[1]\n");
			return;
		}
	}

	$value = 0;

	my ($prevName, $prevValue);
	while (getline0())
	{
		if ($line =~ /^\}/)
		{
			last;
		}
		my ($name, undef, $val) = $line =~ /^ (\w+) (\s* \= \s* (\w+))? (\,)? $/x;
		if (defined($val))
		{
			$value = $val;
		}

		if (!defined($name))
		{
			print("Bad: $line\n") if DBG;
			last;
		}

		if ($name =~ /VERSION_PLUS_ONE|VersionPlusOne/)
		{
			# this is the last version constant, automatic one, ignore it
			last;
		}

		print("ENUM: $name = $value ($line)\n") if DBG;

		if (uc($findConst) eq uc($name))
		{
			print("    $findConst = $value ($_[1])\n");
			$constFound = 1;
		}
		else
		{
			print("    $name = $value\n") if $dump;
		}

		$prevName = $name;
		$prevValue = $value;
		$value = $value + 1;
	}

	print "  $prevName = $prevValue\n" if $latest;
	print "\n" if $dump;
}

if (!defined($ARGV[0]))
{
	print("Usage: UE4Version [--dump] | [--latest] | [constant name]\n");
	exit(0);
}
elsif ($ARGV[0] eq "--dump")
{
	$dump = 1;
}
elsif ($ARGV[0] eq "--latest")
{
	$latest = 1;
}
else
{
	$findConst = $ARGV[0];
}

$constFound = !defined($findConst);
$firstDirectory = 1;

for my $d (@dirs)
{
	if (!$firstDirectory)
	{
		print("\n#" . "-" x 120 . "\n");
	}
	print "#\n# Scanning directory $d:\n#\n\n";
	$firstDirectory = 0;
	for my $f (@files)
	{
		ParseVersionFile($d, $f);
	}
	print "\n" if !$dump;
}

print("Constant $findConst was not found!\n") if !$constFound;
