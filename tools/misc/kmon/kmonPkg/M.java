/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: M.java,v 1.2 2004/03/11 18:06:28 bob Exp $
 *****************************************************************************/

package kmonPkg;

import kmonPkg.C;

public class M {
    public static long PID_FROM_COMMID(long commID) {
	return (((commID) & C.COMMID_PID_MASK) >> C.COMMID_PID_SHIFT);
    }

    public static long RD_FROM_COMMID(long commID) {
	return (((commID) & C.COMMID_RD_MASK) >> C.COMMID_RD_SHIFT);
    }

    public static long TRACE_TIMESTAMP_GET(long x) {
	return ((((x)&C.TRACE_TIMESTAMP_MASK)>>C.TRACE_TIMESTAMP_SHIFT)&
		C.NOT_TRACE_TIMESTAMP_MASK);
    }

    public static long TRACE_LENGTH_GET(long x){
	return (((x)&C.TRACE_LENGTH_MASK)>>C.TRACE_LENGTH_SHIFT);
    }

    public static long TRACE_LAYER_ID_GET(long x) {
	return (((x)&C.TRACE_LAYER_ID_MASK)>>C.TRACE_LAYER_ID_SHIFT);
    }

    public static long TRACE_MAJOR_ID_GET(long x) {
	return (((x)&C.TRACE_MAJOR_ID_MASK)>>C.TRACE_MAJOR_ID_SHIFT);
    }

    public static long TRACE_DATA_GET(long x) {
	return (((x)&C.TRACE_DATA_MASK)>>C.TRACE_DATA_SHIFT);
    }
}

