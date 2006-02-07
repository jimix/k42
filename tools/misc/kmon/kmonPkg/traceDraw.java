/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceDraw.java,v 1.3 2004/04/23 18:38:21 butrico Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.KeyEvent;
import java.util.Vector;
import java.io.*;
import java.awt.event.*;

import kmonPkg.C;
import kmonPkg.CDAInfo;
import kmonPkg.M;
import kmonPkg.CommIDInfo;
import kmonPkg.PIDInfo;
import kmonPkg.PrintfFormat;
import kmonPkg.ShowEvent;
import kmonPkg.ShowEventCount;
import kmonPkg.ShowEventType;
import kmonPkg.ShowString;
import kmonPkg.TraceEvent;
import kmonPkg.traceStrings;
import kmonPkg.getEventType;
import kmonPkg.g;
import kmonPkg.Global;

class possibleEvent {
    int index;
    int xVal;

    possibleEvent() {
	index = 0;
	xVal = 0;
    }
    possibleEvent(int i, int x) {
	index = i;
	xVal = x;
    }
}

public class traceDraw extends Canvas implements MouseListener, KeyListener {
    static final int MAX_ARRAY_WIDTH = 1000;
    static final int NOT_A_HEIGHT = 0;
    static final int FULL_HEIGHT = 1; 
    static final int MOST_HEIGHT = 2; 
    static final int LOW_HEIGHT = 3;  
    Dimension traceDrawSize;
    Image offIm;
    Graphics offGr;
    String messageStr="";
    String message1Str="";
    int MessageX, MessageY, Message1X, Message1Y, Message2X, Message2Y;
    int MessageHeight;
    int rememberDrawEventFirst, rememberDrawEventLast;
    public int drawEventFirst, drawEventLast;
    int traceArrayX, traceArrayY, traceArrayWidth, traceArrayHeight;
    int traceArrayY2;
    int selLineY1, selLineY2, X1, X2;
    Vector possibleEvents = new Vector(0, 1);
    int timeLabelY, zoomY;
    int showEventY, showEventHeight, showStringY;
    int traceArrayLY, traceArrayLHeight;
    int traceArrayMY, traceArrayMHeight;
    long drawTimeStart, drawTimeEnd, drawTimeDiff;
    static final int NUMB_TICKS = 5;  // you get one more displayed
    int[] pixelColor = new int[MAX_ARRAY_WIDTH];
    int[] pixelHeight = new int[MAX_ARRAY_WIDTH];
    StringBuffer tStr = new StringBuffer(128);
    String myStr = new String("");
    String filename = new String("");
    Frame parent;
    long zoomLine1Time, zoomLine2Time;
    int lastScrollVal, lastScrollVis;
    PrintfFormat pft = new PrintfFormat("%.6f");
    

    public traceDraw(Dimension Size, Frame par) {
	int i;

	addKeyListener(this);
	addMouseListener(this);

	parent = par;

	traceDrawSize = Size;

	setSize(traceDrawSize.width, traceDrawSize.height);

	traceArrayX = 50;
	traceArrayY = 120;
	showEventY = 100;
	showEventHeight = 80;
	showStringY = 110;
	traceArrayHeight = 60;
	traceArrayMY = 130;
	traceArrayMHeight = 50;
	traceArrayLY = 150;
	traceArrayLHeight = 30;
	selLineY1 = 105;
	traceArrayWidth = traceDrawSize.width - 100;
	selLineY2 = 195;
	timeLabelY = 195;
	zoomY = 80;
	traceArrayY2 = traceArrayY+traceArrayHeight;

	drawEventFirst = 0;
	drawEventLast = Global.numbTraceEvents - 1;

	drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	calculateDrawTimeDiff();

	zoomLine1Time = 0;
	zoomLine2Time = 0;
	lastScrollVal = 0;
	lastScrollVis = 0;

	MessageX = 10;
	MessageY = 10;
	Message1X = 10;
	Message1Y = 30;
	Message2X = 10;
	Message2Y = 50;
	MessageHeight = 20; 

	for (i=0; i<MAX_ARRAY_WIDTH; i++) {
	    pixelColor[i] = C.COLOR_BLACK;
	    pixelHeight[i] = FULL_HEIGHT;
	}
    }

    public void setFilename(String lfilename) {
	filename = lfilename;
    }

