/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShowEventType.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;

public class ShowEventType {
    int majorID, minorID;
    Color sColor;

    ShowEventType() {
	majorID = 0;
	minorID = 0;
	sColor = Color.black;
    }
    ShowEventType(int maj, int min, Color c) {
	majorID = maj;
	minorID = min;
	sColor = c;
    }
}
