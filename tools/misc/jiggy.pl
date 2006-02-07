#!/usr/bin/env perl
#!perl
# K42: (C) Copyright IBM Corp. 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#
# Prerequisites:
#
# 1. Collect a trace file with the mask bits 0x31910 set.
# 2. Use traceTool to obtain an ASCII version  of the trace file.
#    For multi-processor traces, use the --mpSync option. 
#    You must also use the "--strip" option.
#
# Configuration options
#
# -c  CPU count
# -r  File to read
# -P  Path to root of build tree (/path/to/powerpc/noDeb)
#
# Use -r or -c  (-c x --> x== number of cpus).  If -c is used, the script
# expects the trace files to have the canonical names, in the cwd.
# Use -r to specify a single trace file.
# 
# Use -F to specify the prefix of the canonical names, normally trace-out
#

# Reporting options
#
# -s  Collapse syscall information into "syscall" category
# -t  Print process tree
# -e  Print event info  (this is huge)

#
# Diagnostic options
#
# -u  Print undeleted events at end of run
# -b  Print billing info
# -R  Dump file to read (contains perl-encoding of trace results)
# -D  Create dump file

use strict;
use Getopt::Std;
use Data::Dumper;
use FileHandle;
require 'open3.pl';
my %opts=();
my $startPrint = 0;

getopts('P:F:tdsuc:ebr:R:D:',\%opts);

my %procs = ();
my @events;
my %commIDThread = ();
my @deadThreads = ();
my %waits = ();
my @unmatchedCalls = ();

my @faultTypes =();
$faultTypes[0]={ 'name' => "ZeroFill", 'short' => "Z" };
$faultTypes[1]={ 'name' => "CopyFrame", 'short' => "Copy" };
$faultTypes[2]={ 'name' => "UnMapped", 'short' => "UM" };
$faultTypes[3]={ 'name' => "FCMComp", 'short' => "Comp" };
$faultTypes[4]={ 'name' => "FCMDefault", 'short' => "Def" };
#$faultTypes[3]={ 'name' => "CacheSynced", 'short' => "CSync" };
#$faultTypes[4]={ 'name' => "MakeRO", 'short' => "RO" };
$faultTypes[5]={ 'name' => "WriteFault", 'short' => "WF" };
$faultTypes[6]={ 'name' => "FCMFixed", 'short' => "Fixed" };
$faultTypes[7]={ 'name' => "FCMLTrans", 'short' => "LT" };
#$faultTypes[6]={ 'name' => "DoFill", 'short' => "Fill" };
#$faultTypes[7]={ 'name' => "FromFreeList", 'short' => "FFL" };
$faultTypes[8]={ 'name' => "KeepFrame", 'short' => "Keep" };
$faultTypes[9]={ 'name' => "FCMPrimitive", 'short' => "Prim" };
#$faultTypes[9]={ 'name' => "UnmapOther", 'short' => "UOther" };
$faultTypes[10]={ 'name' => "ProcRep", 'short' => "PRep" };
#$faultTypes[10]={ 'name' => "NeedWrite", 'short' => "NW" };
$faultTypes[11]={ 'name' => "DistribFCM", 'short' => "DFCM" };
$faultTypes[12]={ 'name' => "DistribRoot", 'short' => "DRt" };
$faultTypes[13]={ 'name' => "CreateSeg", 'short' => 'CRSeg' };
$faultTypes[14]={ 'name' => "MapShrSeg", 'short' => 'MSSeg' };
$faultTypes[15]={ 'name' => "WriteFaul", 'short' => 'WF' };


my $event;
my $prev;
my $cpu;
my $cpuCount;
use Memoize;


my $procMap = {'0' => 'os/boot_image.dbg',
	       '1' => 'os/servers/baseServers/baseServers.dbg',
	       '2' => 'os/servers/baseServers/baseServers.dbg',
	       '3' => 'os/servers/baseServers/baseServers.dbg',
	       '7' => 'os/servers/baseServers/baseServers.dbg',
	       '8' => 'os/servers/baseServers/baseServers.dbg',
	       'x' => 'lib/exec.so.dbg'
	      };


my $buildDir = "/home/mostrows/elf.k42/powerpc/partDeb/";
if($opts{P}){
  $buildDir = $opts{P};
  die "Build tree path not correct: $buildDir\n"
    if(!stat($buildDir . $procMap->{'0'}));
}
my @disp;
my $functionModes = {'DispatcherDefault::AsyncMsgHandler'	=> 'async',
		     'TimerEventLinux::Event'			=> 'timer',
		     'ProgExec::ForkWorker'				=> 'SCfork',
		     'ProgExec::ForkChildPhase2'			=> 'SCchild',
		     'linuxCheckSoftIntrs'			=> 'iointr',
		     'COSMgrObject::CleanupDaemon'		=> 'garbage',
		     'DHashTable<LocalPageDescData, AllocPinnedLocalStrict>' .
				'::resizeEntry'			=> 'mmresize',
		     'DHashTable<LocalPageDescData, AllocLocalStrict>' .
				'::resizeEntry'			=> 'mmresize',
		     'DHashTable<MasterPageDescData, AllocGlobalPadded>' .
				'::resizeEntry'			=> 'mmresize',
		     'PMRoot::MyRoot::HandleTimer'		=> 'mmtimer',
		     'MPMsgMgrEnabled::ProcessMsgList'		=> 'mpmsg'
		     };


my @curCDA;

my $symFiles;

sub getFunctionName($$){
  my $file = shift;
  my $ptr = shift;

  if(!$symFiles->{$file}){
    my $input;
    my $output;
    my $err;
    open3($symFiles->{$file}->{input},
	  $symFiles->{$file}->{output}, $err,
	  "powerpc64-linux-addr2line -e $file -C -f");
  }
  print {$symFiles->{$file}->{input}} "$ptr\n";
  my $io = $symFiles->{$file}->{output};
  my $function = <$io>;
  <$io>;
  if($function=~/(.*)\(.*\)/){
    return $1;
  }
  $function=~/(\S+)/;
  return $1;
}
memoize('getFunctionName');

sub getPtrName($$){
  my $pid = shift;
  my $addr = shift;
  my $file;
  if($procMap->{$pid}){
    $file = $procMap->{$pid};
  }else{
    $file = $procMap->{x};
  }
  return getFunctionName($buildDir . $file, $addr);
}
sub getThreadMode($$){
  my $cid = shift;
  my $addr = shift;
  my $file;
  if($procMap->{pidOnly($cid)}){
    $file = $procMap->{pidOnly($cid)};
  }else{
    $file = $procMap->{x};
  }
  my $fnName = getFunctionName($buildDir . $file, $addr);
  my $fn = $functionModes->{$fnName};
  if(!$fn){
    print("Unknown function: \"$fnName\" " .
	  "$buildDir$file $addr $cid\n");
    $fn = "unknown";
  }
  return $fn;
}

sub deep_copy {
  my $this = shift;
  if (not ref $this) {
    $this;
  } elsif (ref $this eq "ARRAY") {
    [map deep_copy($_), @$this];
  } elsif (ref $this eq "HASH") {
    +{map { $_ => deep_copy($this->{$_}) } keys %$this};
  } else { die "what type is $_?" }
}

sub splitCommID($){
  my $inpid = shift;
  my $l = length $inpid;
  my $pid = 0;
  my $vp = 0;
  my $rd = 0;
  if($l > 8){
    $pid = hex(substr($inpid,-16,-8));
  }
  if($l > 4){
    $rd = hex(substr($inpid,-8,4));
  }
  $vp = hex(substr($inpid,-4,4));
  return ($pid,$vp,$rd);
}
#memoize('splitCommID');

sub pinfo($){
  my $inpid = shift;
  my ($pid, $vp, $rd) = splitCommID($inpid);
  return sprintf("%3.3x'%x'%x",$pid,$rd,$vp);
}