    String formDrawAreaStr() {
	String mStr;
	mStr="Showing from ";
	mStr+=pft.sprintf((float)drawTimeStart/(float)Global.ticksPerSec);
	mStr+=" to ";
	mStr+=pft.sprintf((float)drawTimeEnd/(float)Global.ticksPerSec);
	mStr+="  diff ";
	mStr+=pft.sprintf((float)drawTimeDiff/(float)Global.ticksPerSec);
	mStr+="        from event "+drawEventFirst+" to event: "+drawEventLast;
	mStr+=" numb events "+(drawEventLast-drawEventFirst);
	return(mStr);
    }

    public void adjustTraceArray() {
	int i, j, evtIndex, secIndex, lastCDAX, lastCommIDX, majorID, minorID;
	int lastColorIndex;
	ShowEvent showEvent = new ShowEvent();
	ShowEventCount showEventCount = new ShowEventCount();
	
	evtIndex = drawEventFirst;
	lastCDAX = 0;
	lastCommIDX = 0;

	Global.showEventCounts.removeAllElements();

	if (Global.eventLine != -1) {
	    if ((Global.eventLineIndex >= drawEventFirst) &&
		(Global.eventLineIndex <= drawEventLast)) {
		Global.eventLine =
	             timeToX(Global.traceEvent[Global.eventLineIndex].timestamp);
	    } else {
		Global.ta.setText("");
		Global.eventLine = -1;
	    }
	}

	for (i=0; i<Global.showEvents.size(); i++) {
	    showEvent = (ShowEvent)Global.showEvents.elementAt(i);
	    if ((showEvent.index >= drawEventFirst) &&
		(showEvent.index <= drawEventLast)) {

		majorID = Global.traceEvent[showEvent.index].majorID;
		minorID = Global.traceEvent[showEvent.index].minorID;

		secIndex = Global.getShowEventCountIndex(majorID, minorID);
		if (secIndex == -1) {
		    Global.showEventCounts.addElement(new ShowEventCount(
			majorID, minorID, 1, showEvent.sColor,
			traceStrings.minorNames[majorID][minorID]));
		} else {
		    showEventCount =
		        (ShowEventCount)Global.showEventCounts.elementAt(secIndex);
		    showEventCount.count++;
		}
	    }
	}

	for (i=0; i<traceArrayWidth; i++) {
	    pixelHeight[i] = NOT_A_HEIGHT;
	}
	lastColorIndex = C.COLOR_BLACK;
	// get initial last color index
	if ((Global.drawState==C.SHOW_CDAS) || (Global.drawState==C.SHOW_COMMIDS)) {
	    i=evtIndex;
	    while (i>=0) {
		if (Global.traceEvent[i].majorID ==
		    traceStrings.TRACE_EXCEPTION_MAJOR_ID) {
		    if (Global.drawState==C.SHOW_CDAS) {
			if (Global.traceEvent[evtIndex].minorID ==
			    traceStrings.TRACE_EXCEPTION_DISPATCH) {
			    lastColorIndex = Global.cdaInfo[(int)(Global.
				      traceEvent[i].arg)].colorInd;
			    i=0;
			}
		    } else if (Global.drawState==C.SHOW_COMMIDS) {
			if ((Global.traceEvent[evtIndex].minorID >=
			     traceStrings.TRACE_EXCEPTION_PROCESS_YIELD) &&
			    (Global.traceEvent[evtIndex].minorID <=
			     traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE)) {
			    lastColorIndex = Global.commidInfo[(int)(Global.
					     traceEvent[i].arg)].colorInd;
			    i=0;
			}
		    }
		}
		i--;
	    }
	}

	// set colors of all the pixels
	for (i=0; i<traceArrayWidth; i++) {
	    switch (Global.drawState) {
	    case C.SHOW_CDAS:
		while (timeToX(Global.traceEvent[evtIndex].timestamp)
		       == i+traceArrayX) {
		    if (Global.traceEvent[evtIndex].majorID ==
			traceStrings.TRACE_EXCEPTION_MAJOR_ID) {
			if (Global.traceEvent[evtIndex].minorID ==
			    traceStrings.TRACE_EXCEPTION_DISPATCH) {
			    if (i==lastCDAX) {
				if (pixelColor[i] == lastColorIndex) {
				    pixelHeight[i] = MOST_HEIGHT;
				} else {
				    pixelHeight[i] = LOW_HEIGHT;
				}
			    } else {
				for (j=lastCDAX+1; j<=i; j++) {
				    pixelColor[j] = lastColorIndex;
				    pixelHeight[j] = FULL_HEIGHT;
				}
				lastCDAX = i;
			    }
			    lastColorIndex = Global.cdaInfo[(int)(Global.
				      traceEvent[evtIndex].arg)].colorInd;
			}
		    }
		    evtIndex++;
		}
		break;
	    case C.SHOW_COMMIDS:	
		while (evtIndex <= Global.statsEnd &&
		       timeToX(Global.traceEvent[evtIndex].timestamp)
		       == i+traceArrayX) {
		    if (Global.traceEvent[evtIndex].majorID ==
			traceStrings.TRACE_EXCEPTION_MAJOR_ID) {

			if ((Global.traceEvent[evtIndex].minorID >=
			     traceStrings.TRACE_EXCEPTION_PROCESS_YIELD) &&
			    (Global.traceEvent[evtIndex].minorID <=
			     traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE)) {

			    if (i==lastCommIDX) {
				if (pixelColor[i] == lastColorIndex) {
				    pixelHeight[i] = MOST_HEIGHT;
				} else {
				    pixelHeight[i] = LOW_HEIGHT;
				}
			    } else {
				for (j=lastCommIDX+1; j<=i; j++) {
				    pixelColor[j] = lastColorIndex;
				    pixelHeight[j] = FULL_HEIGHT;
				}
				lastCommIDX = i;
			    }
			    lastColorIndex = Global.commidInfo[(int)(Global.
					     traceEvent[evtIndex].arg)].colorInd;
			}
		    }
		    evtIndex++;
		}
		break;
	    default:
		pixelColor[i] = C.COLOR_BLACK;
		pixelHeight[i] = FULL_HEIGHT;
		break;
	    }
	}
	// get a color for the last set of pixels
	if (evtIndex < Global.numbTraceEvents-1) {
	    switch (Global.drawState) {
	    case C.SHOW_CDAS:
		while (evtIndex < Global.numbTraceEvents-1) {
		    if (Global.traceEvent[evtIndex].majorID ==
			traceStrings.TRACE_EXCEPTION_MAJOR_ID) {
			if (Global.traceEvent[evtIndex].minorID ==
			    traceStrings.TRACE_EXCEPTION_DISPATCH) {

			    for (j=lastCDAX; j<=i; j++) {
				if (pixelHeight[j]== NOT_A_HEIGHT) {
				    pixelHeight[j] = FULL_HEIGHT;
				    pixelColor[j] = lastColorIndex;
				} else {
				    if (pixelColor[j] == lastColorIndex) {
					pixelHeight[j] = MOST_HEIGHT;
				    } else {
					pixelHeight[j] = LOW_HEIGHT;
				    }
				}
			    }
			    break;
			}
		    }
		    evtIndex++;
		}
		break;
	    case C.SHOW_COMMIDS:
		while (evtIndex < Global.numbTraceEvents-1) {
		    if (Global.traceEvent[evtIndex].majorID ==
			traceStrings.TRACE_EXCEPTION_MAJOR_ID) {

			if ((Global.traceEvent[evtIndex].minorID >=
			     traceStrings.TRACE_EXCEPTION_PROCESS_YIELD) &&
			    (Global.traceEvent[evtIndex].minorID <=
			     traceStrings.TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE)) {

			    for (j=lastCommIDX; j<=i; j++) {
				if (pixelHeight[j] == NOT_A_HEIGHT) {
				    pixelHeight[j] = FULL_HEIGHT;
				    pixelColor[j] = lastColorIndex;
				} else {
				    if (pixelColor[j] == lastColorIndex) {
					pixelHeight[j] = MOST_HEIGHT;
				    } else {
					pixelHeight[j] = LOW_HEIGHT;
				    }
				}
			    }
			    break;
			}
		    }
		    evtIndex++;
		}
		pixelColor[i] = C.COLOR_BLACK;
		break;
	    default:
		pixelColor[i] = C.COLOR_BLACK;
		break;
	    }

	}
	repaint();
    }

