/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShowEvent.java,v 1.1 2003/12/11 15:08:49 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.awt.*;

public class ShowEvent {
    int index;
    Color sColor;

    ShowEvent() {
	index = 0;
	sColor = Color.black;
    }
    ShowEvent(int i, Color c) {
	index = i;
	sColor = c;
    }
}
