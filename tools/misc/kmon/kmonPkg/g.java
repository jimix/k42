/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: g.java,v 1.10 2004/03/28 15:24:18 bob Exp $
 *****************************************************************************/

package kmonPkg;

import kmonPkg.PrintfFormat;
import java.awt.*;
import java.io.*;
import java.net.*;
import java.util.Vector;
import java.text.*;
import kmonPkg.TraceEvent;

// General useful routine

public class g {
    public static final int MAX_ARGS = 15;
    public static StringBuffer inStr = new StringBuffer(128);
    public static char[] formatStr = new char[128];
    public static long[] argsL = new long[MAX_ARGS];
    public static String[] argsS = new String[MAX_ARGS];

    public static DecimalFormat df = new DecimalFormat("   0.000000000");
    public static PrintfFormat pft = new PrintfFormat("%14.9f");
    public static PrintfFormat pfe = new PrintfFormat("%-36.36s");
    public static PrintfFormat pfsn = new PrintfFormat("%-20.20s");
    public static PrintfFormat pfsc = new PrintfFormat("%-14.14s");
    public static PrintfFormat pftt = new PrintfFormat("%5.9f");
    public static String myString = new String("");

    public static long physProc;
    public static long initTimestamp;
    public static long wrapAdjust = 0;
    public static int XXXval = 0;

    public static final int WRAP_GET = 1;
    public static final int WRAP_KNOWN = 2;
    public static final int WRAP_UNKNOWN = 3;
    public static int timestampDisposition = WRAP_UNKNOWN;

    public static final String execFile = new String("execStats.txt");
    public static final String lockFile = new String("lockStats.txt");

    /*
    public static void processProcEvent(int index) {
	long childCommID, parentCommID;

	if (Global.traceEvent[index].minorID == 
	    traceStrings.TRACE_PROC_LINUX_FORK) {
	    try {
	    
	    Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      2*C.TRACE_ENTRY_SIZE);
	    //pid = Global.inRAF.readLong();

	    } catch (IOException ioe) {
		System.err.println("IOException in read (1): " + ioe);
	    }
	}
    }
    */

    public static void syncTraceStreams(int numbProcs, String mpBase) {
	long minTimestamp;
	String filename;
	int i;

	minTimestamp = 0x7fffffffffffffffL;  // don't like this
	timestampDisposition = WRAP_GET;
	for (i=0; i<numbProcs; i++) {
	    filename = mpBase+"."+i+".trc";
	    readFile(filename, false);
	    System.err.println("stream "+ i +" timestamp " +
			       Long.toHexString(wrapAdjust));
	    if (wrapAdjust < minTimestamp) {
		minTimestamp = wrapAdjust;
	    }
	}
	System.err.println("setting wrap adjust using" + 
			   Long.toHexString(minTimestamp));
	wrapAdjust = (-1)*minTimestamp;
	timestampDisposition = WRAP_KNOWN;
    }

    public static void createNewPIDInfo(int index, long pid, long time, String name) {
	if (index != -1) {
	    Global.traceEvent[index].arg = Global.numbPIDInfos;
	}

	if (name == null) name="FIXME";

	Global.pidInfo[Global.numbPIDInfos] = new PIDInfo();
	Global.pidInfo[Global.numbPIDInfos].pid = pid;
		  
	Global.pidInfo[Global.numbPIDInfos].time = time;
	Global.pidInfo[Global.numbPIDInfos].name = new String(name);
	
	Global.numbPIDInfos++;
    }

    public static void sortCommidInfo() {
	int i;
	CommIDInfo tempCommID;

	if (Global.numbCommIDInfos == 0) return;


	tempCommID = new CommIDInfo();
	tempCommID.commid = Global.commidInfo[Global.numbCommIDInfos].commid;
	tempCommID.time = Global.commidInfo[Global.numbCommIDInfos].time;
	tempCommID.colorInd = Global.commidInfo[Global.numbCommIDInfos].colorInd;
	tempCommID.name = Global.commidInfo[Global.numbCommIDInfos].name;

	if (Global.numbCommIDInfos == 1) {
	    if (tempCommID.commid < Global.commidInfo[0].commid) {
		i=0;
		Global.commidInfo[i+1].commid = Global.commidInfo[i].commid;
		Global.commidInfo[i+1].time = Global.commidInfo[i].time;
		Global.commidInfo[i+1].colorInd = Global.commidInfo[i].colorInd;
		Global.commidInfo[i+1].name = Global.commidInfo[i].name;
	    }
	    i=-1;
	} else {
	    i = Global.numbCommIDInfos-1;
	    
	    while ((i>=1) && (tempCommID.commid < Global.commidInfo[i].commid)) {
		Global.commidInfo[i+1].commid = Global.commidInfo[i].commid;
		Global.commidInfo[i+1].time = Global.commidInfo[i].time;
		Global.commidInfo[i+1].colorInd = Global.commidInfo[i].colorInd;
		Global.commidInfo[i+1].name = Global.commidInfo[i].name;
		i--;
	    }
	}
	Global.commidInfo[i+1].commid = tempCommID.commid;
	Global.commidInfo[i+1].time = tempCommID.time;
	Global.commidInfo[i+1].colorInd = tempCommID.colorInd;
	Global.commidInfo[i+1].name = tempCommID.name;
    }