    public void setShowEvents(int majorSel, int minorSel, Color sColor) {
	ShowEventType showEventType = new ShowEventType();
	int index, i;

	index = Global.getShowEventTypeIndex(majorSel, minorSel);
	if (index == -1) {
	    Global.showEventTypes.addElement(
	           new ShowEventType(majorSel, minorSel, sColor));
	}

	for (i=0; i<Global.numbTraceEvents; i++) {
	    if (Global.traceEvent[i].majorID == majorSel) {
		if (Global.traceEvent[i].minorID == minorSel) {
		    Global.showEvents.addElement(new ShowEvent(i, sColor));
		    if (majorSel == traceStrings.TRACE_USER_MAJOR_ID) {
			if ((minorSel == traceStrings.TRACE_USER_RUN_UL_LOADER)
			    || (minorSel == traceStrings.TRACE_USER_PROC_KILL)) {
			    Global.showStrings.addElement(new ShowString(i, sColor,
			        Global.commidInfo[(int)Global.traceEvent[i].arg].name));
			}
		    }
		}
	    }
	}
    }

    public void reEstablishShowEvents() {
	int i;
	ShowEventType showEventType = new ShowEventType();

	for (i=0;i<Global.showEventTypes.size();i++) {
	    showEventType = (ShowEventType)Global.showEventTypes.elementAt(i);
	    setShowEvents(showEventType.majorID, showEventType.minorID,
			  showEventType.sColor);
	}
	repaint();
    }

