/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShowEventCount.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;

public class ShowEventCount {
    int majorID, minorID, count;
    Color sColor;
    String name;

    ShowEventCount() {
	majorID = 0;
	minorID = 0;
	count = 0;
	sColor = Color.black;
	name = new String("");
    }
    ShowEventCount(int maj, int min, int c, Color col, String n) {
	majorID = maj;
	minorID = min;
	count = c;
	sColor = col;
	name = new String(n);
    }
}
