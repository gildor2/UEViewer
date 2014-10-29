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

$totalLen = 0;
for ($i = 0; $i < $count; $i++)
{
	$totalLen += $lens[$i];
}

printf("%d files total, %.1f MBytes\n", $count, $totalLen / (1024*1024));


# extract files
$extractedLen = 0;
for ($i = 0; $i < $count; $i++)
{
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

	sysseek(IN, $positions[$i], 0);

	# write file
	open(F, ">$filename") or die "Unable to create file $filename\n";
	binmode(F);

	$remainingLen = $lens[$i];
	# copy streams in 64k chunks
	do
	{
		$len = $remainingLen;
		$len = 65536 if $len > 65536;
		sysread(IN, $data, $len);
		syswrite(F, $data);
		$remainingLen -= $len;
		# progress indicator
		$extractedLen += $len;
		printf("Extracting: %.1f%%\r", ($extractedLen / $totalLen) * 100);
	} while ($remainingLen > 0);	# allow zero-sized files

	close(F);
}

printf("\n");

# close archive
close(IN);