    void resetFirstLastDrawEvent(long time1, long time2) {
	int ev;
	// FIXME should be binary search
	
	if (time1 >= Global.traceEvent[Global.numbTraceEvents-1].timestamp) {
	    System.err.println("Error: zoom zoom zoom");
	    return;
	}
	if (time2 <= Global.traceEvent[0].timestamp) {
	    System.err.println("Error: zoom zoom zoom");
	    return;
	}

	ev=0;
	while (time1 > Global.traceEvent[ev].timestamp) {
	    ev++;
	}
	drawEventFirst = ev;

	ev = Global.numbTraceEvents-1;
	while (time2 < Global.traceEvent[ev].timestamp) {
	    ev--;
	}
	drawEventLast = ev;
	while (drawEventLast <= Global.statsEnd &&
	       drawEventLast - drawEventFirst < 4) {
	    if (drawEventFirst > 0) drawEventFirst--;
	    else drawEventLast++;
	    drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	    drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	    calculateDrawTimeDiff();
	}
    }

    public void zoom(int zoomFactor) {
	long diff;

	diff = drawTimeDiff;
               
	switch (zoomFactor) {
	case 24: // zoom to time
	    String str;
	    double tim;
	    Double tps = new Double(Global.ticksPerSec);

	    str = Global.zoomWindow.startData.getText();
	    tim = (Double.valueOf(str)).doubleValue();
	    drawTimeStart = (long)((tps.doubleValue())*tim);

	    str = Global.zoomWindow.endData.getText();
	    tim = (Double.valueOf(str)).doubleValue();
	    drawTimeEnd = (long)((tps.doubleValue())*tim);

	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case 23: // zoom to not include extra heartbeat events
	    int index;

	    index = 0;
	    while ((Global.traceEvent[index].majorID == 
		    traceStrings.TRACE_CONTROL_MAJOR_ID) &&
		   (Global.traceEvent[index].minorID == 
		    traceStrings.TRACE_CONTROL_HEARTBEAT)) {
		index++;
	    }
	    drawEventFirst = index;
	    index = Global.numbTraceEvents-1;
	    while ((Global.traceEvent[index].majorID == 
		    traceStrings.TRACE_CONTROL_MAJOR_ID) &&
		   (Global.traceEvent[index].minorID == 
		    traceStrings.TRACE_CONTROL_HEARTBEAT)) {
		index--;
	    }
	    drawEventLast = index;
	    drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	    drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	    rememberDrawEventFirst = drawEventFirst;
	    rememberDrawEventLast = drawEventLast;
	    break;
	case 22: // set zoom mark
	    rememberDrawEventFirst = drawEventFirst;
	    rememberDrawEventLast = drawEventLast;
	    return;
	case 21: // to zoom mark
	    drawEventFirst = rememberDrawEventFirst;
	    drawEventLast = rememberDrawEventLast;
	    drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	    drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	    break;
	case 20: // out full
	    drawEventFirst = 0;
	    drawEventLast = Global.numbTraceEvents-1;
	    drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	    drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	    break;
	case 10: // out 10x
	    drawTimeStart = drawTimeStart - (4*diff+diff/2);
	    drawTimeEnd = drawTimeEnd + (4*diff+diff/2);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case 5: // out 5x
	    drawTimeStart = drawTimeStart - (2*diff);
	    drawTimeEnd = drawTimeEnd + (2*diff);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case 2: // out 2x
	    drawTimeStart = drawTimeStart - (diff/2);
	    drawTimeEnd = drawTimeEnd + (diff/2);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case -2: // in 2x
	    drawTimeStart = drawTimeStart + (diff/4);
	    drawTimeEnd = drawTimeEnd - (diff/4);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case -5: // in 5x
	    drawTimeStart = drawTimeStart + (2*diff/5);
	    drawTimeEnd = drawTimeEnd - (2*diff/5);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case -10: // in 10x
	    drawTimeStart = drawTimeStart + (9*diff/20);
	    drawTimeEnd = drawTimeEnd - (9*diff/20);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	case -20: // in full
	    drawTimeStart = drawTimeStart + (diff/2);
	    drawTimeEnd = drawTimeEnd - (diff/2);
	    resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	    break;
	default:
	    System.err.println("Error: unknown zoom factor");
	    break;	    
	}
	calculateDrawTimeDiff();
	setHorzScroll();
	adjustTraceArray();
	repaint();
    }

