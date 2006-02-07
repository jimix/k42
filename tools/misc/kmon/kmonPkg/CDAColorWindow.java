/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CDAColorWindow.java,v 1.1 2003/12/11 15:08:47 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;

public class CDAColorWindow extends Frame implements ActionListener {
    static final int MAX_TEXT_FIELDS = 20;
    Button okButton;
    GridBagConstraints c;
    GridBagLayout gbl;
    Choice[] cdaChoice = new Choice[MAX_TEXT_FIELDS];
    TextField[] cdaText = new TextField[MAX_TEXT_FIELDS];
    

    public CDAColorWindow() {
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

	    cdaText[i] = new TextField();
	    cdaText[i].setForeground(Color.black);
	    cdaText[i].setText("cda. "+i);
	    gbl.setConstraints(cdaText[i], c);
	    add(cdaText[i]);

	    c.gridx = 1;
	    c.gridy = i;
	    cdaChoice[i] = new Choice();
	    for (j=0;j<C.NUMB_ALL_COLORS; j++) {
		cdaChoice[i].addItem(C.allColorNames[j]);
	    }
	    cdaChoice[i].select(C.COLOR_BLACK);

	    gbl.setConstraints(cdaChoice[i], c);
	    add(cdaChoice[i]);
	}


	c.gridx = 0;
	c.gridy = MAX_TEXT_FIELDS;
	c.gridwidth = GridBagConstraints.REMAINDER;

        okButton = new Button("OK");
	okButton.addActionListener(this);
	gbl.setConstraints(okButton, c);
	add(okButton);

        validate();
    }

    public void actionPerformed(ActionEvent event) {
	Object source = event.getSource();
	if (source == okButton) {
	    setVisible(false);
	}
    }


    public boolean display() {
	int i;

	if (Global.numbCDAInfos < MAX_TEXT_FIELDS) {
	    invalidate();
	    for (i=0; i<Global.numbCDAInfos; i++) {
		cdaText[i].setForeground(C.allColors[Global.cdaInfo[i].colorInd]);
		cdaText[i].setBackground(Color.black);
		cdaText[i].setVisible(true);
		cdaChoice[i].select(Global.cdaInfo[i].colorInd);
		cdaChoice[i].setVisible(true);
	    }
	    for (i=Global.numbCDAInfos; i<MAX_TEXT_FIELDS; i++) {
		cdaText[i].setVisible(false);
		cdaChoice[i].setVisible(false);
	    }
	    pack();
	    validate();
	    setVisible(true);
	} else {
	    System.err.println("help NYI too many CDAs");
	}
        return true;
    }

}
