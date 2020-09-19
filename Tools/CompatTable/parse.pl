#!/usr/bin/perl -w

$embed_css = 1;
$debug_js = 0;
$OUT = "table.html";
$debug = 0;

$JsLog = "opera.postError";				# Opera
#$JsLog = "console.log";				# IE


#------------------------------------------------------------------------------
#	Line parser
#------------------------------------------------------------------------------

$unget_line = "";
$inComment  = 0;

sub getline {
	if ($unget_line ne "")
	{
		local $ret = $unget_line;
		$unget_line = "";
		return $ret;
	}

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
		$line =~ s/\s*\;.*//;			# ; = comment
		# remove traling and leading spaces
		$line =~ s/^[\s\t]+//;
		$line =~ s/[\s\t]+$//;
		# remove cosmetic whitespaces
#		$line =~ s/[\s\t]*([=*+\-,(){}#])[\s\t]*/$1/g;
		$line =~ s/[\s\t]+/\ /g;

		next if $line eq "";
		return 1;
	}
	return 0;
}


sub ungetLine {
	die "multiple ungetLine" if $unget_line ne "";
	$unget_line = $_[0];
}


#------------------------------------------------------------------------------
#	Headers
#------------------------------------------------------------------------------

sub styleSheet {
	my ($file) = $_[0];
	if (!$embed_css) {
		print OUT "<link href=\"$file\" rel=\"stylesheet\" type=\"text/css\" />\n";
	} else {
		print OUT <<EOF
<style type="text/css">
<!--
EOF
;
		open(IN, $file) or die "Unable to read file $file\n";
		while (my $line = <IN>) {
			print OUT $line;
		}
		close(IN);
		print OUT <<EOF
-->
</style>

EOF
;
	}
}


sub tableSearch {
	print OUT <<EOF
<script type="text/javascript">
EOF
;
	my ($file) = $_[0];
	open(IN, $file) or die "Unable to read file $file\n";
	while (my $line = <IN>) {
		if ($debug_js) {
			$line =~ s/DEBUG/$JsLog/g;
		} else {
			# remove debugging stuff
			$line =~ s/DEBUG\(.*\);//g;
			$line =~ s/(\s*\/\/.*)$//;
#			$line =~ s/\s+/ /;
			next if $line =~ /^\s*$/;
		}
		print OUT $line;
	}
	close(IN);
	print OUT <<EOF
</script>

<div class="filter_container">
  <div id="filter_info"></div>
  <form id="filter_form">
    <input name="filt" id="filt_input" placeholder="FILTER" onkeyup="filter(this, 'compat_table', 'filter_info')" type="text" class="filter">
  </form>
  <p />
</div>
EOF
;
}


sub tableHeader {
	print OUT <<EOF
<table id="compat_table" width="100%" border="1" align="left" class="config">
<thead>
  <tr>
    <th width="45">Year</th>
    <th width="301">Title</th>
    <th width="30">Skel</th>
    <th width="30">Tex</th>
    <th width="30">Anim</th>
    <th width="30">Stat</th>
    <th width="64">Engine</th>
    <th>Developer</th>
  </tr>
</thead>
<tbody>
EOF
;
}


sub tableFooter {
    my $supported = $partSupp + $fullSupp;
	print OUT <<EOF
  <tr><td colspan="8">&nbsp;</td></tr>
  <tr>
    <td colspan="8"><table width="100%" border="0" align="center">
      <tr>
        <td style="border-style: none;" width="35">&nbsp</td>
        <td style="border-style: none;"><div align="left" class="detailtxt">
          <p><b>Year:</b> click to open the forum thread that talks about this game.</p>
          <p><b>Title:</b> click to take you to the Wikipedia information for that game. If the Wikipedia page does not exist it will take you to their website or other press release.</p>
          <p><b>Developer:</b> click to open the Wikipedia information for that developer. If the Wikipedia page does not exist it will take you to another relevant page.</p>
          <p><b>Other columns:</b> Mesh: skeletal mesh, Tex: texture, Anim: skeletal animation, Stat: static mesh.</p>
        </div></td>
      </tr>
    </table></td>
  </tr>
  <tr class="footer">
    <td colspan="8">
      Total games: ${supported} supported (${fullSupp} fully), ${notSupp} unsupported
    </td>
  </tr>
</tbody>
</table>
EOF
;
}


#------------------------------------------------------------------------------
#	Company urls
#------------------------------------------------------------------------------

%compUrls = ();

sub readCompanies {
	my ($file) = $_[0];
	open(IN, $file) or die "Unable to read file $file\n";

	while (getline())
	{
		my ($comp, $url) = $line =~ /(.+?) = (\S+)/x;
		$compUrls{$comp} = $url;
	}

	close(IN);
}


#------------------------------------------------------------------------------
#	Main code
#------------------------------------------------------------------------------

# parsing state
@lines = ();
$engine = "";
$year = 0;

sub yesno {
	my ($v) = $_[0];
	return "Yes" if ($v eq "Y");
	return "No" if ($v eq "N");
	return $v;
}

# statistics
$fullSupp = 0;		# number of fully supported games in total
$partSupp = 0;		# number of partially supported games
$notSupp  = 0;		# number of unsupported games

$lastEngine = "";
$yearGames = 0;		# number of supported games in the current year
%stats = ();