    public void doScroll() {
	int val, vis;
	
	val = Global.horzScroll.getValue();
	vis = Global.horzScroll.getVisibleAmount();

	if ((vis == lastScrollVis) & 
	    (((val-10) == lastScrollVal) || ((val+10) == lastScrollVal))) {

	    if ((val-10) == lastScrollVal) {
		drawTimeStart = drawTimeEnd;
	    } else {
		drawTimeStart = drawTimeStart - drawTimeDiff;;
	    }
	    drawTimeEnd = drawTimeStart + drawTimeDiff;
	}
	else if ((vis == lastScrollVis) & 
	    (((val-1) == lastScrollVal) || ((val+1) == lastScrollVal))) {

	    if ((val-1) == lastScrollVal) {
		drawTimeStart = drawTimeStart + (drawTimeDiff/4);
	    } else {
		drawTimeStart = drawTimeStart - (drawTimeDiff/4);
	    }
	    drawTimeEnd = drawTimeStart + drawTimeDiff;
	}
	else {
	    drawTimeStart = (long)(((((float)val)/((float)Global.horzScrollMax)) *
	         ((float)(Global.traceEvent[Global.numbTraceEvents-1].timestamp -
			  Global.traceEvent[0].timestamp))) +
				   ((float)Global.traceEvent[0].timestamp));
	    drawTimeEnd = drawTimeStart + drawTimeDiff;

	}

	/*

	System.err.println("val "+val+" vis "+vis);	

	System.err.println("adj start "+drawTimeStart+" end 2 "+drawTimeEnd+" diff "+drawTimeDiff);
	System.err.println("adj first "+drawEventFirst+" last "+drawEventLast);

	*/

	resetFirstLastDrawEvent(drawTimeStart, drawTimeEnd);
	setHorzScroll();
	adjustTraceArray();
	repaint();
    }
    

    void setHorzScroll() {
	long totald;

	totald = Global.traceEvent[Global.numbTraceEvents-1].timestamp -
                 Global.traceEvent[0].timestamp;

	Global.horzScroll.setValues((int)((Global.horzScrollMax*drawTimeStart)/totald),
				    (int)((Global.horzScrollMax*drawTimeDiff)/totald),
				    Global.horzScrollMin, Global.horzScrollMax);

	lastScrollVal = Global.horzScroll.getValue();
	lastScrollVis = Global.horzScroll.getVisibleAmount();
    }

    void calculateDrawTimeDiff() {
	drawTimeDiff = drawTimeEnd - drawTimeStart;
	if (drawTimeDiff == 0) drawTimeDiff=1;
    }

    void calculateDrawTimes() {
	drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
	drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
	drawTimeDiff = drawTimeEnd - drawTimeStart;
	if (drawTimeDiff == 0) drawTimeDiff=1;
    }

    long xToTime(int xVal) {
	return (long)
               (drawTimeStart +
               (((((1000000*(xVal-traceArrayX))/(traceArrayWidth)) *
		  (drawTimeDiff)))/1000000));
    }

    int timeToX(long timeVal) {
	return (int)(((traceArrayWidth* 
		  ((1000000*(timeVal-drawTimeStart))/drawTimeDiff))/1000000)+
		traceArrayX);
    }
    