sub einfo($){
  my $event = shift;
  sprintf("[%05d ". pinfo($event->{nextCID}). "]", $event->{num});
}

sub pidOnly($){
  my $commID = shift;
  return sprintf("%x", (splitCommID($commID))[0]);
}

sub kernelCID($){
  my $event = shift;
  return $event->{cpu};
}

sub proc($):lvalue{
  my $pid = shift;
  if(!defined $pid) { 
    print "*************** Bad pid ***************\n"; 
  }
  return $procs{$pid};
}

sub incCnt($){
  my $event = shift;
  ++$event->{done};
#  print einfo($event) . " inc count: $event->{done}\n";
}

sub decCnt($){
  my $event = shift;
  --$event->{done};
#  print einfo($event) . " dec count: $event->{done}\n";
  if($event->{done} == 0){
    delete $events[$event->{cpu}][$event->{idx}]->{owner};
    undef $events[$event->{cpu}][$event->{idx}];
    delete $events[$event->{cpu}][$event->{idx}];
#    delete $events{$event->{cpu}}{$event->{idx}}->{owner};
#    undef $events{$event->{cpu}}{$event->{idx}};
#    delete $events{$event->{cpu}}{$event->{idx}};
  }
}

sub beginPreempt($){
  my $info = shift; #Info contains "begin" (event), "type" (description)
  incCnt($info->{begin});
  proc($info->{pid})->{external}->{$info->{begin}->{idx}} = $info;
}

sub finishPreempt($){
  my $info = shift;
  my $idx = $info->{begin}->{idx};
  my $proc = proc($info->{pid});
  delete $proc->{external}->{$idx};

  if($info->{begin}){
    decCnt($info->{begin});
    delete $info->{begin};
  }
  if($info->{end}){
    delete $info->{end};
  }
}

sub getPreempt($$){
  my $pid = pidOnly(shift);
  my $key = shift;
  return proc($pid)->{external}->{$key};
}

sub previous($){
  my $event = shift;
  return $event->{prev};
}

sub startThreadLong($$$$){
  my $event = shift;
  my $pid = shift;
  my $ptr = shift;
  my $fn = shift;
  my $id = $event->{idx};
  my $proc = proc($pid);
  if(defined $proc->{threads}->{$ptr}){
    print einfo($event) . "****** Thread is already in use: $ptr  proc($pid)->{threads}->{$ptr}->{id}\n";
    return;
  }
  delete $proc->{threads}->{ptr}->{delete}; 
  delete $proc->{threads}->{ptr}->{owner};
  $proc->{threads}->{$ptr}->{id} = $id;
  $proc->{threads}->{$ptr}->{fct} = $fn;
  $proc->{threads}->{$ptr}->{key} = $ptr;
  $proc->{threadID}->{$id} = $proc->{threads}->{$ptr};
  return $id;
}

sub startThread($$){
  my $event = shift;
  my $function = shift;
  return startThreadLong($event, pidOnly($event->{nextCID}), 
			 $event->{thrptr}, $function);
}


sub cleanupThreads(){
  my $thr;
  while($thr = pop @deadThreads){
    $thr->{proc}->{thrSummary}->{$thr->{fct}}->{count}++;
    $thr->{proc}->{thrSummary}->{$thr->{fct}}->{time} += $thr->{time};
    $thr->{proc}->{thrSummary}->{$thr->{fct}}->{count}++;
    $thr->{proc}->{thrSummary}->{$thr->{fct}}->{faults} += $thr->{faults};
    $thr->{proc}->{thrSummary}->{$thr->{fct}}->{faultTime}+= $thr->{faultTime};
    delete $thr->{owner};
    delete $thr->{proc}->{threadID}->{$thr->{id}};
  }
}

sub deleteThread($){
  my $event = shift;
  my $pid = pidOnly($event->{nextCID});
  my $proc = proc($pid);
  my $thr = $proc->{thread}->{$event->{thrptr}};
#  print "###1 Delete thread $thr->{id} $event->{thrptr}\n";
  if($commIDThread{$event->{nextCID}} == $event->{thr}){
    delete $commIDThread{$event->{nextCID}};
  }
  $thr->{proc} = $proc;
  $thr->{delete} = 1;
  push @deadThreads, $thr;
}

sub deleteThreadPtr($){
  my $event = shift;
  my $pid = pidOnly($event->{nextCID});
  my $proc = proc($pid);
  my $thr = $proc->{threads}->{$event->{thrptr}};
#  print "###2 Delete thread $thr->{id} $event->{thrptr} $event->{id}\n";
  delete $proc->{threads}->{$event->{thrptr}};
  if($proc->{threadID}->{$thr->{id}} == $thr){
    $thr->{proc} = $proc;
    $thr->{delete} = 1;
    push @deadThreads, $thr;
  }
}

sub deleteThreadID($){
  my $event = shift;
  my $pid = pidOnly($event->{nextCID});
  my $proc = proc($pid);
  my $thr = $proc->{threadID}->{$event->{thr}};
  if(!$thr){ return;}
#  print "###3 Delete thread $thr->{id} $thr->{key}\n";
  if($commIDThread{$event->{nextCID}} == $event->{thr}){
    delete $commIDThread{$event->{nextCID}};
  }
  $thr->{proc} = $proc;
  $thr->{delete} = 1;
  push @deadThreads, $thr;
}


sub getThread($){
  my $event = shift;
  return proc(pidOnly($event->{nextCID}))->{threadID}->{$event->{thr}};
}

sub getThreadID($$){
  my ($pid, $id) = @_;
  return proc($pid)->{threadID}->{$id};
}

sub getThreadPid($$){
  my $pid = shift;
  my $event = shift;
  return proc($pid)->{threadID}->{$event->{thr}};
}
sub getThreadPtr($){
  my $event = shift;
  return proc(pidOnly($event->{nextCID}))->{threads}->{$event->{thrptr}};
}

sub billSlot(){
  my $slotTime =1000000 * $event->{time} - 1000000 * $prev->{time};
  my $proc = proc(pidOnly($prev->{nextCID}));
  my $cpu = $prev->{cpu};
  $proc->{modes}->{$prev->{mode}}[$cpu]->{slots}++;
  $proc->{modes}->{$prev->{mode}}[$cpu]->{time} += $slotTime;
  my $thr = getThread($prev);


  if($thr){
    if(!$thr->{delete}){
      $commIDThread{$prev->{nextCID}} = $prev->{thr};
    }
    if(!$thr->{lastMode} || $thr->{lastMode} ne $prev->{mode}){
      $proc->{modes}->{$prev->{mode}}[$cpu]->{count}++;
    }
    $thr->{lastMode} = $prev->{mode};
    $thr->{mode} = $thr->{lastMode};
    $thr->{lastIdx} = $prev->{idx};
    $thr->{time} += $slotTime;
  }else{
    if($proc->{$prev->{nextCID}}->{lastMode} ne $prev->{mode}){
      $proc->{modes}->{$prev->{mode}}[$cpu]->{count}++;
    }
  }
  $proc->{$prev->{nextCID}}->{lastMode} = $prev->{mode};

  my $str = einfo($prev);
  my $x = $prev->{owner};
  while($x){
    my $proc = proc($x->{pid});
    if($x->{pid} eq "234"){
      print "Billed 1 234\n";
    }
    my $a;
    my $b;
    if($x->{type} eq "fault" || $x->{type} eq "double"){
      $a = "faults";
      $b = "faultTime";
    }elsif($x->{type} eq "ppc"){
      $a = "ppcs";
      $b = "ppcTime";
    }
    if($a){
      $proc->{modes}->{$x->{prevMode}}[$cpu]->{$b}+=$slotTime;
    }
    $proc->{extmodes}->{$x->{type}}[$cpu]->{time}+= $slotTime;
    $proc->{extmodes}->{$x->{type}}[$cpu]->{slots}++;
    if($x->{begin} == $prev) {
      if($a){
	$proc->{modes}->{$x->{prevMode}}[$cpu]->{$a}++;
      }
      $proc->{extmodes}->{$x->{type}}[$cpu]->{count}++;
    }
    $str .= " ($x->{pid},$x->{type},$x->{begin}->{idx})";
    $x = $x->{previous};
  }
  if($opts{b} && $prev->{idx} >= $startPrint){
    print "$str\n";
  }


  $proc->{lastThr} = $prev->{thr};
}

