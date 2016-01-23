#!/usr/bin/perl -w

#?? take "OUT" from commandline + take directory from commandline
$EXTS = "vert|frag|ush";
$OUT  = "../Shaders.h";

$inComment = 0;

sub getline {
	while ($line = <IN>)
	{
		# remove CR/LF
		$line =~ s/\r//;
		$line =~ s/\n//;
		# remove comments
		if ($inComment)
		{
			if ($line =~ /.*\*\//)		# close comment
			{
				$line =~ s/^.*\*\///;
				$inComment = 0;			# ... and continue parsing here
			}
			else
			{
				$line = "\\n";			# continue comment
				return 1;
			}
		}
		$line =~ s/\/\*.*\*\///g;		# remove /* ... */ in a single line
		if ($line =~ /\/\*/)
		{
			$inComment = 1;				# open comment
			$line =~ s/\/\*.*$//;		# ... and continue parsing to remove extra spaces etc
		}
#		$line =~ s/\/\*.*$//;
		$line =~ s/\s*\/\/.*//;
		# remove traling and leading spaces
		$line =~ s/^[\s\t]+//;
		$line =~ s/[\s\t]+$//;
		# remove cosmetic whitespaces
		$line =~ s/[\s\t]*([=*+\-,(){}#])[\s\t]*/$1/g;
		$line =~ s/[\s\t]+/\ /g;
		# replace escape chars
		$line =~ s/\t/\\t/g;
		$line =~ s/\"/\\\"/g;
		# add "\n"
		$line .= "\\n";
		return 1;
	}
	return 0;
}


sub process {
	my ($file) =@_;
	open(IN, $file) or die "Unable to read file $file\n";
	my $name = $file;
	$name =~ s/\./_/;
	print(OUT "\n// $file\nstatic const char ${name}[] = \"${file}\\x00\"");
	my $accum = "";
	while (getline())
	{
		if (length($accum) + length($line) < 80)
		{
			$accum .= $line;
			next;
		}
		print(OUT "\n\"$accum\"");
		$accum = $line;
	}
	print(OUT "\n\"$accum\"") if ($accum ne "");
	print(OUT ";\n\n");
	close(IN);
}

# return file modification time
sub FileTime {
	my @s = stat($_[0]) or die "File \"${_[0]}\" was not found\n";
	return $s[9];
}



opendir(DIR, ".");
@filelist = readdir(DIR);
closedir(DIR);

$ThisExec = $0;				# $PROGRAM_NAME does not works
$ExecTime = FileTime($ThisExec);

my $rebuild = 0;
if (!-f $OUT) {
	print STDERR "$OUT does not exists, creating ...\n";
	$rebuild = 1;
} else {
	$OutTime = FileTime($OUT);
	if ($ExecTime > $OutTime) {
		print STDERR "Updated this script, rebuilding $OUT ...\n";
		$rebuild = 1;
	}
}


if ($rebuild == 0) {
	# verify individual file times
	for $f (@filelist)
	{
		if ($f =~ /.*\.($EXTS)$/) {
			my $ShaderTime = FileTime($f);
			if ($ShaderTime > $OutTime) {
				print STDERR "$f is updated, rebuilding $OUT ...\n";
				$rebuild = 1;
			}
		}
	}
}

if ($rebuild == 0) {
#	print STDERR "$OUT is up to date.\n";
	exit;
}

open(OUT, ">$OUT") or die "Unable to create file $OUT\n";
print OUT <<EOF
// Automatically generated file
// Do not modify

EOF
;

for $f (@filelist) {
	if ($f =~ /.*\.($EXTS)$/) {
		process($f);
	}
}

close(OUT);
