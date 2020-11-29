#!/usr/bin/perl -w

@dirs =
(
	"../../Epic/UnrealEngine4-latest/Engine/Source/Runtime/Engine/Classes/Engine",
);

@files =
(
	"StaticMesh.h",
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

sub ParseClass
{
	my ($type, $className) = @_;

	print("$type $className\n{\n");

	my $num = 0;
	while (getline0())
	{
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
			print("    /* $num */ $line\n");
			$num++;
		}
	}

	print("};\n");
}

sub ParseHeaderFile
{
	my $header = $_[0]."/".$_[1];
	open(IN, $header) || return;

	while (getline0())
	{
		if ($line =~ /(struct|class)\s+\w+[^;]*$/)
		{
			# struct|class
			my ($type, undef, $namespace) = $line =~ /^(struct|class) \s+ ([A-Z]+_API\s+)? (\w+)/x;
			print "\n// $line\n";
			if (defined($namespace))
			{
				ParseClass($type, $namespace);
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
		ParseHeaderFile($d, $f);
	}
	print "\n" if !$dump;
}
