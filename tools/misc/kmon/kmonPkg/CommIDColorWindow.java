/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CommIDColorWindow.java,v 1.1 2003/12/11 15:08:48 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;

public class CommIDColorWindow extends Frame implements ActionListener, ItemListener{
    static final int NUMB_TEXT_FIELDS = 20;
    int commidCount, firstCommid, lastCommid;
    Button closeButton;
    GridBagConstraints c;
    GridBagLayout gbl;
    Choice rangeChoice = new Choice();
    Choice[] commidChoice = new Choice[NUMB_TEXT_FIELDS];
    TextField[] commidText = new TextField[NUMB_TEXT_FIELDS];
    

    public CommIDColorWindow() {
	int i,j;
	String str;
        setLayout(new BorderLayout());
	commidCount = 0;

        setLayout(new GridBagLayout());
	gbl = (GridBagLayout)getLayout();
	c = new GridBagConstraints();

	i=0;
	str = "                showing commids from "+i+" to ";
	str += +(i+NUMB_TEXT_FIELDS-1)+"               ";
	rangeChoice.addItem(str);
	rangeChoice.addItemListener(this);

	c.fill = GridBagConstraints.HORIZONTAL;
	c.weightx = 1;
	c.weighty = 1;
	c.gridheight = 1;
	c.gridx = 1;
	c.gridy = 0;

	for (i=0;i<NUMB_TEXT_FIELDS;i++) {
	    c.gridx = 0;
	    c.gridy = i;

	    commidText[i] = new TextField();
	    commidText[i].setForeground(Color.black);
	    if (i<Global.numbCommIDInfos) {
		commidText[i].setText("commid "+i+". "+
				      Global.commidInfo[Global.commidIndex[i]].name);
	    } else {
		commidText[i].setText("commid "+i);
	    }
	    commidText[i].setEditable(false);
	    gbl.setConstraints(commidText[i], c);
	    add(commidText[i]);

	    c.gridx = 1;
	    c.gridy = i;
	    commidChoice[i] = new Choice();
	    for (j=0;j<C.NUMB_ALL_COLORS; j++) {
		commidChoice[i].addItem(C.allColorNames[j]);
		commidChoice[i].addItemListener(this);
	    }
	    commidChoice[i].select(C.COLOR_BLACK);

	    gbl.setConstraints(commidChoice[i], c);
	    add(commidChoice[i]);
	}

	c.gridx = 0;
	c.gridy = NUMB_TEXT_FIELDS;
	c.gridwidth = 2;
	gbl.setConstraints(rangeChoice, c);
	add(rangeChoice);

	c.gridy = NUMB_TEXT_FIELDS+1;
        closeButton = new Button("close");
	closeButton.addActionListener(this);
	gbl.setConstraints(closeButton, c);
	add(closeButton);

        validate();
    }


    public void setDisplayValues() {
	String str;
	int i, ind;

	invalidate();

	if (commidCount != Global.numbCommIDInfos) {

	    rangeChoice.removeAll();
	    
	    commidCount = Global.numbCommIDInfos;
	    for (i=0; i<commidCount; i+=NUMB_TEXT_FIELDS) {
		str = "                showing commids from "+i+" to ";
		if ((i+NUMB_TEXT_FIELDS-1) > commidCount) {
		    str += commidCount+"               ";
		} else {
		    str += (i+NUMB_TEXT_FIELDS-1)+"               ";
		}
		rangeChoice.addItem(str);
		rangeChoice.addItemListener(this);
	    }
	    firstCommid = 0;
	    lastCommid = NUMB_TEXT_FIELDS-1;
	}

	for (i=0; i<NUMB_TEXT_FIELDS; i++) {
	    ind = i+firstCommid;
	    if (ind<Global.numbCommIDInfos) {
		commidText[i].setText("CommID 0x"+
	          Long.toHexString(Global.commidInfo[Global.commidIndex[ind]].commid)+
		  ". "+Global.commidInfo[Global.commidIndex[ind]].name);
		commidText[i].setForeground(
		  C.allColors[Global.commidInfo[Global.commidIndex[ind]].colorInd]);
		commidText[i].setBackground(Color.black);
		commidText[i].setVisible(true);
		commidChoice[i].select(Global.commidInfo[
                  Global.commidIndex[ind]].colorInd);
		commidChoice[i].setVisible(true);
	    }
	    else {
		commidText[i].setVisible(false);
		commidChoice[i].setVisible(false);
	    }
	}

	pack();
	validate();
	setVisible(true);
    }

    public void itemStateChanged(ItemEvent event) {
	Object source = event.getSource();
	int i, ind;

	if (source == rangeChoice) {
	    ind = rangeChoice.getSelectedIndex();
	    firstCommid = (ind)*NUMB_TEXT_FIELDS;
	    lastCommid = firstCommid + NUMB_TEXT_FIELDS - 1;
	    setDisplayValues();
	}
        for (i=0; i<NUMB_TEXT_FIELDS; i++) {
	    ind = i+firstCommid;
	    if (source == commidChoice[i]) {
		Global.commidInfo[Global.commidIndex[ind]].colorInd = 
	            commidChoice[i].getSelectedIndex();
		commidText[i].setForeground(C.allColors[
                    Global.commidInfo[Global.commidIndex[ind]].colorInd]);
		commidText[i].setBackground(Color.black);
		commidText[i].setVisible(true);

		Global.traceArea.adjustTraceArray();
		return;
	    }
	}
    }

    public void actionPerformed(ActionEvent event) {
	Object source = event.getSource();
	if (source == closeButton) {
	    setVisible(false);
	    return;
	}
    }

    public boolean display() {
	setDisplayValues();
        return true;
    }
}
