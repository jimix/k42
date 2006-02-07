/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kmon.java,v 1.17 2003/12/23 20:22:20 bob Exp $
 *****************************************************************************/

import java.awt.*;
import java.text.*;
import java.applet.*;
import java.net.*;
import java.util.Vector;
import java.util.Random;
import java.lang.*;
import java.io.*;
import java.awt.event.KeyEvent;
import java.awt.event.*;


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
import kmonPkg.MsgDialog;
import kmonPkg.ZoomWindow;

import kmonPkg.g;
import kmonPkg.Global;
import kmonPkg.traceDraw;
import kmonPkg.TraceFilter;


//  implements WindowListener KeyListener AdjustmentListener ActionListener{

class TraceWindow extends Frame
  implements ActionListener, AdjustmentListener, KeyListener, ComponentListener{
    Dimension canvasSize;
    FileDialog fileDialog;
    
    MenuBar mb;
    Menu m0, m1, m2, m3, m4;
    MenuItem mi1_1, mi1_2, mi1_3, mi1_4, mi1_5;
    MenuItem mi2_1, mi2_2, mi2_3, mi2_4;
    MenuItem mi4_1;
    TraceFilter traceFilter = new TraceFilter();
    

    void printOutEvents() {
	int i;
	String myString = new String("");
	for (i=Global.traceArea.drawEventFirst;
	     i<=Global.traceArea.drawEventLast; i++) {

	    //Global.ta.append(g.unpackTraceEventToString(i, true, -1));
	    myString += g.unpackTraceEventToString(i, true, -1);
	}
	System.out.println("starting to set text");

	Global.ta.setText(myString);
    }

    void setTimeBase() {
	String myFile = new String("");
	String myDir = new String("");

	fileDialog.setFile("trace-out.1.trc");
	fileDialog.show();
	myFile = fileDialog.getFile();
	myDir = fileDialog.getDirectory();
	if ((myFile != null) && (myDir != null)) {

	    //g.readFile(myDir+myFile, false);

	}
    }

    void openFile() {
	String myFile = new String("");
	String myDir = new String("");

	fileDialog.show();
	myFile = fileDialog.getFile();
	myDir = fileDialog.getDirectory();
	if ((myFile != null) && (myDir != null)) {
	    Global.initialize();
	    Global.reset();
	    g.readFile(myDir+myFile, false);
	    Global.drawState = C.SHOW_COMMIDS;
	    Global.traceArea.setFilename(myFile);
	    Global.traceArea.reEstablishShowEvents();
	    Global.traceArea.zoom(23);
	}
    }

    void scanFile() {
	String myFile = new String("");
	String myDir = new String("");

	fileDialog.show();
	myFile = fileDialog.getFile();
	myDir = fileDialog.getDirectory();
	if ((myFile != null) && (myDir != null)) {
	    g.scanFile(myDir+myFile);
	}
    }


    public void setMenus() {
        //Build the menu bar.
        mb = new MenuBar();
        setMenuBar(mb);

        //Build first menu in the menu bar.
        //Specifying the second argument as true
        //makes this a tear-off menu.
        m0 = new Menu("File", true);
        mb.add(m0);

        m0.add(new MenuItem("Open"));
        m0.add(new MenuItem("Scan for PID"));
        m0.add(new MenuItem("Quit"));

	m0.addActionListener(this);
        m1 = new Menu("Show", true);
        mb.add(m1);

        m1.add(new MenuItem("Show Code (alt-c)"));
	m1.addSeparator();
        m1.add(new MenuItem("Show Event"));
        m1.add(new MenuItem("Hide Event"));
        m1.add(new MenuItem("Hide All Events"));
	m1.addSeparator();
        m1.add(new MenuItem("Print Out Events"));
	m1.addSeparator();
        m1.add(new MenuItem("Show by CommID"));
        m1.add(new MenuItem("Show by Process NYI"));
        m1.add(new MenuItem("Show CDAs"));
        m1.add(new MenuItem("Show None"));
	m1.addSeparator();
        m1.add(new MenuItem("CDA colors"));
        m1.add(new MenuItem("CommID colors"));
        m1.add(new MenuItem("PID colors NYI"));

	m1.addActionListener(this);

        m2 = new Menu("Calc", true);
        mb.add(m2);

        m2.add (new MenuItem("Test Socket"));
        m2.add (new MenuItem("Set Time Base"));
        m2.add (new MenuItem("CommID Stats"));
        m2.add (new MenuItem("Execution Stats"));
        m2.add (new MenuItem("Lock Stats"));
        //m2.add (new MenuItem("Set Start"));
        //m2.add (new MenuItem("Set Intermediate"));
        //m2.add (new MenuItem("Set End"));
	m2.addActionListener(this);

        m3= new Menu("Zoom", true);
        mb.add(m3);

	m3.add (new MenuItem("To Zoom Mark"));
	m3.add (new MenuItem("Set Zoom Mark"));
	m3.addSeparator();
	m3.add (new MenuItem("To Time"));
	m3.addSeparator();
	m3.add (new MenuItem("To Event"));
	m3.addSeparator();
	m3.add (new MenuItem("Out Full"));
	m3.add (new MenuItem("Out 10x"));
	m3.add (new MenuItem("Out 5x"));
	m3.add (new MenuItem("Out 2x"));
	m3.addSeparator();
	m3.add (new MenuItem("In 2x"));
	m3.add (new MenuItem("In 5x"));
	m3.add (new MenuItem("In 10x"));
	m3.add (new MenuItem("In Full"));
	m3.addActionListener(this);

        m4 = new Menu("Help", true);
        mb.add(m4);

        mi4_1 = new MenuItem("Show Help");
        m4.add(mi4_1);
	m4.addActionListener(this);
    }

    public TraceWindow() {
        int i,j;
	Dimension taD;
	GridBagConstraints c;
	GridBagLayout gbl;
	Font fta = new Font("Courier", Font.PLAIN, 14);

	Global.initialize();
	
	Global.taNumbRows = 25;
	Global.taNumbCols = 80;
	Global.ta = new TextArea(Global.taNumbRows, Global.taNumbCols);


	Global.ta.addKeyListener(this);
	Global.ta.setEditable(false);
	Global.ta.setBackground(Color.white);
	Global.ta.setFont(fta);
	
	//	g.readFile("trace-out.0.trc", false);

	fileDialog = new FileDialog(this, "Open", FileDialog.LOAD);
	fileDialog.setFilenameFilter(traceFilter);
	fileDialog.setFile("trace-out.0.trc");

        Global.horzScroll = new Scrollbar(Scrollbar.HORIZONTAL, 
			         Global.horzScrollMin, Global.horzScrollMax, 
				 Global.horzScrollMin, Global.horzScrollMax);
					  
	Global.horzScroll.addAdjustmentListener(this);

	Global.helpDialog = new MsgDialog(this, "help");

	Global.zoomWindow = new ZoomWindow(this, "zoom window");

	setHelpText();

	Global.chosenMajor = -1;
	Global.windowSizeX = 800;
	Global.windowSizeY = 700;

        setSize(Global.windowSizeX, Global.windowSizeY);
	
	Global.drawState = C.SHOW_NONE;

	canvasSize = new Dimension(800, 200);


        Global.traceArea = new traceDraw(canvasSize, this);
	//Global.traceArea.addKeyListener(this);

	Global.getEventType = new getEventType(this, "message");
	Global.cdaColorWindow = new CDAColorWindow();
	Global.commidColorWindow = new CommIDColorWindow();
	Global.pidColorWindow = new PIDColorWindow();
	
	setMenus();

	Panel p = new Panel();
	//p.addKeyListener(this);
	p.setLayout(new BorderLayout());
	p.add("North", Global.traceArea);
	p.add("South", Global.horzScroll);

	addComponentListener(this);



        setLayout(new GridBagLayout());

	gbl = (GridBagLayout)getLayout();

	c = new GridBagConstraints();
	c.fill = GridBagConstraints.NONE;
	c.gridx = 0;
	c.gridy = 0;
	c.weightx = 1;
	c.weighty = 1;
	c.gridheight = 1;
	gbl.setConstraints(p, c);
	add(p);

	c.fill = GridBagConstraints.NONE;
	c.gridx = 0;
	c.gridy = 1;
	c.gridheight = 1;
	gbl.setConstraints(Global.ta, c);
	add(Global.ta);

	validate();

	taD = Global.ta.getPreferredSize(Global.taNumbRows, Global.taNumbCols);
	Global.taColSizeRatio = taD.width/Global.taNumbCols;
	//System.err.println("width "+taD.width+" ratio "+Global.taColSizeRatio);


	show();

	Global.traceArea.repaint();
    }
      
    public void actionPerformed(ActionEvent event) {
	String actCom = event.getActionCommand();

	if (actCom == "Show CDAs") {
	    Global.drawState = C.SHOW_CDAS;
	    Global.traceArea.adjustTraceArray();		
	}
	else if (actCom == "Show Code (alt-c)") {
	    g.showCode();
	}
	else if (actCom == "Show Event") {
	    Global.chooseType = C.CHOOSE_SHOW_EVENT;
	    Global.getEventType.display("test");
	    Global.traceArea.repaint();		
	}
	else if (actCom == "Hide Event") {
	    Global.chooseType = C.CHOOSE_HIDE_EVENT;
	    Global.getEventType.display("test");
	    Global.traceArea.repaint();		
	}
	else if (actCom == "Hide All Events") {
	    Global.getEventType.hideAllEvents();
	    Global.traceArea.repaint();		
	}
	else if (actCom == "CDA colors") {
	    Global.cdaColorWindow.display();		
	}
	else if (actCom == "CommID colors") {
	    Global.commidColorWindow.display();		
	}
	else if (actCom == "PID colors") {
	    Global.pidColorWindow.display();		
	}
	else if (actCom == "Show None") {
	    Global.drawState = C.SHOW_NONE;
	    Global.traceArea.adjustTraceArray();		
	}
	else if (actCom == "Show by CommID") {
	    Global.drawState = C.SHOW_COMMIDS;
	    Global.traceArea.adjustTraceArray();		
	}
	else if (actCom == "Show by Process") {
	    Global.drawState = C.SHOW_PIDS;
	    Global.traceArea.adjustTraceArray();		
	}
	else if (actCom == "Set Time Base") {
	    setTimeBase();
	}
	else if (actCom == "CommID Stats") {
	    g.commIDStats(Global.traceArea.drawEventFirst,
			     Global.traceArea.drawEventLast);
	}
	else if (actCom == "Execution Stats") {
	    g.executionStats();
	}
	else if (actCom == "Test Socket") {
	    g.testSocket();
	}
	else if (actCom == "Lock Stats") {
	    g.lockStats();
	}
	else if (actCom == "Print Out Events") {
	    printOutEvents();
	}
	else if (actCom == "Set Start") {
	    Global.chooseType = C.CHOOSE_START;
	    Global.getEventType.display("test");

	    System.out.println("set start");
	}
	else if (actCom == "Set Intermediate") {
	    System.out.println("set intermediate");
	}
	else if (actCom == "Set End") {
	    Global.chooseType = C.CHOOSE_END;
	    Global.getEventType.display("test");

	    System.out.println("set end");
	}

	else if (actCom == "To Zoom Mark") {
	    Global.traceArea.zoom(21);
	}
	else if (actCom == "Set Zoom Mark") {
	    Global.traceArea.zoom(22);
	}
	else if (actCom == "To Time") {
	    Global.zoomWindow.display();		

	}
	else if (actCom == "To Event") {
	    if (Global.zoomState == C.ZOOM_EVENT) {
		Global.traceArea.setCursor(Global.DEFAULT_CURSOR);
		Global.zoomState = C.ZOOM_NORMAL;
		Global.zoomLine1 = -1;
		Global.zoomLine1Index = 0;
		Global.zoomLine2 = -1;
		Global.zoomLine2Index = 0;
	    } else {
		Global.traceArea.setCursor(Global.CROSSHAIR_CURSOR);
		Global.zoomState = C.ZOOM_EVENT;
	    }
	}
	else if (actCom == "Out Full") {
	    Global.traceArea.zoom(20);
	}
	else if (actCom == "Out 10x") {
	    Global.traceArea.zoom(10);
	}
	else if (actCom == "Out 5x") {
	    Global.traceArea.zoom(5);
	}
	else if (actCom == "Out 2x") {
	    Global.traceArea.zoom(2);
	}
	else if (actCom == "In 2x") {
	    Global.traceArea.zoom(-2);
	}
	else if (actCom == "In 5x") {
	    Global.traceArea.zoom(-5);
	}
	else if (actCom == "In 10x") {
	    Global.traceArea.zoom(-10);
	}
	else if (actCom == "In Full") {
	    Global.traceArea.zoom(-20);
	}
	else if (actCom == "Open") {
	    openFile();
	}
	else if (actCom == "Scan for PID") {
	    scanFile();
	}
	else if (actCom == "Show Help") {
	    Global.helpDialog.display(Global.helpText);
	}
	else if (actCom == "Quit") {
	    System.exit(0);
	}
	//System.err.println("unknown action");

    }
      
    public void setHelpText() {
	Global.helpText = "";
	Global.helpText+="Scrolling:\n";
	Global.helpText+="    Clicking the arrow keys in the scroll bar moves\n";
	Global.helpText+="      the screen 1/4 of the way to the right or left.\n";
	Global.helpText+="    Clicking in the scroll bar to the right or left of\n";
	Global.helpText+="      indicator will move a full screen in that direction\n";
	Global.helpText+="Zooming:\n";
	Global.helpText+="    With mouse:\n";
	Global.helpText+="      Click left mouse button in gray bary below where\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";
	Global.helpText+="\n";



    }

    public void keyPressed(KeyEvent event) {
	int keyCode = event.getKeyCode();
	String modStr = new String("");

	//System.out.println("received key pressed event");

	// FIXME why doesn't this work
	//if ((mod & KeyEvent.VK_CONTROL) != 0) {
	modStr = event.getKeyModifiersText(event.getModifiers());

	//System.out.println(" mod str X"+modStr+"X");

	if (modStr.equals("Ctrl")) {
	    switch(keyCode) {
	    case KeyEvent.VK_B:
		Global.traceArea.printEventLineEvents(Global.eventLineIndex-1);
		break;
	    case KeyEvent.VK_F:
		Global.traceArea.printEventLineEvents(Global.eventLineIndex+1);
		break;
	    }
	    return;
	}

	if (modStr.equals("Meta")) {
	    switch(keyCode) {
	    case KeyEvent.VK_C:
		g.showCode();
		break;
	    }
	    return;
	}

	switch (keyCode) {
	case KeyEvent.VK_LEFT:
	case KeyEvent.VK_UP:
	    Global.traceArea.printEventLineEvents(Global.eventLineIndex-1);
	    break;
	case KeyEvent.VK_RIGHT:
	case KeyEvent.VK_DOWN:
	    Global.traceArea.printEventLineEvents(Global.eventLineIndex+1);
	    break;
	}
    }
      
    public void componentResized(ComponentEvent event) {
	Dimension kmonSize;

	kmonSize = getSize();
	Global.taNumbCols = (kmonSize.width-50)/Global.taColSizeRatio;
	Global.ta.setColumns(Global.taNumbCols);
	validate();
	show();
    }

    public void keyReleased(KeyEvent event) {
	//System.err.println("in key relesase");
	return;
    }
      
    public void keyTyped(KeyEvent event) {
	//System.err.println("in key trype");
	return;
    }

    public void adjustmentValueChanged(AdjustmentEvent e) {
	Global.traceArea.doScroll();
    }   


    public void componentMoved(ComponentEvent e) {
	System.err.println("in component moved");
    }

    public void componentShown(ComponentEvent e) {
	//System.err.println("in component shown");

    }

    public void componentHidden(ComponentEvent e) {
	System.err.println("in component hidden");
    }
}

