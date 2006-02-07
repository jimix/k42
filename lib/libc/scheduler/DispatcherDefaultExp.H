#ifndef __DISPATCHER_DEFAULT_EXP_H_
#define __DISPATCHER_DEFAULT_EXP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultExp.H,v 1.29 2002/07/02 18:12:55 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Static functions that are the C implementations of dispatcher
 *    functionality that is expected to be coded in assembly language.
 * **************************************************************************/

#include __MINC(DispatcherDefaultExp.H)

class DispatcherDefaultExp {
public:
    static void RunEntry(DispatcherDefaultExpRegs *erp);
    static void PPCClient(DispatcherDefaultExpRegs *erp);
    static void IPCCallEntry(DispatcherDefaultExpRegs *erp);
    static void PPCServerOnThread(DispatcherDefaultExpRegs *erp);
    static void IPCReturnEntry(DispatcherDefaultExpRegs *erp);
    static void IPCFaultEntry(DispatcherDefaultExpRegs *erp);
    static void IPCFaultOnThread(DispatcherDefaultExpRegs *erp);
};

#endif /* #ifndef __DISPATCHER_DEFAULT_EXP_H_ */
