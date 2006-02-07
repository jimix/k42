# ############################################################################
# K42: (C) Copyright IBM Corp. 2005.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: generate_next_version.pl,v 1.1 2005/09/01 21:29:32 neamtiu Exp $
# ############################################################################
#!/usr/bin/perl -0777

# Given a .H file with a class Foo containing a Factory definition, generate next version
# of Foo (versioned class named and incremented Factory version)
#
# An example: generate next version of Process Shared:
# ./generate_next_version.pl kitchsrc/os/kernel/proc/ProcessShared.H >ProcessShared_v1.H

# And next version of the newly generated, and so on 
# ./generate_next_version.pl ProcessShared_v1.H >ProcessShared_v2.H


####### pragmas ######
use strict;
######################

sub myDie {
    my $cause = shift;

    printf STDERR $cause."\n";

    exit 1;
}

sub extractClassNameAndFactoryVersion {
    
    my $filename = shift;

    # If we only find a DEFINE_FACTORY_BEGIN(Factory), then version is 0
    # If instead we find a DEFINE_FACTORY_BEGIN_VERSION(Factory,X)
    # then the version is X . Duh.
    my $patvc = 'class.*class\s+([a-zA-Z0-9_]+)\s+:(.*?)\{(.*?)DEFINE_FACTORY_BEGIN_VERSION\(\w*\w*,\w*(\d+)\)';
    my $patv = 'class\s+([a-zA-Z0-9_]+)\s+:(.*?)\{(.*?)DEFINE_FACTORY_BEGIN_VERSION\(\w*\w*,\w*(\d+)\)';
    my $patc = 'class.*class\s+([a-zA-Z0-9_]+)\s+:(.*?)\{(.*?)DEFINE_FACTORY_BEGIN';
    my $pat = 'class\s+([a-zA-Z0-9_]+)\s+:(.*?)\{(.*?)DEFINE_FACTORY_BEGIN';
    open IN, "< $filename" or myDie "can't open $filename for reading";

    my $className; my $vers = -1;

    while(<IN>)
    {
        if (/$patvc/s) {
            #print STDERR "PATVC\n";
            $className = $1;
            $vers = $4;
        } elsif (/$patv/s) {
            #print STDERR "PATV\n";
            $className = $1;
            $vers = $4;
        } elsif (/$patc/s) {
            #print STDERR "PATC\n";
            $className = $1;
            $vers = 0;
        } elsif (/$pat/s) {
            #print STDERR "PAT\n";
            $className = $1;
            $vers = 0;
        }         
        
        if ($vers != -1)
        {
            #print STDERR "<$className><$vers>\n";
            return ($className, $vers);
        }
    }
    myDie "Couldn't find a DEFINE_FACTORY_BEGIN[_VERSION] pattern."
}

sub usage {

    print STDERR "generate_next_version.pl <input.H>\n";
    exit 1;
}

sub makeNewClassName {
    my $oldClassName = shift;
    my $version = shift;

    if ($version == 1) {
        return $oldClassName."_v_".$version;
    } else {
        my ($stem,$rest) = split(/_v_/, $oldClassName);
        return $stem."_v_".$version;
    }

}

#################### main ##################

$#ARGV >=0 or usage(); 

my $inputFile = $ARGV[0];
my $oldClassName;my $version;my $newClassName; 
my $outClassDef; my $outCreate; my $outCreateReplacement; my $outDestroy;

($oldClassName, $version) = extractClassNameAndFactoryVersion($inputFile);
$version++;
$newClassName = makeNewClassName($oldClassName, $version);

$outClassDef = sprintf ("#include <cobj/Factory.H>
#include <cobj/CObjRootSingleRep.H>
#include \"%s.H\"

class %s : public %s {
public:
    DEFINE_FACTORY_BEGIN_VERSION(Factory,%d)
	virtual %sRef create();
	virtual SysStatus destroy(%sRef ref);
	virtual SysStatus createReplacement(CORef ref, CObjRoot *&root);
    DEFINE_FACTORY_END(Factory)

protected:
    DEFINE_GLOBALPADDED_NEW(%s);

};
", $oldClassName ,
$newClassName, $oldClassName,
$version,
$oldClassName,
$oldClassName,
$newClassName);

$outCreate = sprintf ("
%sRef %s::Factory::create(void)
{
    %sRef ref;
    %s *obj = new %s();
    tassert(obj, ;);
    obj->factoryRef = (%s::FactoryRef)getRef();
    new %s::Root((CObjRep *)obj);
    ref = (%sRef)obj->getRef();
    registerInstance((CORef)ref);
    return ref;
}",
$oldClassName, $newClassName,
$oldClassName,
$newClassName, $newClassName,
$oldClassName,
$oldClassName,
$oldClassName);

$outCreateReplacement = sprintf ("
SysStatus %s::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    %s *obj = new %s();
    tassert(obj, ;);
    obj->factoryRef = (%s::FactoryRef)getRef();
    root = (CObjRoot *) new %s::Root((CObjRep *)obj, (RepRef)ref, CObjRoot::skipInstall);
    registerInstance((CORef)ref);
    return 0;
}",
$newClassName,
$newClassName,$newClassName,
$oldClassName,
$oldClassName);

$outDestroy = sprintf ("
SysStatus %s::Factory::destroy(%sRef ref)
{
    SysStatus rc;
    rc = deregisterInstance((CORef)ref);
    tassert(_SUCCESS(rc), ;);
    DREF(ref)->destroy();
    return 0;
}",
$newClassName, $oldClassName);

print $outClassDef;
print $outCreate;
print $outCreateReplacement;
print $outDestroy;
