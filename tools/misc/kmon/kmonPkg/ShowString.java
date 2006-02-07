/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShowString.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;

public class ShowString {
    int index;
    Color sColor;
    String str;

    ShowString() {
	index = 0;
	sColor = Color.black;
	str = new String();
    }
    ShowString(int i, Color c, String st) {
	index = i;
	sColor = c;
	str = new String(st);
    }
}
