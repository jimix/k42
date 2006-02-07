/*
 * (C) Copyright IBM Corp. 2004
 */
/* $Id: testJava.java,v 1.5 2005/05/06 20:15:13 cascaval Exp $
 
/**
 *  Java test for the standalone PEM API tracing
 *  test program
 *
 * @Author: CC
 * @Date: 01/13/2005
 */

// package com.ibm.PERCS.PPEM.traceFormat;

import com.ibm.PEM.traceFormat.*;
import com.ibm.PEM.Events.*;
import java.io.*;

public class testJava
{

    public testJava() {}

    public static void main(String args[]) {

	if(args.length < 1) {
	    System.out.println("Usage: java TraceTest <output_file>");
	    System.exit(1);
	}

	try {
	    PEMOutputStream ostrm = 
		new PEMOutputStream(new FileOutputStream(new File(args[0])), 
				    100, 0);

	    long ll1 = Long.parseLong("123456789", 16);
	    long ll2 = Long.parseLong("2468ace0f", 16);
	    byte c1 = (byte)255;
	    byte c2 = 1;
	    short s = 0x3579;
	    int w = 0x56655665;
	    long list[] = new long[8];
	    list[0] = Long.parseLong("111222233", 16);
	    list[1] = Long.parseLong("2233344444", 16);
	    list[2] = Long.parseLong("344555666", 16);
	    list[3] = Long.parseLong("4455566667", 16);
	    list[4] = Long.parseLong("555666677", 16);
	    list[5] = Long.parseLong("6667777788", 16); 
	    list[6] = Long.parseLong("778888999",  16);
	    list[7] = Long.parseLong("8888999999", 16);

	    String testStrs[] = new String[5];
	    testStrs[0] = new String("five");
	    testStrs[1] = new String("little");
	    testStrs[2] = new String("strings");
	    testStrs[3] = new String("to");
	    testStrs[4] = new String("test");

	    MON.Test_Test0_Event e1 = new MON.Test_Test0_Event(1);
	    e1.write(ostrm);
	    MON.Test_Test1_Event e2 = new MON.Test_Test1_Event(2, ll1);
	    e2.write(ostrm);
	    MON.Test_Test2_Event e3 = new MON.Test_Test2_Event(3, ll1,ll2,ll2);
	    e3.write(ostrm);
	    MON.Test_Pack_Event e4 = new MON.Test_Pack_Event(4, c1, c2, s, w, ll2);
	    e4.write(ostrm);
	    MON.Test_String_Event e5= new MON.Test_String_Event(5, "bubu");
	    e5.write(ostrm);
	    MON.Test_StrData_Event e6 = new MON.Test_StrData_Event(6, ll1, "bubu1", "bubu2"); 
	    e6.write(ostrm);
	    MON.Test_List_Event e7 = new MON.Test_List_Event(7, w, 8, list);
	    e7.write(ostrm);
 	    MON.Test_ListOfStrings_Event e8 = 
		new MON.Test_ListOfStrings_Event(8, 5, testStrs);
 	    e8.write(ostrm);
	    
	    ostrm.close();
	} catch (Exception ex) {
	    ex.printStackTrace();
	    System.exit(1);
	}
	System.exit(0);
    }
}