    // a bit tricky this sorts commid via the indirection table
    // specified by CommidIndex
    public static void updateCommidIndex() {
	int i, tempCommInd;

	if (Global.numbCommIDInfos == 0) {
	    Global.commidIndex[0] = 0;
	    return;
	}

	Global.commidIndex[Global.numbCommIDInfos] = Global.numbCommIDInfos;
	tempCommInd = Global.commidIndex[Global.numbCommIDInfos];

	if (Global.numbCommIDInfos == 1) {
	    if (Global.commidInfo[tempCommInd].commid < 
		Global.commidInfo[Global.commidIndex[0]].commid) {
		Global.commidIndex[1] = 0;
		Global.commidIndex[0] = 1;
	    }
	} else {
	    i = Global.numbCommIDInfos-1;
	    
	    while ((i>=0) && (Global.commidInfo[tempCommInd].commid < 
			      Global.commidInfo[Global.commidIndex[i]].commid)) {
		Global.commidIndex[i+1] = Global.commidIndex[i];
		i--;
	    }
	    Global.commidIndex[i+1] = tempCommInd;
	}
    }

    public static void createNewCommIDInfo(int index, long commID, long time) {
	long rd, pid;
	String name;

	if (index != -1) {
	    Global.traceEvent[index].arg = Global.numbCommIDInfos;
	}
	Global.commidInfo[Global.numbCommIDInfos] = new CommIDInfo();
	Global.commidInfo[Global.numbCommIDInfos].commid = commID;

	Global.commidInfo[Global.numbCommIDInfos].time = time;
	
	pid = M.PID_FROM_COMMID(commID);
	index = Global.getPIDInfoIndex(pid);
	if (index == -1) {
	    name = new String("");
	} else {
	    name = new String(Global.pidInfo[index].name);
	}
	Global.commidInfo[Global.numbCommIDInfos].name = name;

	// for now name a couple of the pid we know about
	if ((pid == 1) || (pid == 3) || (pid == 4)) {
	    Global.commidInfo[Global.numbCommIDInfos].name=new String("base servers");
	}
	if (pid == 2) {
	    Global.commidInfo[Global.numbCommIDInfos].name = new String("res manager");
	}

	if (pid > 0) { 
	    // not kernel
	    Global.commidInfo[Global.numbCommIDInfos].colorInd =
             C.procColorInd[(int)(M.PID_FROM_COMMID(commID))%C.numbProcColorInds];
	} else {
	    rd = M.RD_FROM_COMMID(commID);
	    if (rd == 0) {
		Global.commidInfo[Global.numbCommIDInfos].colorInd = C.COLOR_RED;
		Global.commidInfo[Global.numbCommIDInfos].name = new String("kernel");
	    } else {
		Global.commidInfo[Global.numbCommIDInfos].colorInd = C.COLOR_BLUE;
		Global.commidInfo[Global.numbCommIDInfos].name = new String("idle");
	    }
	}

	updateCommidIndex();

	Global.numbCommIDInfos++;
    }

