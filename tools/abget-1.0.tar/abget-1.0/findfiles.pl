#!/usr/bin/perl -w
#
#use strict;
use Getopt::Std;
use LWP::UserAgent;
use LWP::Simple;
use HTML::LinkExtor;
use URI::URL;

my %vlinks;
my %vrefs;
my $request;
my $url;
my $fileurl;
my $server;
my %opts;
my $size = 100000;
my $maxreturn = 3;
my @files = ();
my @ahref = ();
my $largefiles = ();
my $base;
my $us;
my $p;
my $readfile;
my $verbose = 0;
my $counter = 0;

getopt("r:w:s:m:", \%opts);

sub usage {
	die "Usage ./findfiles.pl [-v] [-s size] [-m number_of_files] [server]
	\t-v: verbose
	\t-s: limiting results to files greater than size bytes (default = $size)
	\t-m: maximum number of files to return results (default = $maxreturn)\n";
}

if($opts{h}) {
	usage();
}

if($opts{v}) {
	$verbose = 1;
	print "Arguments:\n";
	print "verbose mode\n";
}

if($opts{s}) {
	$size = $opts{s};

	if($verbose) {
		print "Limited to files larger than $size bytes\n";
	}
}

if($opts{m}) {
	$maxreturn = $opts{m};

	if($verbose) {
		print "Maximum return resuls = $maxreturn\n";
	}
}

if(@ARGV) {
	$server = shift(@ARGV);

	if(index($server, "http://") < 0) {
		$server = "http://$server";
	}

	if($verbose) {
		print "Server: $server\n";
	}
}
else {
	if(!$readfile) {
		usage();
	}
}


# Set up a callback that collect image links
sub callback {
	my($tag, %attr) = @_;
#	return if $tag ne 'img' && $tag ne 'link';  # we only look closer at <img ...>
	if($tag eq 'a') {
			foreach $att (%attr) {
					if($att =~ /pdf|ps|doc|rtf|ppt|xls|png|gif|jpg|jpeg/) {
							push(@files, values %attr);
							return;
					}
			}
			push(@ahref, values %attr);
	}
	else {
		push(@files, values %attr);
	}
}

sub check_found_files() {
#	while($url = pop(@files)) {
	foreach $fileurl (@files) {
		if(($fileurl =~ $base)) {
			if(!($fileurl =~ "#")) {
				if(!$vlinks{$fileurl}) {
					if($verbose) {
						print "Querying: $fileurl\n";
					}
					
					$vlinks{$fileurl} = 1;
					
					if($request = head($fileurl)) {
						if($request->content_length && $request->content_length >= $size) {
							$counter++;
							
							print "found \t$fileurl\t", $request->content_length, " $counter \n";
							
							push(@largefiles, $fileurl, $request->content_length);
						}
						
						if($counter eq $maxreturn) {
							return;
						}
					}
					else {
						if($verbose) {
							print "Document does not exists\n";
						}
					}
				}
			}
		}
	}
}

$ua = LWP::UserAgent->new;

# Make the parser.  Unfortunately, we don't know the base yet
# (it might be diffent from $url)
$p = HTML::LinkExtor->new(\&callback);

# Request document and parse it as it arrives
$res = $ua->request(HTTP::Request->new(GET => $server), sub {$p->parse($_[0])});

# Expand all image URLs to absolute ones
$base = $res->base;
@files = map { $_ = url($_, $base)->abs; } @files;
@ahref = map { $_ = url($_, $base)->abs; } @ahref;

check_found_files();

if($counter >= $maxreturn) {
	goto END;
}

#print join("\n ", @ahref), "\n";

#while($url = pop(@ahref)) {
foreach $url (@ahref) {
	if(($url =~ $base)) {
		if(!($url =~ "#")) {
			if(!$vrefs{$url}) {
				$res = $ua->request(HTTP::Request->new(GET => $url), sub {$p->parse($_[0])});
				
				@files = map { $_ = url($_, $base)->abs; } @files;
				@ahref = map { $_ = url($_, $base)->abs; } @ahref;
				
				$vrefs{$url} = 1;
				
				check_found_files();
				
				if($counter >= $maxreturn) {
					goto END;
				}
			}
		}
	}
}

END:

if($verbose) {
	print "Found $counter files\n";
}

# Print them out
print join("\n", @largefiles), "\n";