public class kmon {
    public static void printUsage() {
	System.err.println("kmon [options]");
	System.err.println("  --help : gets this message");
	System.err.println("  --mpSync N : specify number of streams to sync");
	System.err.println("  --mpBase filename : mp base filename");
    }

    // FIXME this is really only a heuristic and even more doesn't handle the
    //       situation where the timestamps are scattered across a wrap

    public static void main(String args[]) {
	int i, numbProcs;
	TraceWindow traceWindow;
	String mpBase;

	numbProcs = 0;
	mpBase = "trace-out";

	for (i=0; i<args.length; i++) {
	    if ((args[i].equals("--help")) || (args[i].equals("-help"))) {
		printUsage();
		System.exit(0);
	    } 
	    else if (args[i].equals("--mpSync")) {
		i++;
		numbProcs = (int)Long.parseLong(args[i]);
	    }
	    else if (args[i].equals("--mpBase")) {
		i++;
		mpBase = args[i];
	    } else {
		System.err.println("unknown option: " + args[i]);
		printUsage();
		System.exit(-1);
	    }
	}

	if (numbProcs > 0) {
	    g.syncTraceStreams(numbProcs, mpBase);
	}

	if (1==2) {
	    g.readFile("trace-out.0.trc", true);

	    System.err.println("finished with file numb events "+
			       Global.numbTraceEvents);

	    for (i=0;i<Global.numbTraceEvents;i++) {
		g.unpackTraceEventToString(i, false, -1);
	    }


	} else {
	    
	    System.out.println("in main calling init");
	    
	    Global.windowSizeX = 800;
	    Global.windowSizeY = 300;
	    
	    traceWindow = new TraceWindow();
	    
	    traceWindow.setTitle("trace tool");
	    
	    traceWindow.show();
	    
	    while (1==1);
	    
	    //System.out.println("after calling init");
	}
    }
}
