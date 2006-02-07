/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Global.java,v 1.2 2003/12/23 20:22:20 bob Exp $
 *****************************************************************************/

package kmonPkg;

import kmonPkg.PrintfFormat;
import java.text.*;
import java.awt.*;
import java.awt.event.KeyEvent;
import java.util.Vector;
import java.io.*;

import kmonPkg.C;
import kmonPkg.CDAColorWindow;
import kmonPkg.CDAInfo;
import kmonPkg.M;
import kmonPkg.CommIDColorWindow;
import kmonPkg.CommIDInfo;
import kmonPkg.PIDColorWindow;
import kmonPkg.PIDInfo;
import kmonPkg.PrintfFormat;
import kmonPkg.ShowEvent;
import kmonPkg.ShowEventCount;
import kmonPkg.ShowEventType;
import kmonPkg.ShowString;
import kmonPkg.TraceEvent;
import kmonPkg.traceStrings;
import kmonPkg.getEventType;
import kmonPkg.traceDraw;

public class Global {
    public static traceDraw traceArea;

    public static Scrollbar horzScroll;
    public static final int horzScrollMin = 0;
    public static final int horzScrollMax = 1000000000;

    public static MsgDialog helpDialog;
    public static String helpText = new String("");

    public static ZoomWindow zoomWindow;

    public static TextArea ta;
    public static int taColSizeRatio;
    public static int taNumbRows;
    public static int taNumbCols;

    public static int[] taEntryStart = new int[C.MAX_TA_ENTRIES];
    public static int[] taEntryEnd = new int[C.MAX_TA_ENTRIES];
    public static String[] taFileLine = new String[C.MAX_TA_ENTRIES];
    public static int taEntryCount = 0;

    public static int statsStart;
    public static int statsEnd;

    public static getEventType getEventType;

    public static int windowSizeX;
    public static int windowSizeY;

    public static int chosenMajor, chosenMinor, chooseType;
    public static int drawState;
    public static TraceEvent[] traceEvent = new TraceEvent[C.MAX_TRACE_EVENTS];
    public static CDAInfo[] cdaInfo = new CDAInfo[C.MAX_CDA_INFOS];
    public static CommIDInfo[] commidInfo = new CommIDInfo[C.MAX_COMMID_INFOS];
    // commidIndex is a ordered mapping between of the actual indixes in commIDInfo
    public static int[] commidIndex = new int[C.MAX_COMMID_INFOS];
    public static PIDInfo[] pidInfo = new PIDInfo[C.MAX_PID_INFOS];
    public static int[] pidIndex = new int[C.MAX_PID_INFOS];

    public static int numbCDAInfos;
    public static int numbCommIDInfos;
    public static int numbPIDInfos;

    public static RandomAccessFile inRAF;

    public static int getCDAInfoIndex(long cdaAddr) {
	int i;

	for (i=0; i<numbCDAInfos; i++) {
	    if (cdaAddr == Global.cdaInfo[i].cdaAddr) {
		return i;
	    }
	}
	return -1;
    }

    public static int getCommIDInfoIndex(long commid) {
	int i;

	for (i=0; i<numbCommIDInfos; i++) {
	    if (commid == Global.commidInfo[i].commid) {
		return i;
	    }
	}
	return -1;
    }

    public static void setCommIDInfoNamefromPID(long pid, String name) {
	int i;
	
	System.err.println("searching for pid "+Long.toHexString(pid)
			   +" with name "+name);
	for (i=0; i<numbCommIDInfos; i++) {
	    if (pid == M.PID_FROM_COMMID(Global.commidInfo[i].commid)) {
		Global.commidInfo[i].name = name;

		System.err.println("found "+i+" val "+
				   Long.toHexString(Global.commidInfo[i].commid));
	    }
	}
    }

    public static int getPIDInfoIndex(long pid) {
	int i;

	for (i=0; i<numbPIDInfos; i++) {
	    if (pid == Global.pidInfo[i].pid) {
		return i;
	    }
	}

	return -1;
    }

    public static int getShowEventCountIndex(int majorID, int minorID) {
	int i;
	ShowEventCount sec = new ShowEventCount();

	for (i=0; i<showEventCounts.size(); i++) {
	    sec = (ShowEventCount)showEventCounts.elementAt(i);
	    if ((sec.majorID == majorID) && (sec.minorID == minorID)) {
		return i;
	    }
	}
	return -1;
    }

    public static int getShowEventTypeIndex(int majorID, int minorID) {
	int i;
	ShowEventType sevt = new ShowEventType();

	for (i=0; i<showEventTypes.size(); i++) {
	    sevt = (ShowEventType)showEventTypes.elementAt(i);
	    if ((sevt.majorID == majorID) && (sevt.minorID == minorID)) {
		return i;
	    }
	}
	return -1;
    }

    public static void reset() {
	ShowEventCount showEventCount = new ShowEventCount();
	int i;

	Global.getEventType.setVisible(false);
	Global.cdaColorWindow.setVisible(false);
	Global.commidColorWindow.setVisible(false);
	Global.pidColorWindow.setVisible(false);
	Global.horzScroll.setValues(Global.horzScrollMin, Global.horzScrollMax,
				    Global.horzScrollMin, Global.horzScrollMax);
	Global.traceArea.adjustTraceArray();
    }

    public static void initialize() {
	Global.showEvents.removeAllElements();
	Global.showStrings.removeAllElements();

	Global.numbTraceEvents = 0;

	Global.drawState = C.SHOW_NONE;

	if (Global.traceArea != null) {
	    Global.traceArea.drawEventFirst = 0;
	    Global.traceArea.drawEventLast = 0;
	}

	Global.numbCDAInfos = 0;
	Global.numbCommIDInfos = 0;
	Global.numbPIDInfos = 0;

	Global.traceEvent[0] = new TraceEvent();
	Global.numbTraceEvents = 1;
	
	Global.zoomState = C.ZOOM_NORMAL;

	Global.zoomLine1 = -1;
	Global.zoomLine1Index = 0;
	Global.zoomLine2 = -1;
	Global.zoomLine2Index = 0;
	Global.eventLine = -1;
	Global.eventLineIndex = 0;
    }

    public static int zoomLine1, zoomLine1Index, zoomLine2, zoomLine2Index;
    public static int eventLine, eventLineIndex;
    public static int zoomState;
    public static CDAColorWindow cdaColorWindow;
    public static CommIDColorWindow commidColorWindow;
    public static PIDColorWindow pidColorWindow;

    public static Vector showEvents = new Vector();
    public static Vector showStrings = new Vector();
    public static Vector showEventCounts = new Vector();
    public static Vector showEventTypes = new Vector();

    public static Cursor DEFAULT_CURSOR = new Cursor(Cursor.DEFAULT_CURSOR);
    public static Cursor CROSSHAIR_CURSOR = new Cursor(Cursor.CROSSHAIR_CURSOR);

    public static int numbTraceEvents;

    public static int ticksPerSec = 10000000;
}
