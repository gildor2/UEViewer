#!/usr/bin/perl -w

# TODO fixed table headers: jQuery version: http://codepen.io/jgx/pen/wiIGc

$embed_css = 1;
$debug_js = 0;
$OUT = "table.html";
$debug = 0;

$JsLog = "opera.postError";				# Opera
#$JsLog = "console.log";					# IE

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
  <form>
    <input name="filt" placeholder="FILTER" onkeyup="filter(this, 'compat_table', 'filter_info')" type="text" class="filter">
  </form>
  <p />
</div>
EOF
;
}


sub tableHeader {
	print OUT <<EOF
<table id="compat_table" width="100%" border="1" align="left" class="config">
  <tr>
    <td width="35"  class="detailbold">Year</td>
    <td width="301" class="detailbold"><div align="left">Title</div></td>
    <td width="29"  class="detailbold">Mesh</td>
    <td width="24"  class="detailbold">Tex</td>
    <td width="30"  class="detailbold">Anim</td>
    <td width="24"  class="detailbold">Stat</td>
    <td width="60"  class="detailbold">Engine</td>
    <td class="detailbold"><div align="left">Developer</div></td>
  </tr>
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
        <td style="border-style: none; font-size: 8px;"><div align="left">
          <p><span class="detailbold">Year:</span> <span class="detailtxt">By clicking on the year for an entry, it will take you to the thread on the form that talks about this game.</span></p>
          <p><span class="detailbold">Title:</span> <span class="detailtxt">By clicking on the title of a game, it will take you to the Wikipedia information for that game. If the Wikipedia page does not exist it will take you to their website or other press release.</span></p>
          <p><span class="detailbold">Developer:</span> <span class="detailtxt">By clicking on Developer for an entry it will take you to the Wikipedia information for that developer. If the Wikipedia page does not exist it will take you to their website or other related information.</span></p>
          <p><span class="detailbold">Other columns:</span><span class="detailtxt"> Mesh: skeletal mesh, Tex: texture, Anim: skeletal animation, Stat: static mesh.</span></p>
        </div></td>
      </tr>
    </table></td>
  </tr>
  <tr bgcolor="#60C060">
    <td colspan="8">
      Total games: ${supported} supported (${fullSupp} fully), ${notSupp} unsupported
    </td>
  </tr>
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

# statistics
$fullSupp = 0;		# number of fully supported games in total
$partSupp = 0;		# number of partially supported games
$notSupp  = 0;		# number of unsupported games
$lastEngine = "";
$yearGames = 0;		# number of supported games in the current year

sub yesno {
	my ($v) = $_[0];
	return "YES" if ($v eq "Y");
	return "NO" if ($v eq "N");
	return $v;
}


sub flushYearStats {
	if ($yearGames) {
		printf("  %d  %3d\n", $year, $yearGames);
		$yearGames = 0;
	}
	if ($engine ne $lastEngine) {
		$lastEngine = $engine;
		printf("\n%s\n", $engine);
	}
}

sub flushGame {
	my ($cmd, $val) = $lines[0] =~ /^ \[ (.+) \s* = \s* (.+) \] $/x;
	if (defined($cmd) && defined($val)) {
		# command
		printf("COMMAND: [%s] = [%s]\n", $cmd, $val) if $debug;
		if ($cmd =~ /engine/i) {
			flushYearStats();
			$engine = $val;
print OUT <<EOF
  <td colspan="8">&nbsp;</td>
  <tr class="engine">
    <td colspan="8" class="engine"><div class="engine">$val</div></td>
  </tr>
EOF
;
		} elsif ($cmd =~ /year/i) {
			flushYearStats();
			$year = $val;
print OUT <<EOF
  <td colspan="8">&nbsp;</td>
  <tr class="year">
    <td colspan="8"><div class="year">$year</div></td>
  </tr>
  <td colspan="8">&nbsp;</td>
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
		my $color = "";
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
			$color = "#BBFFBB";
			$fullSupp = $fullSupp + 1;
			$yearGames++;
		} elsif (($c1 =~ /[-N]/i) && ($c2 =~ /[-N]/i) && ($c3 =~ /[-N]/i) && ($c4 =~ /[-N]/i)) {
			$color = "#FFBBBB";
			$notSupp = $notSupp + 1;
		} else {
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
		if ($color ne "") {
			print OUT "  <tr bgcolor=\"$color\">\n";
		} else {
			print OUT "  <tr>\n";
		}
		if ($url2 ne "") {
			print OUT "    <td><a href=\"$url2\"${newWindow}>$year</a></td>\n";
		} else {
			print OUT "    <td>$year</td>\n";
		}

		if ($url1 ne "") {
			print OUT "    <td><div align=\"left\"><a href=\"$url1\"${newWindow}>$game</a></div></td>\n";
		} else {
			print OUT "    <td><div align=\"left\">$game</div></td>\n";
		}
		printf OUT    "    <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td>\n", yesno($c1), yesno($c2), yesno($c3), yesno($c4), $ver;
		my $compUrl = $compUrls{$company};
		if (defined($compUrl)) {
			print OUT "    <td><div align=\"left\"><a href=\"$compUrl\"${newWindow}>$company</a></div></td>\n"
		} else {
			print OUT "    <td><div align=\"left\">$company</div></td>\n"
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
printf("\nFound %d supported games (%d fully supported) + %d unsupported\n", $partSupp + $fullSupp, $fullSupp, $notSupp);