sub newPPCThread(){
  my $matchPPC;
  for(my $idx = $#unmatchedCalls; $idx>=0; --$idx){
    my $call = $unmatchedCalls[$idx];
    if ($event->{nextCID} eq $call->{nextCID} &&
	$event->{cpu} eq $call->{cpu}) {
      splice @unmatchedCalls, $idx, 1;
      #Thread is new, any old existence should be deleted
      #The thread pointer specified by $event has to be freed
      deleteThreadPtr($event);
      $event->{thr} = startThread($event, $event->{fct});

      $event->{printInfo} = "thread start for ppc " . einfo($call) . 
	" $call->{mode} $event->{thr} $event->{fct} $event->{fct} " . 
	  ($event->{time} - $call->{time})*1000000;

      $event->{enterMode} = 1;
      $event->{mode} = "ppcsrv";
      my $ppc = $call->{owner};
      getThread($event)->{owner} = $ppc;
      return;
    }
  }
}


sub threadSwitch(){
  my $thr = getThreadPtr($event);
  if($thr){
    $event->{thr} = $thr->{id};
  }else{
    $event->{enterMode} = 1;
    $event->{thr} = startThread($event, 'unknown');
  }

  if(defined $prev && getThread($prev)){
    #      print "Preempt thread " . $prev->{thr} . 
    #	" " . $prev->{mode} . "\n";
    getThread($prev)->{mode}=$prev->{mode};
  }

  my $thr = getThread($event);
  if($thr && $thr->{mode}){
    $event->{mode} = $thr->{mode};
  }elsif($thr && $thr->{lastMode}){
    $event->{mode} = $thr->{lastMode};
  }else{
    $event->{enterMode} = 1;
    $event->{mode} = "unknown";
  }

  $event->{printInfo} = "id: $event->{thr} ptr: $event->{thrptr}";
}

sub endPPC(){
  if(!$prev){
    $event->{thr} = 'u';
    $event->{mode} = 'unknown';
    $event->{enterMode} = 1;
    return;
  }

  if(defined $prev->{owner}){
    my $ppc = $prev->{owner};
    $ppc->{end} = $event;

    # Disassociate the thread from the ppc record
    
    delete getThread($prev)->{owner};
    decCnt($prev->{owner}->{begin});

    # The thread is over only if this is a RETURN, (not a PROC_KILL)
    if($event->{type} eq "TRACE_EXCEPTION_PPC_RETURN"){
      $event->{mode} = $ppc->{prevMode};
      $event->{thr} = $ppc->{prevThr};
      $event->{owner} = $ppc->{previous};
      deleteThreadID($prev);
      $event->{ppcTime} = getThread($prev)->{time} + 
	1000000 * ($event->{time}-$prev->{time});
      # For accounting purposes remember the mode that made the ppc call
      # $event->{mode} may change due to scheduling events
      $event->{PPCCallMode} = $ppc->{prevMode};
    }
    $event->{printInfo} = "completes " . einfo($ppc->{begin}) . 
      " time: " . sprintf("%0.2f", $event->{ppcTime});
      #"mode " 
      #. $ppc->{begin}->{callerMode} . " " . $ppc->{begin}->{callerThr};
  }else{
    $event->{thr} = "u";
    $event->{mode} ="unknown";
  }
}

# Keep track of begin, end events (though since these are in-progress, the
# end events are not defined.
# Eventually, upon completion we'll store the PF data in the process 
#
sub dispatchWait(){
  my $wait = ();
  my $cid = $prev->{nextCID};
  my $cpu = $event->{cpu};
  $event->{currentPID}=0;

  $wait->{pid} = pidOnly($cid);
  my $proc = proc($wait->{pid});

  $wait->{type} = "scheduling";
  $event->{printInfo} = $event->{printInfo} . " $prev->{mode} $cid";
  if($prev->{nextCID} ne $event->{currentPID} &&
     ($prev->{type} eq 'TRACE_EXCEPTION_DISPATCH' ||
      $prev->{type} eq 'TRACE_EXCEPTION_TIMER_INTERRUPT' || 
      $prev->{type} eq 'TRACE_EXCEPTION_EXCEPTION_MPMSG' ||
      $prev->{type} eq 'TRACE_EXCEPTION_PGFLT_DONE' ||
      $prev->{type} eq 'TRACE_EXCEPTION_PPC_RETURN') &&
     $prev->{mode} ne 'scheduler'){

    $wait->{begin}= $prev;
    $wait->{beginWait}= $prev->{idx};

    $wait->{prevMode} = $prev->{mode};
    $wait->{prevThr} = $prev->{thr};
    $wait->{prevCID} = $prev->{nextCID};
    $prev->{nextCID} = $event->{currentPID};

    $prev->{thr} = 'K';
    $prev->{mode} = 'scheduler';
    delete $prev->{owner};
  }else{
    $wait->{begin} = $event;
    $wait->{prevMode} = $prev->{mode};
    $wait->{prevThr} = $prev->{thr};
    $wait->{prevCID} = $prev->{nextCID};
  }

  if($proc->{$cid}->{preemption}){
    $event->{printInfo} = $event->{printInfo} .
      " finish $wait->{begin}->{idx}/" . $wait->{pid};
    $proc->{$cid}->{preemption}->{end} = $wait->{begin};
    finishPreempt($proc->{$cid}->{preemption});
  }

  $event->{nextCID} = kernelCID($event);
  $event->{mode} = "scheduler";
  $event->{thr} = 'K';
  delete $event->{owner};
  beginPreempt($wait);

  $event->{printInfo} = $event->{printInfo} .
    "begin $wait->{begin}->{idx}/$wait->{pid} $wait->{begin}->{mode}";
  $proc->{$cid}->{preemption} = $wait;
  $proc->{$cid}->{waiting} = $wait;
}

sub dispatchDone(){
  my $proc = proc(pidOnly($curCDA[$event->{cpu}]));
  my $cid = $curCDA[$event->{cpu}];
  my $info = $proc->{$cid}->{preemption};

  if($info && $info->{type} eq "scheduling"){
    $info->{end} = $event;
    $event->{thr} = $info->{prevThr};
    $event->{mode}= $info->{prevMode};
    $event->{nextCID} =$info->{prevCID};
    $event->{printInfo} = $event->{printInfo} .
      "finish $info->{begin}->{idx} $info->{beginWait} ". pidOnly($cid);
    finishPreempt($info);
    delete $proc->{$cid}->{preemption};
    delete $proc->{$cid}->{waiting};
  }
  $event->{currentPID} = 0;
}


