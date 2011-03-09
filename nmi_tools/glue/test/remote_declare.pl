#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

use strict;
use warnings;
use Getopt::Long;
use vars qw/ @opt_testclasses $opt_help /;

parseOptions();

######################################################################
# generate list of all tests to run
######################################################################

my $BaseDir = $ENV{BASE_DIR} || die "BASE_DIR not in environment!\n";
my $TaskFile = "$BaseDir/tasklist.nmi";
my $UserdirTaskFile = "$BaseDir/../tasklist.nmi";
my $testdir = "condor_tests";

# Look for a specific file that contains overrides for the default timeouts for some tests
my %CustomTimeouts;
my $TimeoutFile = "$BaseDir/condor_tests/TimeoutChanges";
if( -f $TimeoutFile) {
    print "Found a custom timeout file at '$TimeoutFile'.  Loading it...\n";
    open(TIMEOUTS, '<', $TimeoutFile) || die "Failed to open $TimeoutFile for reading: $!\n";
    while(<TIMEOUTS>) {
        if(/^\s*([\-\w]*)\s+(\d*)\s*$/) {
            print "\tCustom Timeout: $1:$2\n";
            $CustomTimeouts{$1} = $2;
        }
    }
    close(TIMEOUTS);
}
else {
    print "INFO: No custom timeout file found.\n";
}

my %RuncountChanges;
my $RuncountFile = "$BaseDir/condor_tests/RuncountChanges";
# Do we have a file with non-default runtimes for some tests?
if( -f $RuncountFile) {
    print "Found a custom runcount file at '$RuncountFile'.  Loading it...\n";
    open(RUNCOUNT, '<', $RuncountFile) || die "Failed to open $RuncountFile for reading: $!\n";
    while(<RUNCOUNT>) {
        if(/^\s*([\w\-]+)\s+(\d*)\s*$/) {
            print "\tCustom Runcount: $1:$2\n";
            $RuncountChanges{$1} = $2;
        }
    }
    close(RUNCOUNT);
}
else {
    print "INFO: No custom runcount file found.\n";
}

# file which contains the list of tests to run on Windows
my $WinTestList = "$BaseDir/condor_tests/Windows_list";
my $ShortWinTestList = "$BaseDir/condor_tests/Windows_shortlist";

# Figure out what testclasses we should declare based on our
# command-line arguments.  If none are given, we declare the testclass
# "all", which is *all* the tests in the test suite.
my @classlist = @opt_testclasses;
if( ! @classlist ) {
    push( @classlist, "all" );
}

# The rest of argv, after the -- are any additional configure arguments.
my $configure_args = join(' ', @ARGV);

print "****************************************************\n";
print "**** Preparing to declare tests for these classes:\n";
foreach my $class (@classlist) {
    print "****   $class\n";
}

# Make sure we can write to the tasklist file, and have the filehandle
# open for use throughout the rest of the script.
open(TASKFILE, '>', $TaskFile ) || die "Can't open $TaskFile for writing: $!\n"; 
open(USERTASKFILE, '>', $UserdirTaskFile ) || die "Can't open $UserdirTaskFile for writing: $!\n"; 


######################################################################
# For each testclass, generate the list of tests that match it
######################################################################

my %tasklist;

if( !($ENV{NMI_PLATFORM} =~ /winnt/) ) {
    foreach my $class (@classlist) {
    	print "****************************************************\n";
    	print "**** Finding tests for class: '$class'\n";
    	print "****************************************************\n";
    	my $tests = findTests( $class, "top" );
        %tasklist = (%tasklist, %$tests);
    }
}
else {
    # eat the file Windows_list into tasklist hash
    foreach my $class (@classlist) {
    	print "****************************************************\n";
    	print "**** Finding Windows tests '$class'\n";
    	print "****************************************************\n";
        if($class eq "quick") {
            open(WINDOWSTESTS, '<', $WinTestList) || die "Can't open $WinTestList: $!\n";
        }
        elsif($class eq "short") {
            open( WINDOWSTESTS, '<', $ShortWinTestList) || die "Can't open $ShortWinTestList: $!\n";
        }
        else {
            # if things got confused just run the hourly tests
            print "Unknown test class provided for Windows: '$class' (only quick and short are supported)\n";
            print "We will run the short tests in this undefined case.\n";
            open( WINDOWSTESTS, '<', $ShortWinTestList) || die "Can't open $ShortWinTestList: $!\n";
        }

        # Load the tasks, one per line.  Skip comments (lines starting with #)
        %tasklist = map { chomp; $_ => 1} grep !/^\s*\#/, <WINDOWSTESTS>;
        close(WINDOWSTESTS);
    }
}

my $total_tests = scalar(keys %tasklist);
print "-- Found $total_tests test(s) in all directories\n";

print "****************************************************\n";
print "**** Writing out tests to tasklist.nmi\n";
print "****************************************************\n";
foreach my $task (sort keys %tasklist) {
    my $test_count = defined($RuncountChanges{$task}) ? $RuncountChanges{$task} : 1;

    if(defined($CustomTimeouts{"$task"})) {
        print "CustomTimeout:$task $CustomTimeouts{$task}\n";
        foreach(1..$test_count) {
            print TASKFILE "$task-$_ $CustomTimeouts{$task}\n";
            print USERTASKFILE "$task-$_ $CustomTimeouts{$task}\n";
        }
    }
    else {
        foreach(1..$test_count) {
            print TASKFILE "$task-$_\n";
            print USERTASKFILE "$task-$_\n";
        }
    }
}
close( TASKFILE );
close( USERTASKFILE );
print "Wrote " . scalar(keys %tasklist) . " unique tests.\n";
exit(0);

sub findTests {
    my( $classname, $dir_arg ) = @_;
    my ($ext, $dir);

    if( $dir_arg eq "top" ) {
        $ext = "";
        $dir = $testdir;
    }
    else {
        $ext = ".$dir_arg";
        $dir = "$testdir/$dir_arg";
    }
    print "-- Searching directory '$dir' for tests with class '$classname'\n";
    chdir( "$BaseDir/$dir" ) || die "Can't chdir($BaseDir/$dir): $!\n";

    my $list_target = "list_$classname";
    open(LIST, '<', $list_target) || die "cannot open $list_target: $!\n";
    my %tasklist = map { chomp; "$_$ext" => 1 } <LIST>;
    close(LIST);

    print join("\n", keys %tasklist) . "\n";

    my $total = scalar(keys %tasklist);
    print "-- Found $total test(s) in directory '$dir' for class '$classname'\n\n";
    return \%tasklist;
}

sub usage {
    print <<EOF;
--help          This help.
--test-class    Which test class to run, comma separated or multiple occurrence.
EOF
	
    exit 1;
}

# We use -- to delineate the boundary between args to this script and args to
# the configure in this script.
sub parseOptions {
    print "Script called with ARGV: " . join(' ', @ARGV) . "\n";

    my $rc = GetOptions('test-class=s' => \@opt_testclasses,
                        'help'         => \$opt_help,
                        );

    if( !$rc ) {
        usage();
    }

    if(defined($opt_help)) {
        usage();
    }

    # allow comma separated list in addition to multiple occurrences.
    @opt_testclasses = split(/,/, join(',', @opt_testclasses));

    if (!defined(@opt_testclasses)) {
        die "Please supply a test class!\n";
    }
}