    void paintTraceArray(Graphics g) {
	int eventCount, cdaIndex, i, errCount;
	ShowEvent showEvent = new ShowEvent();
	ShowEventCount showEventCount = new ShowEventCount();
	ShowString showString = new ShowString();

	//System.err.println("in paint trace array");

	g.setColor(Color.lightGray);
	g.fillRect(0, 0, traceDrawSize.width, traceDrawSize.height);

	g.setColor(Color.gray);
	g.fillRect(0, 100, traceDrawSize.width, traceDrawSize.height-100);
	g.setColor(Color.yellow);
	g.drawString(filename, 0, 115);
	g.setColor(Color.black);
	g.fillRect(traceArrayX, traceArrayY, traceArrayWidth, traceArrayHeight);

	errCount=0;
	for (i=0; i<traceArrayWidth; i++) {
	    g.setColor(C.allColors[pixelColor[i]]);
	    if (pixelHeight[i] == NOT_A_HEIGHT) {
		errCount++;
		//System.err.println("ERROR NOT a height at"+i);
	    } else if (pixelHeight[i] == FULL_HEIGHT) {
		g.drawLine(traceArrayX+i, traceArrayY, traceArrayX+i,
			   traceArrayY+traceArrayHeight);
	    } else if (pixelHeight[i] == MOST_HEIGHT) {
		g.drawLine(traceArrayX+i, traceArrayMY, traceArrayX+i,
			   traceArrayMY+traceArrayMHeight);
	    } else {
		g.drawLine(traceArrayX+i, traceArrayLY, traceArrayX+i,
			   traceArrayLY+traceArrayLHeight);
	    }
	}
	if (errCount>0) {
	    //System.err.println("there were "+errCount+" height errors");
	}

	for (i=0; i<Global.showEvents.size(); i++) {
	    showEvent = (ShowEvent)Global.showEvents.elementAt(i);
	    if ((showEvent.index >= drawEventFirst) &&
		(showEvent.index <= drawEventLast)) {

		X1 = timeToX(Global.traceEvent[showEvent.index].timestamp);
		g.setColor(showEvent.sColor);
		g.drawLine(X1, showEventY, X1, showEventY+showEventHeight);
	    }
	}

	for (i=0; i<Global.showStrings.size(); i++) {
	    showString = (ShowString)Global.showStrings.elementAt(i);
	    if ((showString.index >= drawEventFirst) &&
		(showString.index <= drawEventLast)) {

		X1 = timeToX(Global.traceEvent[showString.index].timestamp);
		g.setColor(showString.sColor);
		g.drawString(showString.str, X1, showStringY);
	    }
	}

	g.setColor(Color.black);
	g.drawString("secs", 2, timeLabelY);
	for (i=0; i<=NUMB_TICKS; i++) {
	    X1 = traceArrayX+(((1000*i*traceArrayWidth)/NUMB_TICKS)/1000);
	    
	    g.drawLine(X1, traceArrayY2, X1, selLineY2);
	    g.drawString(pft.sprintf(((float)(xToTime(X1)))/
				     (float)Global.ticksPerSec),
			 X1+2, timeLabelY);
	}

	g.setColor(Color.black);
	messageStr = formDrawAreaStr();
	g.drawString(messageStr, MessageX, MessageY);
	i=0; 

	while ((i<Global.showEventCounts.size()) && (i<9)) {

	    g.setColor(Color.gray);
	    g.fillRect(Message1X+((i/3)*(traceDrawSize.width/3)),
		       Message1Y-MessageHeight+5+(MessageHeight*(i%3)),
		       traceDrawSize.width/3, MessageHeight);

	    showEventCount = (ShowEventCount)Global.showEventCounts.elementAt(i);
	    g.setColor(showEventCount.sColor);
	    g.drawString(showEventCount.name+" "+showEventCount.count,
			 Message1X+((i/3)*(traceDrawSize.width/3)),
			 Message1Y+MessageHeight*(i%3));
	    i++;
	}

	/*
	g.setColor(Color.red);
	if ((Global.statsStart >= drawEventFirst) &&
	    (Global.statsStart <= drawEventLast)) {


	    X1 = timeToX(Global.traceEvent[Global.statsStart].timestamp);
	    g.drawLine(X1, traceArrayY, X1, traceArrayY+traceArrayHeight);
	}
	if ((Global.statsEnd >= drawEventFirst) &&
	    (Global.statsEnd <= drawEventLast)) {
	    X1 = timeToX(Global.traceEvent[Global.statsEnd].timestamp);
	    g.drawLine(X1, traceArrayY, X1, traceArrayY+traceArrayHeight);
	}
	*/

	if (Global.chosenMajor != -1) {

	    eventCount = 0;

	    g.setColor(Color.cyan);

	    for (i=drawEventFirst; i<drawEventLast; i++) {
		if ((Global.traceEvent[i].majorID == Global.chosenMajor) &&
		    (Global.traceEvent[i].minorID == Global.chosenMinor)) {
		    eventCount++;
		    X1 = timeToX(Global.traceEvent[i].timestamp);
		    g.drawLine(X1, selLineY1, X1, selLineY2);
		    
		    possibleEvents.addElement(new possibleEvent(i, X1));
		    System.err.println("x1 "+X1+" index "+i);
		}
	    }
	    messageStr="There are "+eventCount+" events of your chosen";
	    messageStr+="type - pick one";
	    System.err.println(messageStr);
	}

	g.setColor(Color.yellow);
	if (Global.zoomLine1 != -1) {
	    g.drawLine(Global.zoomLine1, zoomY,
		       Global.zoomLine1, traceDrawSize.height);
	}
	if (Global.zoomLine2 != -1) {
	    g.drawLine(Global.zoomLine2, zoomY,
		       Global.zoomLine2, traceDrawSize.height);
	}
	g.setColor(Color.orange);
	if (Global.eventLine != -1) {
	    g.drawLine(Global.eventLine, zoomY,
		       Global.eventLine, traceDrawSize.height);
	}
    }

