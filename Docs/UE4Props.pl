#!/usr/bin/perl -w

# TODO:
# - command line (has some commented code here)
# - show file/line where class defined
# - command line: show particular class and its parents
# - when there's parent, parse its prpos and add count to the 1st property (e.g. for UMaterialInstanceConstant)


@dirs =
(
	"../../Epic/UnrealEngine4/Engine/Source/Runtime/Engine",
#	"../../Epic/UnrealEngine4-latest/Engine/Source/Runtime/Engine",
);

@files =
(
	"Public/Components.h",
	"Public/BoneContainer.h",
	"Classes/Engine/StreamableRenderAsset.h",
	"Classes/Engine/StaticMesh.h",
	"Classes/Engine/SkeletalMesh.h",
	"Classes/Engine/SkeletalMeshSampling.h",
	"Public/Animation/SkinWeightProfile.h",
	"Classes/Engine/Texture.h",
	"Classes/Engine/Texture2D.h",
	"Classes/Engine/TextureCube.h",
	"Classes/Materials/MaterialInterface.h",
	"Classes/Materials/MaterialInstanceBasePropertyOverrides.h",
	"Classes/Materials/MaterialLayersFunctions.h",
	"Public/StaticParameterSet.h",
	"Public/MaterialCachedData.h",
	"Classes/Materials/MaterialInstance.h",
	"Classes/Materials/Material.h",
	"Classes/Materials/MaterialInstanceConstant.h",
	"Classes/Animation/Skeleton.h",
	"Classes/Animation/AnimationAsset.h",
	"Classes/Animation/AnimSequenceBase.h",
	"Classes/Animation/AnimSequence.h",
	"Classes/Animation/CustomAttributes.h",
	"Classes/Curves/StringCurve.h",
	"Classes/Curves/IntegralCurve.h",
	"Classes/Curves/SimpleCurve.h",
	"Classes/Animation/AnimLinkableElement.h",
	"Classes/Animation/AnimNotifies/AnimNotify.h",
	"Public/Animation/AnimTypes.h",
);

%consts =
(
	"EPhysicalMaterialMaskColor::MAX" => 8,
	"NumMaterialRuntimeParameterTypes" => 5,
);

sub DBG() {0}

#$dump = 0;
#$latest = 0;

%classProps = ();

sub getline0
{
	while ($line = <IN>)
	{
		$lineNo++;
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

sub ParseClass
{
	my ($type, $className, $parent, $text) = @_;

	my $num = 0;

	if (defined($parent))
	{
		$text .= "$type $className : public $parent\n{\n";
		if (exists($classProps{$parent}))
		{
			# Don't adjust property indices: parent class is always serialized next
			# because last PropertyLinkNext of the class points to the first property
			# of parent class.
			#$num = $classProps{$parent};
		}
	}
	else
	{
		$text .= "$type $className\n{\n";
	}

	my $brace = -1;
	while (getline0())
	{
		# Handle nested structures
		if ($line eq "{")
		{
			$brace++;
			while ($brace > 0 && getline0())
			{
				if ($line eq "{")
				{
					$brace++;
				}
				elsif ($line =~ /^}/)
				{
					$brace--;
				}
			}
			next;
		}

#		print "## $line\n";
		last if $line =~ "};.*";
		if ($line =~ "#if WITH_EDITOR")
		{
			while (getline0())
			{
				last if $line =~ "#endif";
			}
			next;
		}
		if ($line =~ "UPROPERTY")
		{
			getline0();
			#!! todo: static arrays are not parsed
			my ($countValue) = $line =~ /.*\[(\S+)\]/;
			my $increment = 1;
			if (defined($countValue))
			{
				if ($countValue =~ /^[\d]+$/)
				{
					# numeric value
					$increment = $countValue;
				}
				elsif (exists($consts{$countValue}))
				{
					$increment = $consts{$countValue};
					$text .= "    // $countValue = $increment\n";
				}
				else
				{
					$text .= "    // Warning: array of unknown size $countValue\n";
				}
			}
			$text .= "    /* $num */ $line\n";
			$num += $increment;
		}
	}

	if ($num)
	{
		print($text);
		print("    /* $num properties */\n");
		print("};\n");
		$classProps{$className} = $num;
	}
}

sub ParseHeaderFile
{
	$header = $_[0]."/".$_[1]; # global variable
	my $filename = $_[1];
	$lineNo = 0;
	open(IN, $header) || return;

	print("//" . "-" x 120 . "\n");
	print("//  $filename\n");
	print("//" . "-" x 120 . "\n");

	while (getline0())
	{
		my $classLine = $lineNo;
		if ($line =~ /(struct|class)\s+\w+[^;]*$/)
		{
			# struct|class
#			my ($type, undef, $namespace) = $line =~ /^(struct|class) \s+ ([A-Z]+_API\s+)? (\w+)/x;
			my ($type, undef, $namespace, undef, undef, $parent) = $line =~ /^(struct|class) \s+ ([A-Z]+_API\s+)? (\w+) (\s* : \s* (public\s+)? (\w+) )?/x;
			if (defined($namespace))
			{
				my $comment = "\n// $line\n";
				$comment .= "// $filename:$classLine\n";
				ParseClass($type, $namespace, $parent, $comment);
			}
			next;
		}
		print("SKIP: $line\n") if DBG;
	}
}

#if (!defined($ARGV[0]))
#{
#	print("Usage: UE4Props [no options]\n");
#	exit(0);
#}
#elsif ($ARGV[0] eq "--dump")
#{
#	$dump = 1;
#}
#elsif ($ARGV[0] eq "--latest")
#{
#	$latest = 1;
#}
#else
#{
#	$findConst = $ARGV[0];
#}

print "#include // make C++ syntax highlighting\n";

for my $d (@dirs)
{
	print("\n//\n// $d:\n//\n\n");
	for my $f (@files)
	{
		ParseHeaderFile($d, $f);
		print("\n");
	}
}
