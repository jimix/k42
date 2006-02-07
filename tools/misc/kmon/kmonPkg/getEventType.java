/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getEventType.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;

public class getEventType extends Dialog implements ActionListener, ItemListener { 
    Button okButton;
    Button closeButton;
    Button applyButton;
    List majorList = new List(10, false);
    List minorList = new List(10, false);
    List colorList = new List(10, false);
    String majorStr="";
    String minorStr="";
    Label majorLabel, minorLabel, colorLabel;
    int majorSel, minorSel, prevMajorSel;
    GridBagConstraints c;
    GridBagLayout gbl;
    static final int height = 400;


    public getEventType(Frame dw, String title) {
	super(dw, "Choose Event Type", false);
	int i;
        setLayout(new BorderLayout());

        majorLabel = new Label("Choose Major");
        minorLabel = new Label("Choose Minor");
        colorLabel = new Label("Choose Color");

	setSize(500, height);

	majorSel=0;
	minorSel=0;
	prevMajorSel=0;

	for (i=0; i<traceStrings.numbMajors; i++) {
	    majorList.add(traceStrings.majorNames[i]);
	}
	majorList.addItemListener(this);

	for (i=0; i<traceStrings.numbMinors[0]; i++) {
	    minorList.add(traceStrings.minorNames[0][i]);
	}
	minorList.addItemListener(this);

	for (i=0; i<C.NUMB_ALL_COLORS; i++) {
	    colorList.add(C.allColorNames[i]);
	}
	colorList.addItemListener(this);

	majorList.select(0);
	minorList.select(0);
	colorList.select(0);

        okButton = new Button("Okay");
	okButton.addActionListener(this);

        closeButton = new Button("Close");
	closeButton.addActionListener(this);

        applyButton = new Button("Apply");
	applyButton.addActionListener(this);

        setLayout(new GridBagLayout());

	setTitle("test");
	gbl = (GridBagLayout)getLayout();

	c = new GridBagConstraints();
	c.fill = GridBagConstraints.NONE;
	c.gridx = 0;
	c.gridy = 0;
	c.weightx = 1;
	c.weighty = 0;
	c.gridheight = 1;

	gbl.setConstraints(majorLabel, c);
	add(majorLabel);

	//c.gridwidth = GridBagConstraints.RELATIVE;
	c.weightx = 8;
	c.gridx = 1;
	c.gridy = 0;
	gbl.setConstraints(minorLabel, c);
	add(minorLabel);

	c.weightx = 1;
	c.gridwidth = 1;
	c.gridx = 7;
	c.gridy = 0;
	gbl.setConstraints(colorLabel, c);
	add(colorLabel);

	c.weightx = 1;
	c.weighty = 1;
	c.fill = GridBagConstraints.BOTH;
	c.gridx = 0;
	c.gridy = 1;
	gbl.setConstraints(majorList, c);
	add(majorList);

	c.gridwidth = GridBagConstraints.RELATIVE;
	c.weightx = 8;
	c.gridx = 1;
	c.gridy = 1;
	gbl.setConstraints(minorList, c);
	add(minorList);

	c.weightx = 1;
	c.gridwidth = 1;
	c.gridx = 7;
	c.gridy = 1;
	gbl.setConstraints(colorList, c);
	add(colorList);

	c.gridwidth = GridBagConstraints.BOTH;
	c.weightx = 1;
	c.weighty = 0;
	c.gridx = 0;
	c.gridy = 2;
	gbl.setConstraints(closeButton, c);
	add(closeButton);

	c.gridwidth = GridBagConstraints.RELATIVE;
	c.weightx = 1;
	c.weighty = 0;
	c.gridx = 1;
	c.gridy = 2;
	gbl.setConstraints(applyButton, c);
	add(applyButton);

	c.gridwidth = GridBagConstraints.REMAINDER;
	c.weightx = 1;
	c.weighty = 0;
	c.gridx = 7;
	c.gridy = 2;
	gbl.setConstraints(okButton, c);
	add(okButton);

        validate();
    }

    public void itemStateChanged(ItemEvent event) {
	Object source = event.getSource();
	int i;

	if (source == majorList) {
	    majorSel = majorList.getSelectedIndex();
	    if (majorSel != prevMajorSel) {
		
		minorList.removeAll();
		
		for (i=0; i<traceStrings.numbMinors[majorSel]; i++) {
		    minorList.add(traceStrings.minorNames[majorSel][i]);
		}
		
		prevMajorSel = majorSel;
		minorSel = 0;
		minorList.select(0);
		setVisible(true);
	    }
	}
    }


    public void hideAllEvents() {
	Global.showEvents.removeAllElements();
	Global.showEventCounts.removeAllElements();
	Global.showEventTypes.removeAllElements();
	Global.showStrings.removeAllElements();
    }

    public void actionPerformed(ActionEvent event) {
	Object source = event.getSource();
	int colorSel, i;
	ShowEvent showEvent = new ShowEvent();
	ShowString showString = new ShowString();
	ShowEventType showEventType = new ShowEventType();
	int index;

	if ((source == applyButton) || (source == okButton)) {
	    majorSel = majorList.getSelectedIndex();
	    minorSel = minorList.getSelectedIndex();
	    if (Global.chooseType == C.CHOOSE_SHOW_EVENT) {
		colorSel = colorList.getSelectedIndex();
		Global.traceArea.setShowEvents(majorSel, minorSel,
					       C.allColors[colorSel]);
	    }
	    else if (Global.chooseType == C.CHOOSE_HIDE_EVENT) {
		index = Global.getShowEventTypeIndex(majorSel, minorSel);
		if (index == -1) {
		    System.err.println("ERROR couldn't find show event type");
		} else {
		    Global.showEventTypes.removeElementAt(index);
		}

		colorSel = colorList.getSelectedIndex();
		i=0;
		while(i<Global.showEvents.size()) {
		    showEvent = (ShowEvent)Global.showEvents.elementAt(i);
		    if (showEvent.sColor == C.allColors[colorSel]) {
			Global.showEvents.removeElementAt(i);
		    } else {
			i++;
		    }
		}
	    }
	    else {
		
		majorStr = majorList.getSelectedItem();
		minorStr = minorList.getSelectedItem();
		
		Global.chosenMajor = majorSel;
		Global.chosenMinor = minorSel;
	    }
	    if (source == okButton) {
		setVisible(false);
	    }
	    Global.traceArea.repaint();
        }
	else if (source == closeButton) {
	    setVisible(false);
	    Global.traceArea.repaint();
	}

	Global.traceArea.adjustTraceArray();
        return;
    }

    public void display (String text) {
	invalidate();
	if ((Global.chooseType == C.CHOOSE_SHOW_EVENT) ||
	    (Global.chooseType == C.CHOOSE_HIDE_EVENT)) {
	    setSize(600, height);
	    colorLabel.setVisible(true);
	    colorList.setVisible(true);
	} else {
	    setSize(500, height);
	    colorLabel.setVisible(false);
	    colorList.setVisible(false);
	}
	validate();

	setVisible(true);
    }
}

