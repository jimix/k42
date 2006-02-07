/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ZoomWindow.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;

public class ZoomWindow extends Frame implements ActionListener {
    Button okButton;
    Button cancelButton;

    TextField startData, endData;
    Label startLabel, endLabel, instrLabel;

    GridBagConstraints c;
    GridBagLayout gbl;

    public ZoomWindow(Frame dw, String title) {
	setSize(400,200);

	instrLabel = new Label("start time");

	startLabel = new Label("start time");
	endLabel = new Label("end time");
	instrLabel = new Label("enter zoom time");

	startData = new TextField();
	endData = new TextField();

	startData.setSize(200,30);
	endData.setSize(200,30);

	okButton = new Button("OK");
	okButton.addActionListener(this);

	cancelButton = new Button("Cancel");
	cancelButton.addActionListener(this);
	
        setLayout(new GridBagLayout());
	gbl = (GridBagLayout)getLayout();
	c = new GridBagConstraints();

	c.fill = GridBagConstraints.BOTH;
	c.gridheight = 1;
	c.gridwidth = 1;
	c.weightx = 1;

	c.gridx = 0;
	c.gridy = 0;
	gbl.setConstraints(instrLabel, c);
	add(instrLabel);

	c.gridx = 0;
	c.gridy = 1;
	gbl.setConstraints(startLabel, c);
	add(startLabel);

	c.gridx = 1;
	c.gridy = 1;
	gbl.setConstraints(endLabel, c);
	add(endLabel);

	c.gridx = 0;
	c.gridy = 2;
	gbl.setConstraints(startData, c);
	add(startData);

	c.gridx = 1;
	c.gridy = 2;
	gbl.setConstraints(endData, c);
	add(endData);

	c.gridx = 0;
	c.gridy = 3;
	gbl.setConstraints(okButton, c);
	add(okButton);

	c.gridx = 1;
	c.gridy = 3;
	gbl.setConstraints(cancelButton, c);
	add(cancelButton);

	validate();
    }

    public void actionPerformed(ActionEvent event) {
	Object source = event.getSource();
	if (source == okButton) {
	    setVisible(false);
	    Global.traceArea.zoom(24);
	}
	if (source == cancelButton) {
	    setVisible(false);
	}
    }

    public void display () {
      show();
    }
}
