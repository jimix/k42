/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TraceFilter.java,v 1.2 2003/12/19 19:37:47 bob Exp $
 *****************************************************************************/

package kmonPkg;

import java.io.File;
import java.io.FilenameFilter;

public class TraceFilter implements FilenameFilter {
    public boolean accept(File dir, String name) {
	return (name.endsWith(".trc"));
    }
}
