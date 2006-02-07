/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocalAsm.C,v 1.1 2002/08/15 20:13:41 rosnbrg Exp $
 *************************************************************************** */

#include <sys/sysIncs.H>
#include <exception/ExceptionLocal.H>

void ExceptionLocal_AcceptRemoteIPC(IPCRegsArch *ipcRegsP,
				    CommID callerID,
				    uval ipcType,
				    ProcessAnnex *curProc)
{
    /* empty body */
}
