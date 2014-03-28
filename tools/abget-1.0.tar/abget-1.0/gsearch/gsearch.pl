#!/usr/bin/perl

use strict;
use Getopt::Std;
use SOAP::Lite;
use LWP::Simple;
use LWP::UserAgent;

my $ua;
my $key='2V0yCr1QFHJ5D5zYVMES2S5yFJxl7lSN';
my $googleSearch = SOAP::Lite->service("file:GoogleSearch.wsdl");
my $url;
my $request;
my $reqres;
my $i = 0;
my $tmp;
my $counter;
my $position = 0;
my %opts;
my $writefile;
my $readfile;
my $server;
my $size = 100000;
my $maxreturn = 3;


getopt("r:w:s:m:", \%opts);

if($opts{h})
{
	die "Usage: ./gsearch.pl -v [-w writefile] [-r readfile] [-s size] [-m number_of_files] [server]
\t-v: verbose
\t-w: write results to file <writefile>
\t-r: read server(s) from file <readfile>
\t\t file format: <server>\\n
\tif readfile is not provided then server name is needed
\t-s: limiting results to files greater than size bytes (default = $size)
\t-m: maximum number of files to return results (default = $maxreturn)\n";
}

if($opts{v})
{
	print "Arguments:\n";
	print "verbose mode\n";
}

if($opts{w})
{
	$writefile = $opts{w};

	open (WFD, ">$writefile") or die "Can't open $writefile: $!\n";

	if($opts{v})
	{
		print "$writefile\n";
	}
}

if($opts{r})
{
	$readfile = $opts{r};

	if($opts{v})
	{
		print "Reading from file: $readfile\n";
	}
}

if($opts{s})
{
	$size = $opts{s};

	if($opts{v})
	{
		print "Limited to files larger than $size bytes\n";
	}
}

if($opts{m})
{
	$maxreturn = $opts{m};

	if($opts{v})
	{
		print "Maximum returned results = $maxreturn\n";
	}
}

if(@ARGV) 
{
	$server = shift(@ARGV);

	if(index($server, "http://") < 0)
	{
		$server = "http://$server";
	}
	
	if($opts{v})
	{
		print "Server: $server\n";
	}
}
else
{
	if(!$readfile)
	{
		die "Usage: ./gsearch.pl -v [-w writefile] [-r readfile] [-s size] [server]
\t-v: verbose
\t-w: write results to file <writefile>
\t-r: read server(s) from file <readfile>
\t\t file format: <server>\\n
\tif readfile is not provided then server name is needed\n
\t-s: limiting results to files greater than size bytes (default = 100KB)\n";
	}
}

if($server)
{
	if($writefile)
	{
		print WFD "Querying server: $server\n";
	}
	else
	{
		print "Querying server: $server\n";
	}

	foreach my $filetype ("pdf", "ps", "doc", "xsl", "ppt", "rtf")
	{
		my $query="site:$server AND filetype:$filetype";
		
		if($opts{v})
		{
			print "query: $query\n";
			
			if($writefile)
			{
				print WFD "query: $query\n";
			}
		}
		
		my $result = $googleSearch->doGoogleSearch($key, $query, 0, 10, "false", "", "false", "", "", "");
		
		if($opts{v})
		{
			print "About $result->{'estimatedTotalResultsCount'} results.\n";
		}
		
		for($position = 1; $position < $result->{'estimatedTotalResultsCount'};)
		{
			foreach my $res (@{$result->{resultElements}})
			{
				$position++;
				$url = $res->{URL};
				
				if($opts{v})
				{
					print "Checking url: $url\n";
				}
				
				if($request = head($url))
				{
					$tmp = $request->content_length;
					
					if($opts{v})
					{
						print "OK document exists\n"; 
						print "size = $tmp\n";
					}
					
					if($tmp >= $size)
					{
						$counter++;

						if($writefile)
						{
							print WFD "\t$url\n";
						}
						else
						{
							print "\t$url\n";
						}
					}
					
					if($counter eq $maxreturn)
					{
						last;
					}
				}
				else
				{
					if($opts{v})
					{
						print "Document does not exists\n";
					}
				}
			}
			
			if($counter eq $maxreturn)
			{
				last;
			}
			
			$result = $googleSearch->doGoogleSearch($key, $query, $position, 10, "false", "", "false", "","","");
		}
				
		if($counter eq $maxreturn)
		{	
			last;
		}
	}

	if($counter eq 0)
	{
		if($writefile)
		{
			print WFD "\tNo results found\n";
		}
	}
}
else
{
	if($opts{v})
	{
		print "Reading from file: $readfile\n";
	}

	open (RFD, $readfile) or die "Can't open $readfile: $!\n";
	
	while(<RFD>)
	{
		chop($_);
		$server = $_;
		
		$counter = 0;

		if($writefile)
		{
			print WFD "Querying server: $server\n";
		}
		else
		{
			print "Querying server: $server\n";
		}
		
		foreach my $filetype ("pdf", "ps", "doc", "xsl")
		{
			my $query="site:$server AND filetype:$filetype";
			
			if($opts{v})
			{
				print "query: $query\n";
				
				if($writefile)
				{
					print WFD "query: $query\n";
				}
			}
			
			my $result = $googleSearch->doGoogleSearch($key, $query, 0, 10, "false", "", "false", "", "", "");
			
			if($opts{v})
			{
				print "About $result->{'estimatedTotalResultsCount'} results.\n";
			}
			
			for($position = 1; $position < $result->{'estimatedTotalResultsCount'};)
			{
				foreach my $res (@{$result->{resultElements}})
				{
					$position++;
					$url = $res->{URL};
					
					if($opts{v})
					{
						print "Checking url: $url\n";
					}
					
					if($request = head($url))
					{
						$tmp = $request->content_length;
						
						if($opts{v})
						{
							print "OK document exists\n"; 
							print "size = $tmp\n";
						}
						
						if($tmp >= $size)
						{
							$counter++;
							
							if($writefile)
							{
								print WFD "\t$url\n";
							}
							else
							{
								print "\t$url\n";
							}
						}
						
						if($counter eq $maxreturn)
						{
							last;
						}
					}
					else
					{
						if($opts{v})
						{
							print "Document does not exists\n";
						}
					}
				}
				
				if($counter eq $maxreturn)
				{
					last;
				}
				
				$result = $googleSearch->doGoogleSearch($key, $query, $position, 10, "false", "", "false", "","","");
			}
			
			if($counter eq $maxreturn)
			{	
				last;
			}
		}
		
		if($counter eq 0)
		{
			if($writefile)
			{
				print WFD "\tNo results found\n";
			}
		}
	}
	
	close RFD;
}

if($writefile)
{
	close WFD;
}
