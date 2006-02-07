#!/bin/bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: pmu.sh,v 1.7 2005/08/22 21:49:01 bob Exp $
# ############################################################################
#

# Default trace mask 
# (OS::Info,APP:App,HW::HWPerfMon,OS::Control,OS::Proc,OS::Exception,OS::User,OS::Mem)
#traceMask=0x530914
# (HW::HWPerfMon)
traceMask=0x020000

# countingMode :
#  0: both user and kernel
#  1: kernel only
#  2: user only
countingMode=0
periodType=0

# multiplexingRound: in terms of cycles or completed instructions is 
# the period in which all programmed groups have had a chance to be counted once
multiplexingRound=1000000

# logPeriod: in terms of cycles or completed instructions is 
# the  time between two periodical recording of HPC values in the log buffer
logPeriod=1000000


traceMaskFile=/ksys/traceMask
console=/ksys/console


function setKernelFlags()
{
   flags=$1
   echo "0|C$flags" > $console
   echo "Set kernel flags to $flags"
}

function disableThinwire()
{
    echo 0x0 > /ksys/wireDaemon
    echo "Thinwire Disabled"
}

function enableThinwire()
{
    echo 0x1 > /ksys/wireDaemon
    echo "Thinwire Enabled"
}

function resetTrace()
{
   echo "0" > $traceMaskFile
   echo "T|R"  > $console
   echo "T|I"  > $console
   echo "-- Trace buffers cleared and reset"
}

function startTrace()
{
   mask=$1

   echo "-- Starting tracing with mask $1"
   echo "$mask" > $traceMaskFile
}

function stopTrace()
{
   # stop tracing
   echo "0" > $traceMaskFile
   echo "T|I" > $console
   echo "-- Trace stopped"
}

function setCountingMode()
{
   countingMode=$1
   echo "M" > $console
   echo "e" > $console
   echo "-- Counting mode is set to $countingMode" 
   echo $countingMode  > $console
}

function setPeriodType()
{
   periodType=$1
   echo "M" > $console
   echo "t" > $console
   echo "-- Period type is set to $periodType" 
   echo $periodType  > $console
}

function setMultiplexingRound()
{
   multiplexingRound=$1; 
   echo "M" > $console
   echo "R" > $console
   echo "-- Multiplexing round is set to $multiplexingRound time units"
   echo $multiplexingRound  > $console
}

function setLogPeriod()
{
   logPeriod=$1; 
   echo "M" > $console
   echo "L" > $console
   echo "-- Log period is set to $logPeriod time units"
   echo $logPeriod  > $console
}

function addCounterGroup()
{
   local group=$1 
   echo "M" > $console
   echo "G" > $console
   echo $group > $console
}

function startSampling()
{
   local delay=$1
   echo "M" > $console
   echo "0" > $console 
   echo $delay > $console 
   echo "================="
   echo "Sampling started"
   echo "================="
}

function stopSampling()
{
   echo "M" > $console
   echo "1" > $console  	
   echo "================="
   echo "Sampling stopped"
   echo "================="
}

function startCPIBreakdown()
{
   local delay=$1
   echo "M" > $console
   echo "2" > $console  	
   echo $delay > $console 
   echo "================================"
   echo "Estimating CPI Breakdown started"
   echo "================================"
}

function stopCPIBreakdown()
{
   echo "M" > $console
   echo "3" > $console  	
   echo "================================"
   echo "Estimating CPI breakdown stopped"
   echo "================================"
}

function processStart()
{
   shift

   while getopts :m:t: o
   do
      case "$o" in 
         m)  mode=$OPTARG;;
         t)  traceMask=$OPTARG;;
      esac
   done

   case $mode in 
      sampling) echo "";;            # valid input do nothing
      cpi) echo "";;                 # valid input do nothing
      * ) echo "usage: "
           echo "pmu.sh start -m sampling [delay]: to start sampling hwperf counters"
           echo "pmu.sh start -m cpi      [delay]: to start estimating cpi breakdown"
           echo "pmu.sh start [?]      : to print usage"
	   return;;
   esac 

   setKernelFlags 0x0
   resetTrace
   disableThinwire
   case $mode in 
      sampling) startSampling;;
      cpi) startCPIBreakdown;;
   esac 
   startTrace $traceMask
}

function processStop()
{
   shift
   case $1 in 
      sampling) stopSampling;;
      cpi) stopCPIBreakdown;;
      *)   echo "usage: "
           echo "pmu.sh stop sampling : to stop sampling hwperf counters"
           echo "pmu.sh stop cpi      : to stop estimating cpi breakdown"
           echo "pmu.sh stop [?]      : to print usage"
	   return;;
   esac 
   stopTrace
   enableThinwire
}

function processConfig()
{
   shift

   while getopts :e:R:G:L:g:t: o
   do
      case "$o" in 
         e)  setCountingMode $OPTARG;;
         t)  setPeriodType $OPTARG;;
         G)  addCounterGroup $OPTARG;;
         g)  delCounterGroup $OPTARG;;
         R)  setMultiplexingRound $OPTARG;;
         L)  setLogPeriod $OPTARG;;
         *)  echo "usage:"
             echo " pmu.sh config [-G groupno,share[,sampleFreq]]* -t period_type -e countin_mode -R multiplexing_round -L log_period -g groupNo"

	     echo " NOTE: -G option must come the LAST!  "
	     echo " -G  : to add a counter group "
	     echo " -g  : to remove a counter group "
	     echo " -R  : to set the multiplexing round in terms of cycles/instructions completed"
	     echo " -L  : to set the logging period in terms of cycles/instructions completed"
	     echo " -e  : to set the counting mode "
	     echo " -t  : to set the period type "
	     echo " groupno : the group id (e.g. 1, 3, 35, 44)"
	     echo " share : a power of 2 number between 1 and 32 (e.g. 4, 8)"
	     echo " sampling fequency (only for data sampling groups : "
	     echo "    specifies one out of how many "sampled" event occurances should be sampled "
	     echo " counting_mode: "
	     echo "   0: in both user and kernel levels "
	     echo "   1: in kernel level only "
	     echo "   2: in user level only "
	     echo " period_type: "
	     echo "   0: period is in terms of cycles "
	     echo "   1: period is in terms of instructions completed "
	     echo " multiplexing round: "
	     echo "   a period in terms of processor cycles in which all the "
             echo "   programmed HPC groups are once scheduled "
	     echo " log period: "
	     echo "   the period of time between two periodical recording of HPC" 
             echo "   values into the log buffer "
	    return;;
      esac
   done

}

# the main part
case $1 in
   config) processConfig $*;;
   start) processStart $*;;
   stop) processStop $*;;
   *) echo "Usage: "
      echo " -----------------"
      echo "   pmu.sh config args"
      echo "   pmu.sh start [-t mask] [-m sampling|cpi]" 
      echo "   pmu.sh stop [sampling|cpi]"

      echo "  "
      echo "Usage for config: "
      echo "-----------------"
      processConfig - -h
      echo "  "
      echo "Usage for start: "
      echo "-----------------"
      processStart - -h
      echo "  "
      echo "Usage for stop: "
      echo "-----------------"
      processStop - -h;;
esac

   