    public void repaintNow() {
	paintTraceArray(offGr);
    }

    public void paint(Graphics g) {
      update(g);
    }

    public void printEventLineEvents(int index) {
	int j, pos1, pos2;
	long time1, time2, diff;

	Global.eventLineIndex = index;
	if ((index>5) && (index<(Global.numbTraceEvents-5-1))) {
	    myStr = "";
	    for (j=index-5;j<index;j++) {
		myStr+=g.unpackTraceEventToString(j, true, -1);
	    }
	    pos1 = myStr.length();
	    myStr+=g.unpackTraceEventToString(index, true, -1);
	    pos2 = myStr.length();
	    for (j=index+1;j<=index+5;j++) {
		myStr+=g.unpackTraceEventToString(j, true, -1);
	    }
	    Global.ta.setText(myStr);
	    Global.ta.select(pos1, pos2);
	    
	} else {
	    Global.ta.setText(g.unpackTraceEventToString(index, true, -1));
	}
	if ((index<=drawEventFirst) || (index>=drawEventLast)) {
	    diff = Global.traceEvent[drawEventLast].timestamp -
	              Global.traceEvent[drawEventFirst].timestamp;
	    if (index<=drawEventFirst) {
		time1 = Global.traceEvent[drawEventFirst].timestamp - (diff/2);
		time2 = Global.traceEvent[drawEventLast].timestamp - (diff/2);
	    } else {
		time1 = Global.traceEvent[drawEventFirst].timestamp + (diff/2);
		time2 = Global.traceEvent[drawEventLast].timestamp + (diff/2);
	    }
	    resetFirstLastDrawEvent(time1, time2);
	    setHorzScroll();
	    adjustTraceArray();
	}
	Global.eventLine = timeToX(Global.traceEvent[index].timestamp);
	repaint();
    }

