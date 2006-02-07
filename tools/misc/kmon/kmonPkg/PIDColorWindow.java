/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PIDColorWindow.java,v 1.1 2003/12/11 15:08:48 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;

public class PIDColorWindow extends Frame implements ActionListener, ItemListener{
    static final int MAX_TEXT_FIELDS = 20;
    Button closeButton;
    GridBagConstraints c;
    GridBagLayout gbl;
    Choice[] pidChoice = new Choice[MAX_TEXT_FIELDS];
    TextField[] pidText = new TextField[MAX_TEXT_FIELDS];
    

    public PIDColorWindow() {
	int i,j;
        setLayout(new BorderLayout());
	setSize(200,800);

        setLayout(new GridBagLayout());
	gbl = (GridBagLayout)getLayout();
	c = new GridBagConstraints();

	c.fill = GridBagConstraints.HORIZONTAL;
	c.weightx = 1;
	c.weighty = 1;
	c.gridheight = 1;

	for (i=0;i<MAX_TEXT_FIELDS;i++) {
	    c.gridx = 0;
	    c.gridy = i;

	    pidText[i] = new TextField();
	    pidText[i].setForeground(Color.black);
	    if (i<Global.numbPIDInfos) {
		pidText[i].setText("pid "+i+". "+Global.pidInfo[i].name);
	    } else {
		pidText[i].setText("pid "+i);
	    }
	    gbl.setConstraints(pidText[i], c);
	    add(pidText[i]);

	    c.gridx = 1;
	    c.gridy = i;
	    pidChoice[i] = new Choice();
	    for (j=0;j<C.NUMB_ALL_COLORS; j++) {
		pidChoice[i].addItem(C.allColorNames[j]);
		pidChoice[i].addItemListener(this);
	    }
	    pidChoice[i].select(C.COLOR_BLACK);

	    gbl.setConstraints(pidChoice[i], c);
	    add(pidChoice[i]);
	}


	c.gridx = 0;
	c.gridy = MAX_TEXT_FIELDS;
	c.gridwidth = GridBagConstraints.REMAINDER;

        closeButton = new Button("close");
	closeButton.addActionListener(this);
	gbl.setConstraints(closeButton, c);
	add(closeButton);

        validate();
    }

    public void itemStateChanged(ItemEvent event) {
	Object source = event.getSource();
	int i;

        for (i=0; i<MAX_TEXT_FIELDS; i++) {
	    if (source == pidChoice[i]) {
		Global.pidInfo[i].colorInd = pidChoice[i].getSelectedIndex();
		pidText[i].setForeground(C.allColors[Global.pidInfo[i].colorInd]);
		pidText[i].setBackground(Color.black);
		pidText[i].setVisible(true);

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
	int i;
	if (Global.numbPIDInfos < MAX_TEXT_FIELDS) {
	    invalidate();
	    for (i=0; i<Global.numbPIDInfos; i++) {
		pidText[i].setText("CommID 0x"+
				   Long.toHexString(Global.pidInfo[i].pid)+
				   ". "+Global.pidInfo[i].name);
		pidText[i].setForeground(
				   C.allColors[Global.pidInfo[i].colorInd]);
		pidText[i].setBackground(Color.black);
		pidText[i].setVisible(true);
		pidChoice[i].select(Global.pidInfo[i].colorInd);
		pidChoice[i].setVisible(true);
	    }
	    for (i=Global.numbPIDInfos; i<MAX_TEXT_FIELDS; i++) {
		pidText[i].setVisible(false);
		pidChoice[i].setVisible(false);
	    }
	    pack();
	    validate();
	    setVisible(true);
	} else {
	    System.err.println("help NYI too many PIDs");
	}
        return true;
    }

}