sub flushYearStats {
	if ($yearGames) {
		my $line = sprintf("  %d  %3d", $year, $yearGames);
		$yearGames = 0;
		if (defined($line)) {
			push(@{$stats{$engine}}, $line);
		}
	}
	if ($engine ne $lastEngine) {
		$lastEngine = $engine;
		$stats{$engine} = [];
	}
}

sub flushTotalStats {
	my $lineIdx = -1;

	# For each year
	while (1) {
		my $line = "";
		# stats
		my $exists = 0;
		# index = 0 -> print engine name
		for $key (sort(keys %stats)) {
			my $val = "";
			if ($lineIdx == -1) {
				$val = $key;
				$exists = 1;
			} else {
				my $aref = \@{$stats{$key}};
				if ($lineIdx <= $#$aref) {
					$exists = 1;
					$val = $aref->[$lineIdx];
				}
			}
			$line .= sprintf("%-20s", $val);
		}
		last if !$exists;
		print("$line\n");
		#?? engine total
		$lineIdx++;
	}
	printf("\nFound %d supported games (%d fully supported) + %d unsupported\n", $partSupp + $fullSupp, $fullSupp, $notSupp);
}

sub flushGame {
	my ($cmd, $val) = $lines[0] =~ /^ \[ (.+) \s* = \s* (.+) \] $/x;
	if (defined($cmd) && defined($val)) {
		# command
		printf("COMMAND: [%s] = [%s]\n", $cmd, $val) if $debug;
		if ($cmd =~ /engine/i) {
			flushYearStats();
			$engine = $val;
			# Note: putting "class" to 'td' tag to make sticky headers working.
			# Also putting text into "div" block to make it styleable.
print OUT <<EOF
  <td colspan="8" class="spacer"></td>
  <tr><td colspan="8" class="engine"><div>$val</div></td></tr>
EOF
;
		} elsif ($cmd =~ /year/i) {
			flushYearStats();
			$year = $val;
print OUT <<EOF
  <td class="spacer"></td>
  <tr><td colspan="8" class="year"><div>$year</div></td></tr>
EOF
;
		} else {
			die "Unknown command: $cmd\n";
		}
	} else {
		# game
		my ($game) = $lines[0] =~ /\[ \s* (.*) \s* \]/x;
		my $caps = $lines[1];
		printf("GAME: [%s] [%s]\n", $game, $caps) if $debug;
		my ($c1, $c2, $c3, $c4, $ver) = $caps =~ /^ (\S+) \s+ (\S+) \s+ (\S+) \s+ (\S+) \s+ (\S+) $/x;
		my $style = "";
		my $note  = "";
		if (defined($c1) && defined($ver)) {	# everything is defined
			# none
		} else {
			($note, $ver) = $caps =~ /^ (\S+) \s+ (\S+) $/x;
			if ($note =~ /unsupported/i) {
				$c1 = $c2 = $c3 = $c4 = "N";
			#?? can add "all" etc
			} else {
				die "Unknown caps: \"$caps\" ($note)";
			}
		}
		if (($c1 =~ /[-Y]/i) && ($c2 =~ /[-Y]/i) && ($c3 =~ /[-Y]/i) && ($c4 =~ /[-Y]/i)) {
			$style = "game_full";
			$fullSupp = $fullSupp + 1;
			$yearGames++;
		} elsif (($c1 =~ /[-N]/i) && ($c2 =~ /[-N]/i) && ($c3 =~ /[-N]/i) && ($c4 =~ /[-N]/i)) {
			$style = "game_bad";
			$notSupp = $notSupp + 1;
		} else {
			$style = "game_part";
			$partSupp = $partSupp + 1;
			$yearGames++;
		}
		my $company = $lines[2];
		my $url1 = "";
		my $url2 = "";
		if (defined($lines[3])) {
			$url1 = $lines[3];
			if (defined($lines[4])) {
				$url2 = $lines[4];
			}
		}
		$url1 = "" if $url1 eq "-";
		$url2 = "" if $url2 eq "-";
		$newWindow = " target=\"_blank\"";
		if ($style ne "") {
			print OUT "  <tr class=\"$style\">\n";
		} else {
			print OUT "  <tr>\n";
		}
		if ($url2 ne "") {
			print OUT "    <td><a href=\"$url2\"${newWindow}>$year</a></td>\n";
		} else {
			print OUT "    <td>$year</td>\n";
		}

		if ($url1 ne "") {
			print OUT "    <td><a href=\"$url1\"${newWindow}>$game</a></td>\n";
		} else {
			print OUT "    <td>$game</td>\n";
		}
		printf OUT    "    <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>\n", yesno($c1), yesno($c2), yesno($c3), yesno($c4), $ver;
		my $compUrl = $compUrls{$company};
		if (defined($compUrl)) {
			print OUT "    <td><a href=\"$compUrl\"${newWindow}>$company</a></td>\n"
		} else {
			print OUT "    <td>$company</td>\n"
		}
		print OUT "  </tr>\n";

	}

	@lines = ();
}

sub process {
	my ($file) =@_;
	open(IN, $file) or die "Unable to read file $file\n";

	while (getline())
	{
		if ($line =~ /\[.*\]/) {
			if (@lines) {
				# new game or command
				flushGame(@lines);
			}
		}
		push @lines, $line
	}
	flushGame($lines);

	close(IN);
}


readCompanies("developers.ini");

open(OUT, ">$OUT") or die "Unable to create file $OUT\n";
styleSheet("style.css");
tableSearch("filter.js");
tableHeader();

process("table.ini");

tableFooter();
close(OUT);

flushYearStats();
flushTotalStats();