    public void update(Graphics g) {
	if (offGr == null) {
	  offIm = createImage(traceDrawSize.width, traceDrawSize.height);
	  offGr = offIm.getGraphics();
	}

	paintTraceArray(offGr);

	g.drawImage(offIm, 0, 0, this);

        return;
    }
    public void mousePressed(MouseEvent event) {
	long timeVal1, timeVal2;
	ShowEvent showEvent = new ShowEvent();
	int xVal, i;
	int evtMod = event.getModifiers();
	int evtX = event.getX();
	int evtY = event.getY();


	if (Global.chosenMajor == -1) {
	    // FIXME get constants for modifiers
	    // left mouse
	    if (evtMod == 16) {
		if (Global.zoomLine1 != -1) {
		    Global.zoomLine1 = -1;
		} else {
		    if (Global.zoomState == C.ZOOM_NORMAL) {
			Global.zoomLine1 = evtX;
			zoomLine1Time = xToTime(evtX);
			i=drawEventFirst;
			      
			while (i<drawEventLast) {
			    if (zoomLine1Time<=Global.traceEvent[i].timestamp) break;
			    i++;
			}
			Global.zoomLine1Index = i;
		    } else {
			// walk through show event find first event within
			//      5 x units of click
			for (i=0; i<Global.showEvents.size(); i++) {
			    showEvent=(ShowEvent)Global.showEvents.elementAt(i);
			    xVal = timeToX(
					   Global.traceEvent[showEvent.index].timestamp);
			    if ((xVal >= evtX-5) && (xVal <= evtX+5)) {
				Global.zoomLine1Index = showEvent.index;
				Global.zoomLine1 = xVal;
				repaint();
				return;
			    }
			}
		    }
		}
	    }
	    // right mouse
	    else if (evtMod == 4) {
		if (Global.zoomLine2 != -1) {
		    Global.zoomLine2 = -1;
		} else {
		    if (Global.zoomState == C.ZOOM_NORMAL) {
			Global.zoomLine2 = evtX;
			      
			zoomLine2Time = xToTime(evtX);
			i=drawEventFirst;
			      
			while (i<drawEventLast) {
			    if (zoomLine2Time<=Global.traceEvent[i].timestamp) break;
			    i++;
			}
			Global.zoomLine2Index = i;
		    } else {
			// walk through show event find first event within
			//      5 x units of click
			for (i=0; i<Global.showEvents.size(); i++) {
			    showEvent=(ShowEvent)Global.showEvents.elementAt(i);
			    xVal = timeToX(
					   Global.traceEvent[showEvent.index].timestamp);
			    if ((xVal >= evtX-5) && (xVal <= evtX+5)) {
				Global.zoomLine2Index = showEvent.index;
				Global.zoomLine2 = xVal;
				repaint();
				return;
			    }
			}
		    }
		}
	    }
	    // middle mouse
	    else if (evtMod == 8) {
		if ((Global.zoomLine1 != -1) && (Global.zoomLine2 != -1)) {

		    drawTimeStart = zoomLine1Time;
		    drawTimeEnd = zoomLine2Time;

		    drawEventFirst = Global.zoomLine1Index;
		    drawEventLast = Global.zoomLine2Index;
		    calculateDrawTimeDiff();

		    setCursor(Global.DEFAULT_CURSOR);
		    while (drawEventLast - drawEventFirst < 4) {
			if (drawEventFirst > 0) drawEventFirst--;
			else drawEventLast++;
			drawTimeStart = Global.traceEvent[drawEventFirst].timestamp;
			drawTimeEnd = Global.traceEvent[drawEventLast].timestamp;
			calculateDrawTimeDiff();
		    }

		    setHorzScroll();
		    adjustTraceArray();
		    Global.zoomState = C.ZOOM_NORMAL;
		    Global.zoomLine1 = -1;
		    Global.zoomLine1Index = 0;
		    Global.zoomLine2 = -1;
		    Global.zoomLine2Index = 0;

		}
		else if ((Global.zoomLine1 == -1) && (Global.zoomLine2 == -1)) {
		    if ((evtX>=traceArrayX) && (evtX<=traceArrayX+traceArrayWidth)) {
			Global.eventLine = evtX;
			timeVal1 = xToTime(evtX);
			i=drawEventFirst;

			while (i<drawEventLast) {
			    if (timeVal1 <= Global.traceEvent[i].timestamp) break;
			    i++;
			}
			printEventLineEvents(i);
		    } else {
			Global.ta.setText("");
			Global.eventLine = -1;
		    }
		}
	    }
	    repaint();
	    return;
	}
	    
	possibleEvent pev = new possibleEvent();

	if (Global.chosenMajor != -1) {

	    for (i=0; i<possibleEvents.size(); i++) {
		pev = (possibleEvent)possibleEvents.elementAt(i);

		if ((evtX > (pev.xVal - 5)) && (evtX < (pev.xVal + 5))) {
			  
			  
		    Global.chosenMajor = -1;

		    if (Global.chooseType == C.CHOOSE_START) {
			Global.statsStart = pev.index;
		    } else if (Global.chooseType == C.CHOOSE_END) {
			Global.statsEnd = pev.index;
		    } else if (Global.chooseType == C.CHOOSE_INTERMEDIATE) {

		    } else {
			System.err.println("help what's choose type");
		    }

		    repaint();
		    possibleEvents.removeAllElements();
		    return;
		}
	    }
	}

	repaint();
	return;
    }

    public void mouseReleased(MouseEvent event) {
    }

    public void mouseEntered(MouseEvent event) {
    }

    public void mouseExited(MouseEvent event) {
    }

    public void mouseClicked(MouseEvent event) {
    }

    public void keyPressed(KeyEvent event) {
	//System.err.println("in key press");
    }

    public void keyReleased(KeyEvent event) {
	//System.err.println("in key release");
    }

    public void keyTyped(KeyEvent event) {
	//System.err.println("in key type");
    }
    
}