    public static void processUserEvent(int index) {
	long pid;
	int lindex;

	if (Global.traceEvent[index].minorID == 
	    traceStrings.TRACE_USER_RUN_UL_LOADER) {
	    try {
	    
	    Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      2*C.TRACE_ENTRY_SIZE);
	    pid = Global.inRAF.readLong();

	    myString = unpackTraceEventToString(index, true, 2);

	    // in general? this is useless because we don't yet
	    // have commid in existence, if that's not the case
	    // i.e., we are winding up with blank commid names when
	    // we shouldn't, then re-enable
	    //Global.setCommIDInfoNamefromPID(pid, myString);

	    lindex = Global.getPIDInfoIndex(pid);
	    if (lindex != -1) {
		Global.pidInfo[lindex].name = myString;
		return;
	    }
	    //System.err.println("creating pid "+Long.toHexString(pid)+ "name"+
	    //myString);			       

	    createNewPIDInfo(index, pid, 0, myString);


	    } catch  (IOException ioe) {
		System.err.println("IOException in read (2): " + ioe);
	    }
	} 
 	else if (Global.traceEvent[index].minorID == 
 	    traceStrings.TRACE_USER_PROC_KILL) {
 	    try {
   
 	    Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
 			      C.TRACE_ENTRY_SIZE);
 	    pid = Global.inRAF.readLong();

 	    lindex = Global.getPIDInfoIndex(pid);

 	    if (lindex == -1) {
 		System.err.println("Error: received return from main without start");
 		Global.traceEvent[index].arg = 0;
 		return;
 	    }
 	    Global.traceEvent[index].arg = lindex;
 	    } catch (IOException ioe) {
 		System.err.println("IOException in read (3): " + ioe);
 	    }
	}
    }

    public static void processProcEvent(int index) {
	long parentPid, childPid, extra;
	int pindex, cindex;

	if (Global.traceEvent[index].minorID == 
	    traceStrings.TRACE_PROC_LINUX_FORK) {
	    try {
	    
	    Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      2*C.TRACE_ENTRY_SIZE);
	    childPid = Global.inRAF.readLong();
	    extra = Global.inRAF.readLong();
	    parentPid = Global.inRAF.readLong();

	    pindex = Global.getPIDInfoIndex(parentPid);
	    if (pindex == -1) {
		System.err.println("error: didn't find parent pid 0x"+
				   Long.toHexString(parentPid)+" for fork event");
		return;
	    }

	    cindex = Global.getPIDInfoIndex(childPid);
	    if (cindex != -1) {
		Global.pidInfo[cindex].name = myString+".f";
		return;
	    }

	    //System.err.println("creating fork pid "+Long.toHexString(childPid)+ 
	    //"name"+Global.pidInfo[pindex].name);

	    createNewPIDInfo(index, childPid, 0, Global.pidInfo[pindex].name+".f");

	    } catch  (IOException ioe) {
		System.err.println("IOException in read (9): " + ioe);
	    }
	} 
	else if (Global.traceEvent[index].minorID == 
		 traceStrings.TRACE_PROC_LINUX_EXEC) {
	    return;
	}
    }


    public static void processMiscKernEvent(int index) {
        try {
	    
	Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      C.TRACE_ENTRY_SIZE);

	switch(Global.traceEvent[index].minorID) {
	case traceStrings.TRACE_CONTROL_TICKS_PER_SECOND:

	    Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      C.TRACE_ENTRY_SIZE);

	    Global.ticksPerSec = (int)Global.inRAF.readLong();

	    break;
	default:
	    break;
	}
        } catch (IOException ioe) {
            System.err.println("IOException in read (4): " + ioe);
	}
    }

    public static void processExceptionEvent(int index) {
	long cdaAddr, commID;
	int lindex;

        try {
	    
	Global.inRAF.seek((long)(Global.traceEvent[index].offset) +
			      C.TRACE_ENTRY_SIZE);

	switch(Global.traceEvent[index].minorID) {
	case traceStrings.TRACE_EXCEPTION_CDA_INIT:

	    cdaAddr = Global.inRAF.readLong();
	    Global.traceEvent[index].arg = cdaAddr;
	    lindex = Global.getCDAInfoIndex(cdaAddr);

	    if (lindex == -1) {
		Global.cdaInfo[Global.numbCDAInfos] = new CDAInfo();
	    } else {
		System.err.println("error: duplicate CDA Init event "+cdaAddr);
		return;
	    }

	    Global.cdaInfo[Global.numbCDAInfos].cdaAddr = cdaAddr;
	    Global.cdaInfo[Global.numbCDAInfos].priorityClass =
	                                  (int)Global.inRAF.readLong();
	    Global.cdaInfo[Global.numbCDAInfos].quantum = (int)Global.inRAF.readLong();
	    Global.cdaInfo[Global.numbCDAInfos].weight = (int)Global.inRAF.readLong();
	    Global.cdaInfo[Global.numbCDAInfos].scale = (int)Global.inRAF.readLong();
	    Global.cdaInfo[Global.numbCDAInfos].time = 0;
	    Global.cdaInfo[Global.numbCDAInfos].colorInd =
	           C.procColorInd[Global.numbCDAInfos%C.numbProcColorInds];
	    Global.numbCDAInfos++;
	    break;
	case traceStrings.TRACE_EXCEPTION_DISPATCH:
	    cdaAddr = Global.inRAF.readLong();
	    lindex = Global.getCDAInfoIndex(cdaAddr);
	    if (lindex == -1) {
		Global.cdaInfo[Global.numbCDAInfos] = new CDAInfo();

		Global.cdaInfo[Global.numbCDAInfos].cdaAddr = cdaAddr;

		Global.cdaInfo[Global.numbCDAInfos].priorityClass = 0;
		Global.cdaInfo[Global.numbCDAInfos].quantum = 0;
		Global.cdaInfo[Global.numbCDAInfos].weight = 0;
		Global.cdaInfo[Global.numbCDAInfos].scale = 0;
		Global.cdaInfo[Global.numbCDAInfos].time = 0;
		Global.cdaInfo[Global.numbCDAInfos].colorInd =
	           C.procColorInd[Global.numbCDAInfos%C.numbProcColorInds];

		Global.traceEvent[index].arg = Global.numbCDAInfos;

		Global.numbCDAInfos++;
		// System.err.println("warning: adding cda with addr "+cdaAddr); 
	    } else {
		Global.traceEvent[index].arg = lindex;
	    }
	    break;
	case traceStrings.TRACE_EXCEPTION_PROCESS_YIELD:
	case traceStrings.TRACE_EXCEPTION_PPC_CALL:
	case traceStrings.TRACE_EXCEPTION_PPC_RETURN:
	case traceStrings.TRACE_EXCEPTION_IPC_REFUSED:
	case traceStrings.TRACE_EXCEPTION_PGFLT_DONE:
	case traceStrings.TRACE_EXCEPTION_AWAIT_DISPATCH_DONE:
	case traceStrings.TRACE_EXCEPTION_PPC_ASYNC_REMOTE_DONE:
	case traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE:

	case traceStrings.TRACE_EXCEPTION_PGFLT:
	case traceStrings.TRACE_EXCEPTION_AWAIT_DISPATCH:
	case traceStrings.TRACE_EXCEPTION_PPC_ASYNC_REMOTE:
	case traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY:
	case traceStrings.TRACE_EXCEPTION_IPC_REMOTE:
	    if ((Global.traceEvent[index].minorID == 
		 traceStrings.TRACE_EXCEPTION_PGFLT) ||
		(Global.traceEvent[index].minorID == 
		 traceStrings.TRACE_EXCEPTION_AWAIT_DISPATCH) ||
		(Global.traceEvent[index].minorID == 
		 traceStrings.TRACE_EXCEPTION_PPC_ASYNC_REMOTE) ||
		(Global.traceEvent[index].minorID == 
		 traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY)) {
		commID = physProc;
	    } else {
		commID = Global.inRAF.readLong();
	    }

	    // pid = M.PID_FROM_COMMID(commID);

	    lindex = Global.getCommIDInfoIndex(commID);

	    Global.traceEvent[index].arg = lindex;

	    if (lindex != -1) {
		// we've already added this proc info
		return;
	    }

	    createNewCommIDInfo(index, commID, 0);
	    
	    break;

	default:
	    break;
	}
        } catch (IOException ioe) {
            System.err.println("IOException in read (5): " + ioe);
	}
    }

    // overload method, if stringToGet is non negative this routine
    // will return the string in argsS[stringToGet]
    public static String unpackTraceEventToString(int index, boolean toString,
					   int stringToGet) {
	myString = "";
	int offset = Global.traceEvent[index].offset;
	int majorID = Global.traceEvent[index].majorID;
	int minorID = Global.traceEvent[index].minorID;
	long timestamp = Global.traceEvent[index].timestamp;
	int len, epi, numbArgs, j, strIndex, mainIndex, ordIndex, formIndex;
	byte inByte;
	char[] eventParse =
           traceStrings.eventParse[majorID][minorID].toCharArray();
	char[] eventMainPrint = 
           traceStrings.eventMainPrint[majorID][minorID].toCharArray();
    
	int bitNumb;
	long tmpArg;

        try {

	Global.inRAF.seek((long)(offset+C.TRACE_ENTRY_SIZE));

	if (toString) {
	    myString += pft.sprintf(((double)timestamp/(double)Global.ticksPerSec));
	    myString += " ";
	    myString += pfe.sprintf(traceStrings.minorNames[majorID][minorID]);
	} else {
	    System.out.print(pft.sprintf(((double)timestamp/(double)
					  Global.ticksPerSec)) + " ");
	    System.out.print(pfe.sprintf(traceStrings.minorNames[majorID][minorID]));

	    // System.out.print((double)timestamp/(double)Global.ticksPerSec + " ");
	    // System.out.print(traceStrings.minorNames[majorID][minorID]);
	}

	numbArgs = 0;
	len = eventParse.length;

	if (len == 0) {
	    // there's no parse argument but there still could be a string
	    for (epi=0; epi<eventMainPrint.length; epi++) {
		myString += eventMainPrint[epi];
	    }
	    if (toString) {
		myString += "\n";
	    } else {
		System.out.println(myString);
	    }
	    return myString;
	}
	bitNumb = 0;
	tmpArg=0;
	for (epi=0; epi<len; epi++) {
	    // str
	    if (eventParse[epi] == 's') {
		inByte = Global.inRAF.readByte();
		inStr.setLength(0);
		strIndex = 0;
		while (inByte != 0) {
		    inStr.append((char)inByte);
		    strIndex++;
		    inByte = Global.inRAF.readByte();
		} 

		argsS[numbArgs] = inStr.toString();

		// now keep reading to the end of the 64 bit boundary
		for (j=0; j<(7-(strIndex%8)); j++) {
		    inByte = Global.inRAF.readByte();
		}
		numbArgs++;
		epi+=3;
	    }
	    // 64
	    else if (eventParse[epi] == '6') {
		if (bitNumb != 0) {
		    numbArgs++;
		    bitNumb = 0;
		}
		argsL[numbArgs] = Global.inRAF.readLong();
		numbArgs++;
		epi+=2;
	    }
	    // 32
	    else if (eventParse[epi] == '3') {
		if ((bitNumb != 0) && (bitNumb != 32)) {
		    numbArgs++;
		    bitNumb = 0;
		}
		numbArgs++;
		epi+=2;
		if (toString) {
		    myString = "32 bits NYI\n";
		} else {
		    System.out.println("32 bits NYI");
		}
		return myString;
	    }
	    // 16
	    else if (eventParse[epi] == '1') {
		if ((bitNumb != 0) && (bitNumb != 16) &&
		    (bitNumb != 32) && (bitNumb != 48)) {
		    numbArgs++;
		    bitNumb = 0;
		}
		if (bitNumb == 0) {
		    tmpArg = Global.inRAF.readLong();
		}
		argsL[numbArgs] = (long)
		    ((((tmpArg))>>(64-(bitNumb+16))& 0xffff));		      
		numbArgs++;
		bitNumb+=16;
		epi+=2;
		if (bitNumb>63) {
		    if (bitNumb != 64) {
			System.out.println("ERROR - this is bad - look at why");
		    }
		    bitNumb = 0;
		}		
	    }
	    // 8
	    else if (eventParse[epi] == '8') {
		numbArgs++;
		epi+=1;
		if (toString) {
		    myString = "8 bits NYI\n";
		} else {
		    System.out.println("8 bits NYI");
		}
		return myString;
	    }
	    // packing
	    else if (eventParse[epi] == '(') {
		numbArgs++;
		//epi+=FIXME;
		if (toString) {
		    myString = "packing NYI\n";
		} else {
		    System.out.println("packing NYI");
		}
		return myString;
	    }
	    else {
		if (toString) {
		    myString = "ERROR unknown control type\n";
		} else {
		    System.out.println("ERROR unknown control type");
		}
		return myString;
	    }
	}
        } catch (IOException ioe) {
            System.err.println("IOException in read (6): " + ioe);
	}

	len = eventMainPrint.length;
	mainIndex = 0;

	if (stringToGet >= 0) {
	    return argsS[stringToGet];
	}
	while (mainIndex < len) {
	    if (eventMainPrint[mainIndex] == '%') {
		mainIndex++;

		//FIXME at some point we'll need to get more than 9 args
		ordIndex = ((int)(eventMainPrint[mainIndex]) - (int)'0');

		// now get the format string
		mainIndex++;
		if (eventMainPrint[mainIndex] != '[') {
		    if (toString) {
			myString = "ERROR incorrect format\n";
		    } else {
			System.out.println("ERROR incorrect format");
		    }
		    return myString;
		}

		formIndex = 0;
		mainIndex++;
		while (eventMainPrint[mainIndex] != ']') {
		    formatStr[formIndex] = eventMainPrint[mainIndex];
		    mainIndex++;
		    formIndex++;
		}
		
		if (formatStr[formIndex-1] == 's') {
		    if (toString) {
			myString+=argsS[ordIndex];
		    } else {
			System.out.print(argsS[ordIndex]);
		    }
		}
		else if (formatStr[formIndex-1] == 'x') {
		    if (toString) {
			myString+=Long.toHexString(argsL[ordIndex]);
		    } else {
			System.out.print(Long.toHexString(argsL[ordIndex]));
		    }
		}
		else if (formatStr[formIndex-1] == 'd') {
		    if (toString) {
			myString+=Long.toString(argsL[ordIndex]);
		    } else {
			System.out.print(Long.toString(argsL[ordIndex]));
		    }
		}
		else {
		    if (toString) {
			myString+="unknown format";
		    } else {
			System.out.print("unknown format");
		    }
		}
	    }
	    else {
		if (toString) {
		    myString+= eventMainPrint[mainIndex];
		} else {
		    System.out.print(eventMainPrint[mainIndex]);
		}
	    }
	    mainIndex++;
	}

	if (toString) {
	    myString+="\n";
	} else {
	    System.out.println("");
	}

	return myString;
    }

    public static void readTimeBase(String filename) {
	long lalign, lticks, lphys, ltstamp, newWrap;
	int lversion, lheaderLength, lflags;
	byte lendian, lextra;
	ltstamp = 0;

	try {
	    Global.inRAF = new RandomAccessFile(filename, "rw");

	    lendian = Global.inRAF.readByte();
	    lextra = Global.inRAF.readByte();
	    lextra = Global.inRAF.readByte();
	    lextra = Global.inRAF.readByte();
	    lversion = Global.inRAF.readInt();
	    if (lversion != C.VERSION) {
		System.err.println("wrong(1) version "+ lversion +
				   " for trace file need " + C.VERSION);
		System.exit(0);
	    }
	    lheaderLength = Global.inRAF.readInt();
	    lflags = Global.inRAF.readInt();

	    lalign = Global.inRAF.readLong();
	    lticks = (int)Global.inRAF.readLong();
	    lphys = Global.inRAF.readLong();
	    ltstamp = Global.inRAF.readLong();
        } catch (IOException ioe) {
            System.err.println("IOException in time base: " + ioe);
        }

	newWrap = initTimestamp - ltstamp;

    }

    public static void scanFile(String filename) {
	long inLong, myPid, pid;
	long offset, fileLength;
	int version, headerLength;
	byte endian, extra, majorID, inByte;
	short minorID;
	int numbTraceEvents, eventLength;
	StringBuffer inStr = new StringBuffer(128);
	String name;

	numbTraceEvents = 0;

        try {
	    Global.inRAF = new RandomAccessFile(filename, "r");
	    fileLength = Global.inRAF.length();

	    if (Global.ta != null) {
		Global.ta.setText("starting scan file\n");
		Global.ta.append("file length "+fileLength+"\n");
	    }
		
	    System.err.println("starting scan file");
	    System.err.println("file length " + fileLength);
	    

	    endian = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    version = Global.inRAF.readInt();

	    headerLength = 0;
	    if (version != C.VERSION) {
		System.err.println("wrong(2) version "+ version +
				   " for trace file need " + C.VERSION);
		System.exit(0);
	    } else {
		headerLength = Global.inRAF.readInt();
	    }

	    Global.inRAF.seek(headerLength);
	    offset = headerLength;

	    while (offset<fileLength) {

		inLong = Global.inRAF.readLong();

		eventLength = (int)M.TRACE_LENGTH_GET(inLong);
		majorID = (byte)M.TRACE_MAJOR_ID_GET(inLong);

		if (majorID == traceStrings.TRACE_USER_MAJOR_ID) {
		    minorID = (short)M.TRACE_DATA_GET(inLong);
		    if (minorID == traceStrings.TRACE_USER_RUN_UL_LOADER) {

			myPid = Global.inRAF.readLong();
			pid = Global.inRAF.readLong();

			inByte = Global.inRAF.readByte();
			inStr.setLength(0);
			while (inByte != 0) {
			    inStr.append((char)inByte);
			    inByte = Global.inRAF.readByte();
			} 
			name = new String(inStr);
			System.err.println("pid"+Long.toHexString(pid)+"X"+name+"X");
			System.err.println(" pid"+pid+"X"+inStr+"X");
			Global.setCommIDInfoNamefromPID(pid, name);
			Global.inRAF.seek(offset+C.TRACE_ENTRY_SIZE);
		    }
		}
		    
		if (eventLength == 0) {
		    System.err.println("ERROR event length 0 "+offset+" major "+
				       Global.traceEvent[numbTraceEvents].majorID+
				       " number" + numbTraceEvents);
		    eventLength = 1;
		}
		offset = offset + eventLength*C.TRACE_ENTRY_SIZE;

		Global.inRAF.skipBytes((eventLength-1)*C.TRACE_ENTRY_SIZE);

		if (numbTraceEvents%1000 == 0) {
		    if (Global.ta != null) {
			Global.ta.append(".");
			if (numbTraceEvents%50000 == 49000) {
			    Global.ta.append("\n");
			}
		    }
		}

		numbTraceEvents++;
	    }
        } catch (IOException ioe) {
            System.err.println("IOException in read (8): " + ioe);
        }

	if (Global.ta != null) {
	    Global.ta.append("\nfinished scan file\n");
	}
	System.err.println("finished scan file");

    }


    public static void readFile(String filename, boolean toOutput) {
	long cdaAddr;
	long inLong;
	long lastTime, thisTime;
	int eventLength;
	long offset, fileLength, align;
	int version, headerLength, flags;
	byte endian, extra;
	int i, numbArgs;
	int numbTraceEvents;

	numbTraceEvents = 0;
	lastTime = 0;

        try {

	    Global.inRAF = new RandomAccessFile(filename, "r");

	    fileLength = Global.inRAF.length();

	    numbTraceEvents = 0;

	    if ((Global.ta != null) && (timestampDisposition != WRAP_GET )) {
		Global.ta.setText("starting file read\n");
		Global.ta.append("file length "+fileLength+"\n");
	    }
		
	    if (timestampDisposition != WRAP_GET ) {
		System.err.println("starting file read");
		System.err.println("file length " + fileLength);
	    }
	    
	    endian = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    extra = Global.inRAF.readByte();
	    version = Global.inRAF.readInt();

	    headerLength = 0;
	    if (version != C.VERSION) {
		System.err.println("wrong(3) version "+ version +
				   " for trace file need " + C.VERSION);
		System.exit(0);

	    } else {
		headerLength = Global.inRAF.readInt();
		flags = Global.inRAF.readInt();
		align = Global.inRAF.readLong();
		Global.ticksPerSec = (int)Global.inRAF.readLong();
		physProc = Global.inRAF.readLong();
		initTimestamp = Global.inRAF.readLong();
	    }

	    if (timestampDisposition != WRAP_GET ) {
		System.err.println("Ticks per second " + Global.ticksPerSec);
	    }

	    Global.inRAF.seek(headerLength);
	    offset = headerLength;

	    inLong = Global.inRAF.readLong();
	    Global.inRAF.seek(headerLength);
	    // get the first timestamp to 0 base the rest of timestamps
	    if (timestampDisposition == WRAP_UNKNOWN) {
		wrapAdjust = (-1 * (M.TRACE_TIMESTAMP_GET(inLong)));
	    }
	    else if (timestampDisposition == WRAP_GET) {
		wrapAdjust = (M.TRACE_TIMESTAMP_GET(inLong));
		return;
	    } else {
		if (timestampDisposition != WRAP_KNOWN) {
		    System.err.println("error unknown timestamp state");
		}
	    }

	    if (timestampDisposition == WRAP_KNOWN) {
		System.err.println("initial timestamp " +
		     Long.toHexString((M.TRACE_TIMESTAMP_GET(inLong))) +
				   " using sync wrap val " +
				   Long.toHexString(wrapAdjust*(-1)));
		Global.ta.append("initial timestamp " +
		     Long.toHexString((M.TRACE_TIMESTAMP_GET(inLong))) +
				 " using sync wrap val " +
				 Long.toHexString(wrapAdjust*(-1)) + "\n");
	    } else {
		System.err.println("initial timestamp " +
				   Long.toHexString((M.TRACE_TIMESTAMP_GET(inLong))));
		Global.ta.append("initial timestamp " +
				 Long.toHexString((M.TRACE_TIMESTAMP_GET(inLong))) +
				 "\n");
	    }

	    while (offset<fileLength) {

		inLong = Global.inRAF.readLong();

		Global.traceEvent[numbTraceEvents] = new TraceEvent();

		Global.traceEvent[numbTraceEvents].majorID = 
	            (byte)M.TRACE_MAJOR_ID_GET(inLong);
		Global.traceEvent[numbTraceEvents].minorID =
	            (short)M.TRACE_DATA_GET(inLong);
		Global.traceEvent[numbTraceEvents].offset = (int)offset;

		//FIXME adjust for wraps
		thisTime = M.TRACE_TIMESTAMP_GET(inLong);


		if (thisTime < lastTime) {
		    wrapAdjust+=0x100000000L;
		}
		lastTime = thisTime;
		Global.traceEvent[numbTraceEvents].timestamp = thisTime + wrapAdjust;

		eventLength = (int)M.TRACE_LENGTH_GET(inLong);


		if ((eventLength-1)>C.NUMB_ARGS_STORED) {
		    numbArgs = C.NUMB_ARGS_STORED;
		}
		else numbArgs = eventLength-1;

		if (eventLength == 0) {
		    System.err.println("ERROR event length 0 "+offset+
				       " major "+
				       Global.traceEvent[numbTraceEvents].majorID+
				       " number" + numbTraceEvents);
		    eventLength = 1;
		}
		offset = offset + eventLength*C.TRACE_ENTRY_SIZE;
		Global.inRAF.skipBytes((eventLength-1)*C.TRACE_ENTRY_SIZE);


		if (numbTraceEvents%1000 == 0) {
		    if (Global.ta != null) {
			Global.ta.append(".");
			if (numbTraceEvents%50000 == 49000) {
			    Global.ta.append("\n");
			}
		    }
		}

		numbTraceEvents++;
	    }
	    Global.statsStart = 0;
	    Global.statsEnd = numbTraceEvents - 1;

        } catch (IOException ioe) {
            System.err.println("IOException in read (7): " + ioe);
        }

	if (Global.ta != null) {
	    Global.ta.append("\nfinished file read - starting processing\n");
	}
	System.err.println("finished file read - starting processing");

	Global.numbTraceEvents = numbTraceEvents;

	if (toOutput) return;

	// set up kernel Proc Info
	//createNewCommIDInfo(-1, 0, 0);

	for (i=0; i<numbTraceEvents; i++) {
	    if (i%1000 == 0) {
		if (Global.ta != null) {
		    Global.ta.append(".");
		    if (i%50000 == 49000) {
			Global.ta.append("\n");
		    }
		}
	    }

	    if (Global.traceEvent[i].majorID ==
		traceStrings.TRACE_EXCEPTION_MAJOR_ID) {
		    processExceptionEvent(i);
	    } else
	    if (Global.traceEvent[i].majorID ==
		traceStrings.TRACE_USER_MAJOR_ID) {
		    processUserEvent(i);
	    } else
	    if (Global.traceEvent[i].majorID ==
		traceStrings.TRACE_PROC_MAJOR_ID) {
		    processProcEvent(i);
	    } else
	    if (Global.traceEvent[i].majorID ==
		traceStrings.TRACE_MISCKERN_MAJOR_ID) {
		    processMiscKernEvent(i);
	    }
	}
	if (Global.ta != null) {
	    Global.ta.append("\nfinished processing data\n");
	}
	System.err.println("finished processing data");
    }

    public static String printCommIDInfo() {
	String myString = new String("");

	int i;
	long sumTime, diff;

	sumTime = 0;
	for (i=0; i<Global.numbCommIDInfos; i++) {
	    sumTime += Global.commidInfo[i].time;

	    myString+="CommID 0x"+
	        pfsc.sprintf(
                Long.toHexString(Global.commidInfo[Global.commidIndex[i]].commid))+
                pfsn.sprintf(Global.commidInfo[Global.commidIndex[i]].name+":")+
	        "\t time "+
	        pftt.sprintf(((double)Global.commidInfo[Global.commidIndex[i]].time/
	                      (double)Global.ticksPerSec))+"\n";

	}
	myString += "\n";

	myString += "sum of all times\t\t"+
                    pftt.sprintf((double)sumTime/(double)Global.ticksPerSec)+"\n";

	return myString;
    }
    
    public static String printCDAInfo() {
	String myString = new String("");
	int i;
	long sumTime, diff;

	sumTime = 0;
	for (i=0; i<Global.numbCDAInfos; i++) {
	    sumTime += Global.cdaInfo[i].time;
	    myString+="cda "+i+". Addr "+
	          Long.toHexString(Global.cdaInfo[i].cdaAddr)+" time "+
	          pftt.sprintf((double)Global.cdaInfo[i].time/
			       (double)Global.ticksPerSec)+
	          " pri class "+Global.cdaInfo[i].priorityClass+"\n";

	}
	myString += "\n";

	myString += "sum of all times\t\t"+
                   pftt.sprintf((double)sumTime/(double)Global.ticksPerSec)+"\n";
	diff = Global.traceEvent[Global.traceArea.drawEventLast].timestamp -
                   Global.traceEvent[Global.traceArea.drawEventFirst].timestamp;
	myString += "end-start timestamps\t"+
                   pftt.sprintf((double)diff/(double)Global.ticksPerSec)+"\n";
	return myString;
    }

    
    public static void commIDStats(int startIndex, int endIndex) {
	int i;
	long cdaAddr;
	long lastCDATime, lastCommIDTime;
	int lastCDAIndex, lastCommIDIndex;

	for (i=0; i<Global.numbCDAInfos; i++) {
	    Global.cdaInfo[i].time = 0;
	}
	lastCDATime = Global.traceEvent[startIndex].timestamp;

	for (i=0; i<Global.numbCommIDInfos; i++) {
	    Global.commidInfo[i].time = 0;
	}
	lastCommIDTime = Global.traceEvent[startIndex].timestamp;

	// A nice trick to avoid having to check every time in the inside
	//   loop whether we have a reasonable lastCDAIndex or CommIDINDEX
	//   we'll just set it to this one that we never use
	lastCDAIndex = Global.numbCDAInfos;
	if (Global.numbCDAInfos >= C.MAX_CDA_INFOS) {
	    System.err.println("Error: too many cdas");
	    return;
	}
	Global.cdaInfo[Global.numbCDAInfos] = new CDAInfo();

	lastCommIDIndex = Global.numbCommIDInfos;
	if (Global.numbCommIDInfos >= C.MAX_COMMID_INFOS) {
	    System.err.println("Error: too many commids");
	    return;
	}
	Global.commidInfo[Global.numbCommIDInfos] = new CommIDInfo();


	for (i=startIndex; i<=endIndex; i++) {
	    if (Global.traceEvent[i].majorID ==
		traceStrings.TRACE_EXCEPTION_MAJOR_ID) {

		if (Global.traceEvent[i].minorID ==
		    traceStrings.TRACE_EXCEPTION_DISPATCH) {

		    Global.cdaInfo[lastCDAIndex].time +=
	                     (Global.traceEvent[i].timestamp - lastCDATime);
		    lastCDATime = Global.traceEvent[i].timestamp;
		    lastCDAIndex = (int)Global.traceEvent[i].arg;
		}
		else if ((Global.traceEvent[i].minorID >=
			  traceStrings.TRACE_EXCEPTION_PROCESS_YIELD) &&
			 (Global.traceEvent[i].minorID <=
			  traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE)) {

		    Global.commidInfo[lastCommIDIndex].time += 
	                     (Global.traceEvent[i].timestamp - lastCommIDTime);
		    lastCommIDTime = Global.traceEvent[i].timestamp;
		    lastCommIDIndex = (int)Global.traceEvent[i].arg;
		}
	    }
	}
	Global.ta.setText(" Process Info\n");
	Global.ta.append(printCommIDInfo());
	Global.ta.append("\n\n CDA Info\n");
	Global.ta.append(printCDAInfo());
    }

    static String printInfoFromFile(String title, String filename) {
	String fileInfo = new String("");
        String str;

	fileInfo = title;
	try {
	    BufferedReader in = new BufferedReader(new FileReader(filename));

	    while ((str = in.readLine()) != null) {
		if (str.equals("__kmonflag__")) {
		    Global.taEntryStart[Global.taEntryCount] = fileInfo.length();
		    Global.taFileLine[Global.taEntryCount] = in.readLine();
		    fileInfo += in.readLine();
		    fileInfo += "\n";
		    Global.taEntryEnd[Global.taEntryCount] = fileInfo.length();

		    System.out.println("start pos "+Global.taEntryStart[Global.taEntryCount]+Global.taFileLine[Global.taEntryCount]+"end pos "+Global.taEntryEnd[Global.taEntryCount]);

		    Global.taEntryCount++;

		    if (Global.taEntryCount >= C.MAX_TA_ENTRIES) {
			System.err.println("Too many ta entries");
			System.exit(0);
		    }

		} else {
		    fileInfo += str;
		    fileInfo += "\n";
		}
	    }
	    in.close();
	} catch (IOException e) {
	}
	return fileInfo;
    }

    public static void executionStats() {
	Global.ta.setText("");
	Global.ta.append(printInfoFromFile(" Execution Statistics\n", execFile));
    }

    public static void lockStats() {
	Global.ta.setText("");
	Global.ta.append(printInfoFromFile(" Lock Statistics\n", lockFile));
    }

    public static void showCode() {
	int startPos, i;
	Socket eclipseSocket = null;
        PrintWriter out = null;
	startPos = Global.ta.getSelectionStart();

	System.out.println("pos "+startPos);

	i=0;
	while (i<Global.taEntryCount) {
	    if ((startPos >= Global.taEntryStart[i]) &&
		(startPos <= Global.taEntryEnd[i])) {

		try {
		    eclipseSocket = new Socket("localhost", 7777);
		    out = new PrintWriter(eclipseSocket.getOutputStream(), true);
		    out.println(Global.taFileLine[i]);
		    out.close();
		    eclipseSocket.close();
		} catch (UnknownHostException e) {
		    System.err.println("Don't know about host");
		    return;
		} catch (IOException e) {
		    System.err.println("Couldn't get I/O for "+ "the connection");
		    return;
		}
		
		System.out.println("sending eclipse\n " + Global.taFileLine[i]);
		i = Global.taEntryCount;
	    }
	    i++;
	}
    }

    public static void testSocket() {
	Socket eclipseSocket = null;
        PrintWriter out = null;

	Global.ta.setText(" test socket\n");

        try {
            eclipseSocket = new Socket("localhost", 7777);

            out = new PrintWriter(eclipseSocket.getOutputStream(), true);

	    
	    XXXval++;
	    if (XXXval%3==0) {
		out.println("kitchsrc/os/servers/baseServers/ResMgr.C 1084");
	    }
	    if (XXXval%3==1) {
		out.println("kitchsrc/os/servers/baseServers/ResMgr.C 937");
	    }
	    if (XXXval%3==2) {
		out.println("kitchsrc/lib/libc/scheduler/arch/powerpc/DispatcherDefaultAsm.S 941");
	    }
	    
	    out.close();
	    eclipseSocket.close();

        } catch (UnknownHostException e) {
            System.err.println("Don't know about host");
	    return;
        } catch (IOException e) {
            System.err.println("Couldn't get I/O for "+ "the connection");
	    return;
        }

    }
}
