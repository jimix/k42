#!/usr/bin/env bash
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: kuservalues.sh,v 1.58 2005/06/03 21:40:59 apw Exp $
# ############################################################################

#
# Construct userid-specific values in local variables.
#
kuser_trace_log_numb_bufs=2
		    
ksimether_addr=none

logname=$(id -un)

if [ -f .k42env ] ; then
   . .k42env;
elif [ -f ~/.k42env ] ; then
    . ~/.k42env ;
fi

case $logname in
    bob)	kuser_port=40000; 
		;;
    rosnbrg)	kuser_port=40100; 
		;;
    dje)	kuser_port=40200; 
		;;
    marc)	kuser_port=40300; 
		;;
    okrieg)	kuser_port=40400; 
		;;
    jimix)	kuser_port=40500; 
		;;
    jappavoo)   kuser_port=40600;  
	        ;;
    dilma)	kuser_port=40700; 
		;;
    mostrows)	kuser_port=40800; 
		;;
    mergen)	kuser_port=40900; 
		;;
    kitchawa)	kuser_port=41000; 
		;;
    k42)	kuser_port=41100; 
		;;
    ahmedg)	kuser_port=41200; 
		;;
    butrico)	kuser_port=41300; 
		;;
    lefurgy)	kuser_port=41400; 
	        ;;
    lbsoares)	kuser_port=41500; 
	        ;;

    ben)	kuser_port=6792; 
		;;
    jonathan)   kuser_port=4486;  
		;;
    khui)	kuser_port=4450; 
		;;
    simpson)    kuser_port=41600;
		;;
    cbcoloha)	kuser_port=41700;
		;;
    donf)	kuser_port=41800;
		;;
    ggoodson)	kuser_port=41900;
		;;
    tamda)	kuser_port=42000;
		;;
    soules)	kuser_port=42100;
		;;
    bashore)	kuser_port=42200;
		;;
    nthomas)	kuser_port=42300;
		;;
    kimbrel)	kuser_port=42400;
		;;
    craig)	kuser_port=42500;
		;;
    brianw)	kuser_port=42700;
	        ;;
    ericvh)	kuser_port=42600;
	        ;;
    hshafi)	kuser_port=42800;
	        ;;
    karthick) kuser_port=42900;
	        ;;
    kartick) kuser_port=43000;
	        ;;
    mambo) kuser_port=43100;
	        ;;
    mkistler) kuser_port=43200;
	        ;;
    mootaz) kuser_port=43300;
	        ;;
    pbohrer) kuser_port=43400;
			;;
    peterson) kuser_port=43500;
			;;
    rajamony) kuser_port=43600;
			;;
    rawson) kuser_port=43700;
			;;
    speight) kuser_port=43800;
			;;
    tnakra) kuser_port=43900;
			;;
    wmf) kuser_port=44000;
			;;
    zhangl|zhanglix) kuser_port=44100;
			;;
    andrewb|aabauman)
		kuser_port=44200;
			;;
    awaterl|apw) 
		kuser_port=44300;
			;;
    azimi) 
		kuser_port=44400;
			;;
    duester) 
		kuser_port=44500;
			;;
    sudeep) 
		kuser_port=44600;
			;;
    cascaval) 
		kuser_port=44700;
			;;
    gktse) 
		kuser_port=44800;
			;;
    ealeon) 
		kuser_port=44900;
			;;
    neamtui) 
		kuser_port=45000;
			;;
    bseshas) 
		kuser_port=45100;
			;;
    *)		
    		: ${kuser_port:=none}; 
esac

#
# Assign values to real variables, but don't override environment.
#
: ${USR_BASE_PORT:=$kuser_port}
: ${K42_TRACE_LOG_NUMB_BUFS:=$kuser_trace_log_numb_bufs}



#
# Print warning and assign default values if $logname is unrecognized AND
# environment does not define one or more of the real variables.
#
if [[ ($TW_BASE_PORT = none) || ($K42_IP_ADDRESS = none) ]]; then
    echo "$0:  WARNING:  User $logname not recognized.  Using defaults:"
    if [[ $TW_BASE_PORT = none ]]; then
	TW_BASE_PORT=2103
	echo "                  K42_SIMULATOR_PORT: $K42_SIMULATOR_PORT"
    fi
    if [[ $K42_IP_ADDRESS = none ]]; then
	K42_IP_ADDRESS=192.168.1.127
	echo "                  K42_IP_ADDRESS:     $K42_IP_ADDRESS"
    fi
    echo "$0: Please add "$logname" to kitchsrc/tools/misc/kuservalues to avoid problems."
fi
if [[ ${1:-noecho} = "-echo" ]]; then
    echo TW_BASE_PORT=$K42_SIMULATOR_PORT
    echo K42_IP_ADDRESS=$K42_IP_ADDRESS
fi


for i in USR_BASE_PORT K42_TRACE_LOG_NUMB_BUFS ; do
    eval "echo $i=$`eval echo $i`" 
done


