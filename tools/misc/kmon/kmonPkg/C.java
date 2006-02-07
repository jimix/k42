/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: C.java,v 1.5 2004/03/28 15:24:18 bob Exp $
 *****************************************************************************/
package kmonPkg;

import java.awt.*;

public class C {
    public static final long TRACE_TIMESTAMP_SHIFT=32;
    public static final long TRACE_TIMESTAMP_VEC=0xffffffff;
    public static final long TRACE_TIMESTAMP_MASK=0xffffffff00000000L;
    public static final long NOT_TRACE_TIMESTAMP_MASK=0x00000000ffffffffL;

    public static final long TRACE_LENGTH_SHIFT=24;
    public static final long TRACE_LENGTH_VEC=0xff;
    public static final long TRACE_LENGTH_MASK=0x00000000ff000000L;

    public static final long TRACE_LAYER_ID_SHIFT=20;
    public static final long TRACE_LAYER_ID_VEC=0xf;
    public static final long TRACE_LAYER_ID_MASK=0x0000000000f00000L;

    public static final long TRACE_MAJOR_ID_SHIFT=14;
    public static final long TRACE_MAJOR_ID_VEC=0x3f;
    public static final long TRACE_MAJOR_ID_MASK=0x00000000000fc000L;

    public static final long TRACE_DATA_SHIFT=0;
    public static final long TRACE_DATA_VEC=0xffff;
    public static final long TRACE_DATA_MASK=0x0000000000003fffL;

    public static final int MAX_TRACE_EVENTS=2000000;
    public static final int MAX_CDA_INFOS=100;
    public static final int MAX_COMMID_INFOS=2000;

    public static final int MAX_PID_INFOS=2000;

    public static final int MAX_TA_ENTRIES=200;

    public static final int TRACE_ENTRY_SIZE=8;

    // FIXME - good only for power pc
    public static final long COMMID_PID_MASK=0xffffffff00000000L;
    public static final long COMMID_PID_SHIFT=32;
    public static final long COMMID_RD_MASK=0x00000000ffff0000L;
    public static final long COMMID_RD_SHIFT=16;
    // FIXME - good only for power pc

    public static final int CHOOSE_START = 0;
    public static final int CHOOSE_END = 1;
    public static final int CHOOSE_INTERMEDIATE = 2;
    public static final int CHOOSE_SHOW_EVENT = 3;
    public static final int CHOOSE_HIDE_EVENT = 4;

    public static final int SHOW_NONE = 0;
    public static final int SHOW_CDAS = 1;
    public static final int SHOW_COMMIDS = 2;
    public static final int SHOW_PIDS = 3;

    public static final int COLOR_BLACK = 0;
    public static final int COLOR_BLUE = 1;
    public static final int COLOR_CYAN = 2;
    public static final int COLOR_DARKGRAY = 3;
    public static final int COLOR_GRAY = 4;
    public static final int COLOR_GREEN = 5;
    public static final int COLOR_LIGHTGRAY = 6;
    public static final int COLOR_MAGENTA = 7;
    public static final int COLOR_ORANGE = 8;
    public static final int COLOR_PINK = 9;
    public static final int COLOR_RED = 10;
    public static final int COLOR_WHITE = 11;
    public static final int COLOR_YELLOW = 12;
    public static final int NUMB_ALL_COLORS = 13;

    public static final int VERSION = 3;
    

    public static final int[] procColorInd = {COLOR_CYAN, COLOR_GREEN,
				       COLOR_MAGENTA, COLOR_ORANGE, COLOR_PINK,
                                       COLOR_LIGHTGRAY, 
				       COLOR_YELLOW};    
    public static final int numbProcColorInds = 7;

    public static final Color[] allColors = {Color.black, Color.blue, Color.cyan,
				      Color.darkGray, Color.gray, Color.green,
				      Color.lightGray, Color.magenta, Color.orange,
				      Color.pink, Color.red, Color.white,
				      Color.yellow};
    public static final String[] allColorNames = {"black", "blue", "cyan",
					   "darkGray", "gray", "green",
					   "lightGray", "magenta", "orange",
					   "pink", "red", "white",
					   "yellow"};
    public static final int NUMB_ARGS_STORED=4;

    public static final int ZOOM_NORMAL = 0;
    public static final int ZOOM_EVENT = 1;
}
