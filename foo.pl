#!/usr/bin/env perl

use strict;
use warnings;

my %memory;
my @files;
my $interval = 60;
my $start;
my $end;

for(my $i = 0; $i < $#ARGV; ++$i) {
    my $arg = $ARGV[$i];
    if($arg eq "-s") {
        $start = $ARGV[++$i];
    } elsif($arg eq "-e") {
        $end = $ARGV[++$i];
    } elsif($arg eq "-i") {
        $interval = $ARGV[++$i];
    } else {
        push @files, $arg;
    }
}

foreach my $file (@files) {
    if(defined($start) && $file =~ /([0-9]*).malloc/) {
        if($1 < $start-$interval) {
            print "Skipping: $file: $1 $start\n";
            next
        }
        print "Parsing: $file: $1 $start\n";
    }
    open FILE, "<$file" or die $!;
    while(<FILE>) {
        my $line = $_;
        my $when;
        if($line =~ /([0-9]+) free (0x[0-9a-g]+) \[([^\]]*)\]/) {
            $when = $1;
            delete $memory{$2};
        } elsif($line =~ /([0-9]+) malloc (0x[0-9a-g]+) ([0-9]+) \[([^\]]*)\]/) {
            $when = $1;
        } elsif($line =~ /([0-9]+) realloc (0x[0-9a-g]+) => (0x[0-9a-g]+) ([0-9]+) \[([^\]]*)\]/) {
            $when = $1;
            delete $memory{$2};
            $memory{$3} = { 'length' => $4, 'callstack' => $5 };
        } else {
            print "Unable to grok: $line\n";
        }
        print "Foo: $end -- $when\n";
        last if(defined($end) && $when > $end);
    }
    close FILE;
}

my $count = 0;
foreach my $key (keys %memory) {
    my $m = $memory{$key};
    print "Memory: " . $key . " [" . $m->{length} . "]\n";
    print " * Stack: " . $m->{callstack} . "\n";
    ++$count;
}
print "Count: $count\n";