sub handleProcSwitch(){
  my $cid = $event->{switchtoCID};

  $event->{thr} = "K";
  $event->{mode} = "scheduler";
  $event->{nextCID} = kernelCID($event);

  if(!defined $procs{pidOnly($cid)}){
    $procs{pidOnly($cid)}->{pid} = pidOnly($cid);
    $procs{pidOnly($cid)}->{created} = $event->{idx};
    push @{$procs{pidOnly($cid)}->{progs}}, "unknown";
  }

  if($prev->{nextCID} ne kernelCID($event)){
    $event->{currentPID} = "0";
    pidMismatch_DISPATCH($event);
  }
  my $newProc = proc(pidOnly($cid));
  my $newCID = $newProc->{$cid};

  $event->{printInfo} = "switch $curCDA[$event->{cpu}] -> $cid ";

  if(!$newCID->{preemption}){
    # Some other CID may be the last one to have been preempted
    my $regex = pidOnly($cid) . '000(\S)000(\S)';
  SEARCH:foreach my $key (keys %{$newProc}){
      if($key=~/$regex/){
	if($newProc->{$key}->{preemption} && 
	   $newProc->{$key}->{preemption}->{type} ne "scheduling"){
	  $newCID->{preemption} = $newProc->{$key}->{preemption};
	  delete $newProc->{$key}->{preemption};
	  last SEARCH;
	}
      }
    }
  }

  #
  # If we have a record of this CID being preempted, we need to clean up
  #

  if(!$newCID->{preemption}){
    if(pidOnly($cid) ne pidOnly($curCDA[$event->{cpu}])){
      $event->{thr} = 'K';
      $event->{mode}= 'unknown' ;
      delete $event->{owner};
    }
  }else{
    $newCID->{preemption}->{end} = $event;
    $event->{printInfo} = $event->{printInfo} .
      "fin1 $newCID->{preemption}->{begin}->{idx}/" . pidOnly($cid);
    $event->{thr} = "K";
    $event->{mode} = "scheduler";
    $event->{nextCID} = kernelCID($event);
    delete $event->{owner};
    finishPreempt($newCID->{preemption});
    delete $newCID->{preemption};
  }

  #
  # In some cases if we're switching to the kernel we'll just keep on
  # running the thread that we came in on, so if we're switching
  # to the kernel, assume we're using that thread/mode.
  # "YIELD" handling may switch this back to "K"/scheduler
  #
  if(pidOnly($cid)==0){
    if((splitCommID($cid))[2]!=0){
      $event->{nextCID} = $cid;
      $event->{thr} = "I";
      $event->{mode} = "idle";
    }elsif($commIDThread{$cid}){
      $event->{nextCID} = $cid;
      $event->{thr} = $prev->{thr};
      $event->{mode} = $prev->{mode};
    }	
  }

  # waiting implies we expect a DISPATCH_DONE event, so in fact we're still
  # in scheduling mode with this CID not running until then
  if(defined $newCID->{waiting}){
    $newCID->{waiting}->{begin} = $event;
    $event->{printInfo} .=
      " begins $newCID->{waiting}->{begin}->{idx}/" . pidOnly($cid);
    $newCID->{preemption} = $newCID->{waiting};
    beginPreempt($newCID->{preemption});
  }else{
#    my %sched = ();
#    $sched{begin} = $event;
#    $sched{type} = "scheduling";
#    $sched{pid} = pidOnly($cid);
#    beginPreempt(\%sched);
#    $newProc->{$cid}->{preemption} = \%sched;
#    $newCID = $newProc->{$cid};
    $event->{printInfo} .=
      " begins2 $event->{idx}/" . pidOnly($cid);
  }

  my $cda = $curCDA[$event->{cpu}];
  my $curr = proc(pidOnly($cda));

  $event->{currentPID} = 0;

  if($cid eq $cda){
    return;
  }
  my %preempt;
  $preempt{begin} = $event;
  $preempt{type} = "preempt";
  $preempt{pid} = pidOnly($cda);

  if(defined $curr->{$cda}->{preemption}){
    $preempt{prevMode} =  $curr->{$cda}->{preemption}->{prevMode};
    $preempt{prevThr} =  $curr->{$cda}->{preemption}->{prevThr};
    $curr->{$cda}->{preemption}->{end} = $event;
    $event->{printInfo} = $event->{printInfo} .
      " fin2 $curr->{$cda}->{preemption}->{begin}->{idx}/" . 
	pidOnly($cda);
    finishPreempt($curr->{$cda}->{preemption});
    delete $curr->{$cda}->{preemption};
  }else{
    $preempt{prevMode} = $prev->{mode};
    $preempt{prevThr} = $prev->{thr};
  }


  if(pidOnly($cid) ne pidOnly($cda)){
    my $ref = \%preempt;
    $curr->{$cda}->{preemption} = $ref;
    $event->{printInfo} = $event->{printInfo} .
      " begin2 $curr->{$cda}->{preemption}->{begin}->{idx}/" . 
	pidOnly($cda);
    beginPreempt($ref);
  }
  $curCDA[$event->{cpu}] = $cid;

}

sub pidMismatch_DISPATCH(){
  my $cid = $curCDA[$event->{cpu}];
  my $proc = proc(pidOnly($cid));
  my $info = $proc->{$cid}->{preemption};
 SWITCH:{
    if(($prev->{type} eq 'TRACE_EXCEPTION_DISPATCH' ||
	$prev->{type} eq 'TRACE_EXCEPTION_EXCEPTION_MPMSG' ||
	$prev->{type} eq 'TRACE_EXCEPTION_TIMER_INTERRUPT' ||
	$prev->{type} eq 'TRACE_EXCEPTION_PGFLT_DONE' ||
	$prev->{type} eq 'TRACE_EXCEPTION_PPC_RETURN') &&
       $prev->{mode} ne 'scheduler'){
      my %preempt;
      $preempt{begin} = $prev;
      $preempt{type} = "scheduling";
      $preempt{pid} = pidOnly($cid);
      $preempt{prevMode} = $prev->{mode};
      $preempt{prevThr} = $prev->{thr};
      if(!$proc->{$cid}->{preemption}){
	$proc->{$cid}->{preemption} = \%preempt;
	beginPreempt(\%preempt);
      }else{
	print einfo($prev) . " already preempted\n";
      }
      $prev->{nextCID} = $event->{currentPID};
      $prev->{thr} = 'K';
      $prev->{mode} = 'scheduler';
      delete $prev->{owner};
      last SWITCH;
    }
  }
};



sub startFault(){
  my $handler;
  $event->{nextCID} = kernelCID($event);

  my %extInfo;
  $extInfo{type} = "fault";
  $extInfo{pid} = pidOnly($prev->{nextCID});
  $extInfo{prevMode} = $prev->{mode};
  $extInfo{prevThr} = $prev->{thr};
  $extInfo{begin} = $event;
  $extInfo{addr} = $event->{addr};
  $extInfo{previous} = $prev->{owner};
  incCnt($event);

  $event->{enterMode} = 1;

  if($prev->{owner} && $prev->{owner} && $prev->{owner}->{type} eq "fault"){
    $event->{mode} = 'double';
  }else{
    $event->{mode} = 'fault';
  }

  my $handler;
  if($prev->{nextCID} ne kernelCID($event)){
    deleteThreadPtr($event);
    $event->{thr} = startThread($event,'fault');
    $handler = getThread($event);
  }else{
    $handler = getThread($event);
    # Fault inside the kernel --- use existing thread
    # Remember how much time the thread has already accumulated, so we
    # don't count it towards fault costs later.
    $extInfo{uncounted} = $handler->{time} + 
      1000000 * ($event->{time}-$prev->{time});

  }
  $handler->{owner} = \%extInfo;
  $event->{owner} = \%extInfo;
  $event->{printInfo} = "fault on  $event->{addr}" ;


}

