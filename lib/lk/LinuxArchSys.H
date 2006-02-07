/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxArchSys.H,v 1.2 2002/04/26 17:07:54 mostrows Exp $
 *****************************************************************************/
// These must be implemented by each envrionment in which lk code runs,
// providing specialized services for theat environment

class LinuxArchSys{
public:
    static void PreInit();  //Called before LK environment exists
    static void Init();     //Called once LK environment exists
    static void ShutDown(); //Called to kill environment
};
