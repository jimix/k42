/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MsgDialog.java,v 1.1 2003/12/11 15:08:48 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;
import java.awt.event.*;


public class MsgDialog extends Dialog implements ActionListener {
    Button okButton;
    TextArea ta = new TextArea(25, 88);
    String hText;
  
    public MsgDialog(Frame dw, String title) {
      super(dw, title, false);
      setLayout(new BorderLayout());
      
      ta.setEditable(false);
      
      add("Center", ta);
      
      okButton = new Button("OK");
      okButton.addActionListener(this);
      add("South", okButton);
      
      pack();
      validate();
    }

    public void actionPerformed(ActionEvent event) {
	Object source = event.getSource();
	if (source == okButton) {
	    setVisible(false);
	}
    }

    public void display (String text) {
      ta.setText(text);
      show();
    }

}