sub endFault(){
  my $thr = $prev->{thr};

  my $proc = proc(pidOnly($event->{nextCID}));
#  print "end fault $event->{thr} $threadID[$event->{thr}]->{id} $event->{desc}\n";

  my $throbj = getThread($prev);
  my $faultTime;

  my $fault = $throbj->{owner};
  $fault->{end} = $event;
  decCnt($fault->{begin});

  $event->{owner} = $fault->{previous};

  $faultTime = $throbj->{time} - $fault->{uncounted} +
    1000000 * ($event->{time}-$prev->{time});

  if($fault->{prevThr} != $event->{thr}){
    delete $throbj->{owner};
    deleteThreadID($prev);
  }else{
    $throbj->{owner} = $fault->{previous};
  }
  if($fault->{type} eq "double"){
    $fault->{previous}->{uncounted} += $faultTime;
  }

  my @ctr;
  $ctr[0] = $event->{ctr1};
  $ctr[1] = $event->{ctr2};
  $ctr[2] = $event->{ctr3};
  for(my $i=0; $i<3; ++$i){
    $proc->{faults}->{$event->{faultType}}->{ctr}[$i] += $ctr[2-$i];
  }
  $proc->{faults}->{$event->{faultType}}->{count}++;
  $proc->{faults}->{$event->{faultType}}->{time}+=$faultTime;
  $proc->{faults}->{$event->{faultType}}->{stdDevTime}+=$faultTime*$faultTime;

  $event->{printInfo} .= "$event->{ctr1} $event->{ctr2} $event->{ctr3} ";
  $event->{printInfo} .= sprintf("%.2f $event->{faultType}",
				 $faultTime);
#    " to $fault->{prevMode} " ;

  $event->{mode} = $fault->{prevMode};
  $event->{thr} = $fault->{prevThr};

  $thr = getThreadID(pidOnly($event->{nextCID}),$event->{thr});
  if($thr){
    $thr->{faults}++;
    $thr->{faultTime} += $faultTime;
  }
}

sub startAsyncRemote(){

  $event->{nextCID} = $event->{cpu};
  deleteThreadPtr($event);

  my %extInfo;
  $extInfo{type} = "asyncrmt";
  $extInfo{pid} = pidOnly($prev->{nextCID});
  $extInfo{prevMode} = $prev->{mode};
  $extInfo{prevThr} = $prev->{thr};
  $extInfo{begin} = $event;
  $extInfo{previous} = $prev->{owner};
  incCnt($event);

  $event->{thr} = startThread($event,'async');
  getThread($event)->{owner} = \%extInfo;
  $event->{owner} = \%extInfo;
  $event->{mode} = "asyncrmt";
}

sub endAsyncRemote(){
  my $throbj = getThread($prev);
  my $faultTime;

  my $owner = $throbj->{owner};
  delete $throbj->{owner};
  $owner->{end} = $event;
  decCnt($owner->{begin});
  $event->{owner} = $owner->{previous};
  $event->{thr} = $owner->{prevThr};
  $event->{mode}= $owner->{prevMode};
}

