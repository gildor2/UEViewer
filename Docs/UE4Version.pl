#!/usr/bin/perl -w

$verfionsCpp = "C:/Projects/Epic/UnrealEngine4-latest/Engine/Source/Runtime/Core/Public/UObject/ObjectVersion.h";

sub DBG() {0}

sub getline0 {
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


open(IN, $verfionsCpp) || Error ("can't open infile $verfionsCpp");

while (getline0())
{
	if ($line =~ /^enum\s*\w+/) {
		last;
	}
	print("SKIP: $line\n") if DBG;
}

if ($line !~ ".*\{") {
	getline0();
	die "Bad format" if $line !~ /\{/;
}

$value = 0;
$findConst = $ARGV[0];

if (!defined($findConst)) {
	print("Please supply constant name as argument\n");
	exit(1);
}

while (getline0())
{
	if ($line =~ /^\}/) {
		last;
	}
	my ($name, undef, $val) = $line =~ /^ (\w+) (\s* \= \s* (\w+))? (\,)? $/x;
	if (defined($val)) {
		$value = $val;
	}

	if (!defined($name)) {
		print("Bad: $line\n") if DBG;
		last;
	}
	print("ENUM: $name = $value ($line)\n") if DBG;

	if (uc($findConst) eq uc($name)) {
		print("$findConst = $value\n");
		exit(0);
	}

	$value = $value + 1;
}

print("$findConst was not found!\n");
