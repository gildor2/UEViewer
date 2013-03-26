#!/usr/bin/perl -w

#------------------------------------------------------------------------------
#	Unpacker for Unreal engine 3 Android OBB archives
#------------------------------------------------------------------------------


sub ReadInt
{
	my $data;
	sysread(IN, $data, 4) or die "Problem reading file\n";
	return unpack("l", $data);
}

sub ReadInt64
{
	my $i1 = ReadInt();
	my $i2 = ReadInt();
	die "Int64 with non-zero HIDWORD" if $i2 != 0;
	return $i1;
}

sub ReadString
{
	my $data;
	my $len = ReadInt();
	sysread(IN, $data, $len);
	return $data;
}

# recursive mkdir
sub MakeDir
{
	my $fulldir = $_[0];
	my $dir = '.';
	for my $d (split('/', $fulldir))
	{
		$dir .= '/' . $d;
		mkdir($dir, '0777');
	}
}


# open archive
$filename = $ARGV[0];
open(IN, $filename) or die "Unable to open file \"$filename\"\n";
binmode(IN);

# file ID
sysread(IN, $id, 13);
die "Wrong file format\n" if $id ne "UE3AndroidOBB";

# read archive directory
$count = ReadInt();

@filenames = ();
@positions = ();
@lens      = ();

for ($i = 0; $i < $count; $i++)
{
	$filename = ReadString();
	$position = ReadInt64();
	$len      = ReadInt();
	printf("%08X %8X  %20s\n", $position, $len, $filename);
	push @filenames, $filename;
	push @positions, $position;
	push @lens, $len;
}

printf("%d files total\n", $count);


# extract files
for ($i = 0; $i < $count; $i++)
{
	sysseek(IN, $positions[$i], 0);
	sysread(IN, $data, $lens[$i]);
	my $filename = $filenames[$i];
	$filename =~ s/\\/\//g;			# replace slashes
	$filename =~ s/^\.\.\///;		# remove "../" at beginning

	# create a directory for file
	if ($filename =~ /.*\/.*/)
	{
		# has slashes inside, make a directory
		my ($dirname) = $filename =~ /(.*)\/[^\/]+/;
		MakeDir($dirname);
	}


	# write file
	open(F, ">$filename") or die "Unable to create file $filename\n";
	binmode(F);
	syswrite(F, $data);
	close(F);
}

# close archive
close(IN);