sub startProcess(){
  my $pid = $event->{child};
  $procs{$pid}->{start} = $event;

  $procs{$pid}->{pid} = $pid;
  my $proc = proc($pid);
  my $parent = proc($event->{parent});
  push @{$parent->{children}}, $pid;
  $proc->{parent} = $event->{parent};

  $proc->{start} = $event;
  push @{$proc->{progs}}, $parent->{progs}[$#{$parent->{progs}}];

  $proc->{threadID}= deep_copy proc($event->{parent})->{threadID};
  foreach my $x (keys %{$proc->{threadID}}){
    my $thr = $proc->{threadID}->{$x};
    $proc->{threads}->{$thr->{key}} = $thr;
    if($thr->{lastMode} eq "SCfork" || $thr->{mode} eq "SCfork"){
      $thr->{mode} = "SCchild";
    }
  }
  $commIDThread{$pid . "00000000"} =$prev->{owner}->{prevThr};
  my $thr = getThreadID($pid, $prev->{owner}->{prevThr});
  $thr->{mode} = "SCchild";
}

sub syscallEnter () {
  my %sc = ();
  if($event->{call}=~/__k42_linux_(\S+)/){
    $event->{call} = $1;
  }
  $event->{mode} = "SC" . $event->{call};

  $sc{begin} = $event;
  $sc{prevMode} = $prev->{mode};
  getThread($event)->{syscall} = \%sc;
}

sub syscallExit() {
  if(!getThread($event)){
    return;
  }
  my $info = getThread($event)->{syscall};
  if($info){
    $event->{printInfo} = " completed $info->{begin}->{idx}";
    $event->{mode} = $info->{prevMode};
    if($event->{mode} eq "unknown"){
      $event->{mode} = "user";
    }
    delete getThread($event)->{syscall};
  }
}

sub doHWPerfMon() {
  my $name = getPtrName(pidOnly($event->{nextCID}), $event->{pc});
  $event->{printInfo} .= "sampled: $name $event->{desc}";
}
my $event_info = { 'TRACE_EXCEPTION_PPC_RETURN' =>
		   { 'name' => 'PPC_RETURN',
		     'fields' => 
		     { 'nextCID' => 0 },
		     'parseFn' => *endPPC
		   },
		   'TRACE_HWPERFMON_PERIODIC'=>
		   { 'name' => 'HWPERFMON ',
		     'fields' => 
		     { 'pc' => 0 },
		     'parseFn' => *doHWPerfMon
		   },
		   'TRACE_SCHEDULER_THREAD_CREATE' =>
		   { 'name' => 'THR_CREATE',
		     'fields' => {
				  'thrptr' => 0,
				  'fct' => 1
				 },
		     'parseFn' =>
			sub() {
			  if($prev->{type} eq "TRACE_EXCEPTION_PROCESS_YIELD"){
			    $prev->{thr} = "D";
			    $prev->{mode}= "dispatcher";
			    $event->{thr} = "D";
			    $event->{mode}= "dispatcher";
			  }
			  deleteThreadPtr($event);
			  my $thr = startThread($event,$event->{fct});
			  $thr = getThreadPtr($event);
			  $thr->{mode} = getThreadMode($event->{nextCID},
						       $event->{fct});
			  $event->{printInfo} = "new thread $thr->{mode}".
			    "/$event->{thrptr}";
			}
		   },
		   'TRACE_SCHEDULER_CUR_THREAD' =>
		   { 'name' => 'CUR_THREAD',
		     'fields' => 
		     { 'thrptr' => 0 },
		     'parseFn' => *threadSwitch
		   },
		   'TRACE_SCHEDULER_PPC_XOBJ_FCT' =>
		   { 'name' => 'PPC THREAD',
		     'fields' =>
		     { 'thrptr' => 0,
		       'fct' => 1 },
		     'parseFn' => *newPPCThread
		   },
		   'TRACE_EXCEPTION_PPC_CALL' =>
		   { 'name' => 'PPC_CALL  ',
		     'fields' => 
		     { 'nextCID' => 0 },
		     'parseFn' => 
			sub() {
			  incCnt($event);
			  $event->{thrptr} = $event->{mode} = "dispatcher";
			  $event->{thr} = "D";
			  $event->{caller} = $prev->{nextCID};
			  $event->{callerMode} = $prev->{mode};
			  $event->{callerThr} = $prev->{thr};

			  my %ppc;
			  $ppc{begin} = $event;
			  $ppc{pid} = pidOnly($prev->{nextCID});
			  $ppc{type} = "ppc";
			  $ppc{prevMode} = $prev->{mode};
			  $ppc{prevThr} = $prev->{thr};
			  $event->{owner} = \%ppc;

			  my $prevThr = getThreadID($ppc{pid}, 
						    $ppc{prevThr});
			  if($prevThr){
			    $ppc{previous} = $prevThr->{owner};
			  }
			  push @unmatchedCalls, $event;
			}
		   },
		   'TRACE_PROC_LINUX_FORK' =>
		   { 'name' => 'FORK      ',
		     'fields' =>
		     { 'parent' => 3,
		       'lparent' => 2,
		       'child' => 1,
		       'lchild' => 0
		     },
		     'parseFn' => *startProcess
		   },
		   'TRACE_USER_ENTER_PROC' =>
		   { 'name' => 'ENTER_PROC',
		     'parseFn' => 
			sub () {
			  if($event->{mode} eq "SCexecve"){
			    syscallExit();
			  }
			  $event->{mode} = "user";
			}
		   },
		   'TRACE_USER_PROC_KILL_DONE' =>
		   { 'name' => 'KILL_DONE ',
		     'fields' => { 'exitPID' => 0 },
		     'parseFn' =>
			sub () {
			  my $ppc = getThread($event)->{owner};
			  decCnt($ppc->{begin});
			  delete $ppc->{previous};
			  proc($event->{exitPID})->{exit} = $event;
			  $event->{thr} = "D";
			  $event->{mode} = "dispatcher";
			  my $proc = proc($event->{exitPID});
			  my $regex = $event->{exitPID} . '000(\S)000(\S)';
			  SEARCH:foreach my $key (keys %{$proc}){
			    if($key=~/$regex/){
			      if($proc->{$key}->{preemption}){
				$proc->{$key}->{preemption}->{end} = $event;
				finishPreempt($proc->{$key}->{preemption});
				last SEARCH;
			      }
			    }
			  }
			  deleteThreadID($event);
			}
		   },
		   'TRACE_USER_PROC_KILL' =>
		   { 'name' => 'PROC_KILL ',
		     'parseFn' =>
			sub () {
			  my $ppc = getThread($event)->{owner};
			  my $usrThr = getThreadID($event->{exitPID},
						   $ppc->{prevThr});
			  delete $usrThr->{syscall};
			  endPPC();
			  my %cleanup;
			  $cleanup{type} = "cleanup";
			  $cleanup{pid} = $event->{exitPID};
			  $cleanup{begin} = $event;
			  incCnt($event);
			  $cleanup{previous} = $ppc->{previous};
			  getThread($event)->{owner} = \%cleanup;
			  $event->{owner} = \%cleanup;
			  $event->{mode} = "cleanup";
			},
		     'fields' => { 'exitPID' => 0 },
		   },
		   'TRACE_EXCEPTION_TIMER_INTERRUPT' =>
		   { 'name' => 'TIMER     ',
		   },
		   'TRACE_EXCEPTION_EXCEPTION_MPMSG' =>
		   { 'name' => 'MPMSG RCV ',
		   },
		   'TRACE_EXCEPTION_IO_INTERRUPT' =>
		   { 'name' => 'IO INTR   ',
		   },
		   'TRACE_EXCEPTION_PROCESS_YIELD' =>
		   { 'name' => 'YIELD     ',
		     'pidMismatchFn' => *pidMismatch_DISPATCH,
		     'parseFn' => 
			sub() {
			  if($prev->{type} eq "TRACE_EXCEPTION_DISPATCH"
			     && $prev->{thr} ne "K" && $prev->{thr} ne "I"){
			    $prev->{thr} = "K";
			    $prev->{mode} = "scheduler";
			    delete $prev->{owner};

			  }

			  my $proc = proc(pidOnly($event->{nextCID}));
			  if($event->{nextCID} eq kernelCID($event)){
			    $event->{currentPID}=0;
			    $event->{thr} = "K";
			    $event->{mode} = "scheduler";
			    delete $event->{owner};
			  }else{
			    if($commIDThread{$event->{nextCID}}){
			      my $thr = getThreadID(pidOnly($event->{nextCID}),
					     $commIDThread{$event->{nextCID}});
			      $event->{owner} = $thr->{owner};
			      $event->{thr} = $thr->{id};
			      if($thr->{mode}){
				$event->{mode} = $thr->{mode};
			      }else{
				$event->{mode} = $thr->{lastMode};
			      }
			    }elsif($event->{nextCID} eq "10000"){
			      $event->{thr} = "D";
			      $event->{mode} = "idle";
			    }else{
			      $event->{thr} = "D";
			      $event->{mode} = "dispatcher";
			    }
			  }
			},
		     'fields' => 
		     { 'nextCID' => 0 }
		   },
		   'TRACE_EXCEPTION_AWAIT_DISPATCH' =>
		   { 'name' => 'WT_DISPATC',
		     'fields' => { 'thrptr' => 0 },
		     'parseFn' => *dispatchWait
		   },
		   'TRACE_EXCEPTION_DISPATCH' =>
		   { 'name' => 'DISPATCH',
		     'parseFn' => *handleProcSwitch,
		     'fields' => { 'switchtoCID' => 1 }
		   },
		   'TRACE_EXCEPTION_AWAIT_DISPATCH_DONE' =>
		   { 'name' => 'DSP DONE  ',
		     'parseFn' => *dispatchDone,
		     'fields' => { 'nextCID' => 0 },
		     'pidMismatchFn' => *pidMismatch_DISPATCH,
		   },
		   'TRACE_USER_SYSCALL_ENTER' =>
		   { 'name' => 'SC ENTER  ',
		     'fields' => { 'call' => 1 },
		     'parseFn' => *syscallEnter,
		   },
		   'TRACE_USER_SYSCALL_EXIT' =>
		   { 'name' => 'SC_EXIT   ',
		     'parseFn' => *syscallExit,
		   },
		   'TRACE_USER_START_EXEC' =>
		   { 'name' => 'START_EXEC',
		     'fields' => { 'prog' => 0 },
		     'parseFn' =>
		     sub () {
		       my $proc = proc(pidOnly($event->{nextCID}));
		       push @{$proc->{progs}}, $event->{prog};
		     }
		   },
		   'TRACE_EXCEPTION_PPC_ASYNC_REMOTE' =>
		   { 'name'=>  'ASYNC RMT ',
		     'fields' => { 'thrptr' => 0 },
		     'parseFn' => *startAsyncRemote
		   },
		   'TRACE_EXCEPTION_PPC_ASYNC_REMOTE_DONE' =>
		   { 'name'=>  'ASYNC DONE',
		     'fields' => { 'nextCID' => 0 },
		     'parseFn' => *endAsyncRemote
		   },
		   'TRACE_EXCEPTION_PGFLT' =>
		   { 'name' => 'PGFLT     ',
		     'fields' => 
		     { 'thrptr' => 0,
		       'addr'	=> 1
		     },
		     'parseFn' => *startFault,
		   },
		   'TRACE_EXCEPTION_PGFLT_DONE' =>
		   { 'name' => 'PGFLT DONE',
		     'fields' => 
		     { 'nextCID' => 0,
		       'kthr' => 1,
		       'addr' => 2,
		       'ctr1' => 4,
		       'ctr2' => 5,
		       'ctr3' => 6,
		       'faultType' => 7
		     },
		     'parseFn' => *endFault,
		   }
		 };

sub event_parse(){
  if(!defined $event_info->{$event->{type}}){
    return;
  }
  my $info = $event_info->{$event->{type}};
  $event->{name} = $info->{name};
  if(defined $info->{fields}){
    my $f;
    my $idx;
    my @data = split /\s/, $event->{desc};
    while(($f, $idx) = each %{$info->{fields}}){
      $event->{$f} = $data[$idx];
    }
  }

  if(!defined $procs{pidOnly($event->{nextCID})}){
    $procs{pidOnly($event->{nextCID})}->{pid} = pidOnly($event->{nextCID});
    $procs{pidOnly($event->{nextCID})}->{created} = $event->{idx};
    push @{$procs{pidOnly($event->{nextCID})}->{progs}}, "unknown";
  }
  if(defined $info->{parseFn}){
    $info->{parseFn}();
  }
}

my @evNum;
sub readEvent($$){
  my $fh = shift;
  my $cpu = shift;
  my $line;
  if($line = <$fh>){
    $line=~/\s*(\d+\.\d*)\s*(\S+)\s+(\S.*)$/;
    my %ev = ();
    $ev{time} = $1;
    $ev{type} = $2;
    $ev{desc} = $3;
    $ev{cpu} = $cpu;
    $ev{done} = 1;
    $ev{num} = $evNum[$cpu]++;
    return \%ev;
  }
  return undef;
}



sub readTraceFile($){
  my $files = shift;
  my $pid = 0;
  my $lastTime = 0;
  my $time = 0;
  my $idx=0;
  my @curEvents;
  $procs{"0"}->{created} = -1;
  $procs{"0"}->{pid} = "0";
  push @{$procs{"0"}->{progs}},"kernel";

  for(my $i=0; $i<$cpuCount; ++$i){
    $curCDA[$i] = 0;
    $curEvents[$i] = readEvent($files->[$i],$i);
    $curEvents[$i]->{currentPID} = 0;
  }

 LOOP:while($event = (sort { 
			   if(!defined $a) { 1; }
			   elsif(!defined $b) { -1; }
			   else{ $a->{time} <=> $b->{time};
			       }} 
		  @curEvents)[0]){
    $event->{idx} = $idx++;
    $events[$event->{cpu}][$event->{idx}]=$event;
    $curEvents[$event->{cpu}] = readEvent($files->[$event->{cpu}], 
					  $event->{cpu});
    if(defined $curEvents[$event->{cpu}]){
      $curEvents[$event->{cpu}]->{prev} = $event;
    }
    $prev = $event->{prev};
    if(!defined $prev){
      event_parse();
      next LOOP;
    }
    
    $event->{nextCID} =  $prev->{nextCID};
    $event->{thr} =  $prev->{thr};
    $event->{mode} = $prev->{mode};

    event_parse();

    if(!defined $event->{currentPID}){
      $event->{currentPID} = $prev->{nextCID};
    }
    if($event->{currentPID} ne $prev->{nextCID}){
      if(defined $event_info->{$event->{type}}->{pidMismatchFn}){
	#	print einfo($event) . "mismatch: $event->{currentPID} $event->{nextCID}\n";
	$event_info->{$event->{type}}->{pidMismatchFn}($event);
      }
    }
    
    if(!defined $event->{mode}){
      $event->{mode} = "unknown";
    }

    if(!defined $event->{owner}){
      if(getThread($event)){
	$event->{owner} = getThread($event)->{owner};
      }elsif(!getThread($prev) && $prev->{owner} && !$prev->{owner}->{end}){
	$event->{owner} = $prev->{owner};
      }
    }

    if($opts{e} && $prev->{idx} >= $startPrint){
      if(defined $prev->{printInfo}){
	printf einfo($prev) . " $prev->{cpu} %6.6s $prev->{name} %-8.8s " . 
	  "$prev->{printInfo}\n", $prev->{thr}, $prev->{mode};
      }else{
	printf einfo($prev) . " $prev->{cpu} %6.6s $prev->{name} %-8.8s  #" . 
	  "$prev->{desc}\n", $prev->{thr}, $prev->{mode};
      }
    }
    billSlot();
    cleanupThreads;
    delete $event->{prev};
    decCnt($prev);
    last if ($opts{a} && $opts{a} eq $event->{time});
  }

  if(!$opts{u}){
    return;
  }

  for(my $j =0; $j<$cpuCount; ++$j){
    for(my $i =0; $i<$#{$events[$j]}; ++$i){
      if($events[$j][$i]){
	my $event  = $events[$j][$i];
	if(defined $event->{printInfo}){
	  printf "Undeleted " . einfo($event) . 
	    " %-6.6s $event->{name} %-8.8s " . 
	      "$event->{printInfo}\n",  $event->{thr}, $event->{mode};
	}else{
	  printf "Undeleted " . einfo($event) . 
	    " %-6.6s $event->{name} %-8.8s  #".
	      "$event->{desc}\n",  $event->{thr}, $event->{mode};
	}
      }
    }
  }
}

my $modeName;
my $modeTime;
my $modeCntStr;
my $modeFaultTime;
my $modeFaultCnt;
my $modePPCTime;
my $modePPCCnt;
format STDOUT =
  @<<<<<<<<<<:@>>>>>>>>>>>/@<<<<<<<<< f:@>>>>>>>>>>@<<<<< p:@>>>>>>>>>@<<<<<<<<
$modeName,   $modeTime,   $modeCntStr,$modeFaultTime,$modeFaultCnt, $modePPCTime, $modePPCCnt
.

sub printModeTimesLong($$){
  my $modes = shift;
  my $cpu = shift;
  my $countedTime = 0;
  my $countedSlots = 0;
  my $i = 0;
  if($opts{'s'}){
    delete $modes->{syscalls}[$cpu];
    foreach my $mode (keys %{$modes}){
      if($mode=~/SC/){
	my $m = $modes->{$mode}[$cpu];
	foreach my $x (qw(time count slots faultTime ppcTime ppcs faults)){
	  $modes->{syscalls}[$cpu]->{$x} +=$m->{$x};
	}
      }
    }
  }
  foreach my $mode (sort keys %{$modes}){
    my $m = $modes->{$mode}[$cpu];
    my $time = $m->{time};
    my $str;
    next if $m->{slots}==0;
    if($opts{'s'} && $mode=~/SC/){
      next;
    }

    $modeName = $mode;
    $modeTime = sprintf("%8.2f", $time);
    $modeCntStr= sprintf("%d/%d",$m->{count}, $m->{slots});
    if($m->{faultTime} && $m->{faults}){
      $modeFaultTime= sprintf("%8.2f/", $m->{faultTime});
      $modeFaultCnt = $m->{faults};
    }else{
      undef $modeFaultTime;
      undef $modeFaultCnt;
    }
    if($m->{ppcTime} && $m->{ppcs}){
      $modePPCTime = sprintf("%8.2f/", $m->{ppcTime});
      $modePPCCnt = $m->{ppcs};
    }else{
      undef $modePPCTime;
      undef $modePPCCnt;
    }
    write STDOUT;
    $countedTime+=$m->{time};
    $countedSlots+=$m->{slots};
  }
  return ($countedSlots, $countedTime);
}

my $system;
sub procSummary($){
  my $procs = shift;
  my $slotCount = 0;
  my $pid;
  my $proc;
#  while(($pid,$proc)= each %{$procs}){
  foreach $pid  (sort { hex($a) <=> hex($b) } keys %{$procs}){
    $proc = $procs->{$pid};
    print '#' x 79;
    print "\n\n";
    print " pid: $pid parent: $proc->{start}->{parent} " . 
      "lpid: $proc->{start}->{lchild} lparent: $proc->{start}->{lparent}\n";
    print " Exec:" . join(" ",@{$proc->{progs}}) . "\n";
    my $time;
    my $slots;
    my $m;
    my $k;
    my $mode;

    foreach my $thrptr (keys %{$proc->{threads}}) {
      $proc->{thrSummary}->{$proc->{threads}->{$thrptr}->{fct}}->{count}++;
      $proc->{thrSummary}->{$proc->{threads}->{$thrptr}->{fct}}->{time} 
	+= $proc->{threads}->{$thrptr}->{time};
    }

    for(my $cpu = 0; $cpu < $cpuCount; ++$cpu){
      my $s;
      my $t;
      while(($k,$m)=each %{$proc->{modes}}){
	foreach my $x (qw(time count slots faultTime ppcTime ppcs faults)){
	  $system->{modes}->{$k}[$cpu]->{$x} +=$m->[$cpu]{$x};
	}
      }
      while(($k,$m)=each %{$proc->{extmodes}}){
	foreach my $x (qw(time count slots)){
	  $system->{extmodes}->{$k}[$cpu]->{$x} +=$m->[$cpu]{$x};
	}
      }
      ($s, $t) = printModeTimesLong($proc->{modes}, $cpu);
      $slotCount += $s;
      if($s>0){
	printf " $cpu In-process total: %8.2f/%d\n", $t, $s;
	print '  ' . ('-' x 77) . "\n";
      }

      $slots += $s;
      $time += $t;
      
      ($s, $t) = printModeTimesLong($proc->{extmodes}, $cpu);
      if($s>0){
	printf " $cpu Ex-process total: %8.2f/%d\n", $t , $s;
	$slots += $s;
	$time += $t;
	if($cpu+1 < $cpuCount){
	  print '  ' . ('-' x 77) . "\n";
	}
      }
    }
    if($proc->{exit} && $proc->{start}){
      my $wallTime = 1000000*$proc->{exit}->{time} - 
	1000000*$proc->{start}->{time};
      printf " %-25.25s\n", sprintf("wall %8.2f/%d", $wallTime);
	
    }
    print ('-' x 79) . "\n";

    my $k;
    my $f;
    while(($k,$f)=each %{$proc->{faults}}){
      $system->{faults}->{$k}->{count} += $f->{count};
      $system->{faults}->{$k}->{time} += $f->{time};
      for(my $i=0; $i<3; ++$i){
	$system->{faults}->{$k}->{ctr}[$i] += $f->{ctr}[$i];
      }
    }
    faultSummary($proc->{faults});
    while(($k, $f)=each %{$proc->{thrSummary}}){
      my $name = getPtrName($proc->{pid}, $k);
      if($name=~/\?\?/){
	$name = $k;
      }
      printf "   %-47.47s  %8.2f/%d %8.2f/%d\n", 
	$name, $f->{time}, $f->{count}, $f->{faultTime}, $f->{faults};
    }
  }
  print "Total slots billed: $slotCount\n";
}

sub faultSummary($){
  my $fltInfo = shift;
  my $count = 0;
  printf " %4.4s %5.5s %9.9s %9.9s %9.9s Name\n",
    "Tag", "Count","Time","Avg", "StdDev";
  foreach my $type (sort keys %{$fltInfo}){
    my $info = $fltInfo->{$type};
    my $faultName = "";
    for(my $i = 0; $i<=$#faultTypes; ++$i){
      if(hex($type) & (1<<$i)){
	$faultName = $faultName . "." . $faultTypes[$i]->{short};
	#	$faults[$i]->{count}+= $proc->{faults}->{$type}->{$ftype}->{count};
	#	$faults[$i]->{time}+= $proc->{faults}->{$type}->{$ftype}->{time};
      }
    }
    if($info->{ctr}[0] ||$info->{ctr}[1] ||$info->{ctr}[2]){
      $faultName .= " (";
      for(my $i=0; $i<3; ++$i){
	$faultName .= sprintf("%.2f ", 
			      (64*$info->{ctr}[$i])/
			      ($info->{count}*601));
      }
      $faultName .= ")";
    }
    
    my $stdDev;
    if($info->{count}>1){
      $stdDev = sqrt(abs(($info->{count} * $info->{stdDevTime} - 
		  $info->{time}*$info->{time})/
		 ($info->{count} * ($info->{count}-1))));
    }else{
      $stdDev = 0;
    }
    printf " %4.4s %5d %9.2f %9.2f %9.2f $faultName\n", $type,$info->{count}, 
      $info->{time},$info->{time}/$info->{count}, $stdDev;
  }
}

sub printProcessTree($){
  my $procListRef = shift;
  my @stack=();
  my @indicators=();
  my $proc;
  foreach my $pid (sort { hex($a) <=> hex($b) } keys(%{$procListRef})){
    for(;;){
      $proc = \%{$procListRef->{$pid}};
      last if $proc->{mark} eq "done";

      if($proc->{mark} ne "start"){
	$proc->{mark}="start";
	my $oldInd;
	if($proc->{parent}){
	  my $siblings = $#{@{$procListRef->{$proc->{parent}}->{children}}};
	  $oldInd = $indicators[$#stack];
	  if($siblings>=0){
	    $indicators[$#stack] ="|-";
	  }else{
	    $indicators[$#stack] ="\\-";
	  }
	}
	print(join("",@indicators) .
	      join(" ",
		   "$pid : ", $proc->{progs}[$#{$proc->{progs}}],
		   "\n"));
	if(defined $oldInd){
	  $indicators[$#stack] = $oldInd;
	}
      }
    if($#{@{$proc->{children}}}>=0){
      push (@stack,$pid);
      if($#{@{$proc->{children}}}>0){
	$indicators[$#stack] = "|  ";
      }else{
	$indicators[$#stack] = "   ";
      }
      $pid = shift(@{$proc->{children}});
      next;
    }
      $proc->{mark} = "done";
      if($proc->{parent}){
	$pid = pop(@stack);
	pop(@indicators);
    }
    }
  }
}

my $procRef;
my @files;

eval {
  my $fileBase="trace-out";
  if($opts{F}) {
      $fileBase=$opts{F};
  }
  $cpuCount = 1;
  if($opts{c}  && !$opts{r} && !$opts{R}){
    for(my $i = 0; $i<$opts{c}; ++$i){
      my $name = "$fileBase.$i.txt";
      my $fh;
      open($fh,"<$name");
      push @files, $fh;
    }
    $cpuCount = $#files;
    ++$cpuCount;
    readTraceFile(\@files);
    $procRef = \%procs;
  }elsif($opts{r}){
    open(INFILE,"<$opts{r}");
    push @files, \*INFILE;
    readTraceFile(\@files);
    $procRef = \%procs;
  }elsif($opts{R}){
    my $size;
    my $s;
    eval{
      open(UNDUMP,"<$opts{R}");
      $size = (stat(UNDUMP))[7];
      print "Reading in dump file: $opts{R}\n";
      sysread UNDUMP, $s, $size;
      close(UNDUMP);
      eval $s;
    };
    die "Can't handle %opts{R} $@" if $@;
  }else {
    print "jiggy [-c cpu_count] [-r inputfile] [-P powerpc/{build}-path/]\n";
    print "\t[-F trace-file-prefix-if-not-trace-out]\n";
    print "\t [-s (collapse syscall info)] [-t (print process tree)]\n";
    print "\t [-e (print huge event info)] \n";
    print "\t [-u (print undeleted)] [-b (print billing) ]\n";
    print "\t [-R dump-file-to read] [-D dump-file-to-create]\n";
    exit 1;
  }
};
if($@){
  printf("Death: $@\n");
  for(my $j =0; $j<2; ++$j){
    for(my $i =0; $i<$#{$events[$j]}; ++$i){
      if($events[$j][$i]){
	my $event  = $events[$j][$i];
	if(defined $event->{printInfo}){
	  printf "Undeleted " . einfo($event) . " %-6.6s $event->{name} %-8.8s " . 
	    "$event->{printInfo}\n",  $event->{thr}, $event->{mode};
	}else{
	  printf "Undeleted " . einfo($event) . " %-6.6s $event->{name} %-8.8s  #".
	    "$event->{desc}\n",  $event->{thr}, $event->{mode};
	}
      }
    }
  }
  die "Bye...";
}


if($opts{D}){
  my $d = Data::Dumper->new([$cpuCount, \%procs],["cpuCount", "procRef"]);
  open(DUMP,">$opts{D}");
  print DUMP $d->Dump;
  close(DUMP);
}


procSummary($procRef);

print("\n\nSystem-wide Time Breakdown:\n");
for(my $cpu = 0; $cpu < $cpuCount; ++$cpu){
  my $s;
  my $t;
  ($s, $t) = printModeTimesLong($system->{modes}, $cpu);
  if($s>0){
    printf " $cpu In-process total: %8.2f/%d\n", $t, $s;
    print '  ' . ('-' x 77) . "\n";
  }
  ($s, $t) = printModeTimesLong($system->{extmodes}, $cpu);
  if($s>0){
    printf " $cpu Ex-process total: %8.2f/%d\n", $t , $s;
  }
}
print ('-' x 79) . "\n";

print "\nSystem-wide page-fault breakdown:\n";

faultSummary($system->{faults});
if($opts{t}){
  printProcessTree($procRef);
}

