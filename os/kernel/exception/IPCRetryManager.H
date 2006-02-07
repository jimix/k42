#ifndef __IPC_RETRY_MANAGER_H_
#define __IPC_RETRY_MANAGER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IPCRetryManager.H,v 1.3 2002/08/22 14:48:18 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines IPC retry mechanism.
 * **************************************************************************/

/*
 * Locking: only accessed at exception level - caller must
 * disable.
 */
class IPCRetryManager {
    ProcessAnnex *headSource[Dispatcher::NUM_IPC_RETRY_IDS];
    uval64 remoteIPCRetrySources;
    inline uval getRetryID(ProcessAnnex *pa);
    void internal_notify(uval64 sources, uval64 targets);

    /*
     * We arbitrarily choose the last retryID as the one to use for all
     * remote IPC retry notifications.
     */
    static const uval REMOTE_IPC_RETRY_ID = Dispatcher::NUM_IPC_RETRY_IDS - 1;
public:
    void init();
    void requestNotification(ProcessAnnex *source, ProcessAnnex *target);
    void notify(ProcessAnnex *target);
    void requestNotificationRemote(ProcessAnnex *source, CommID targetID);
    void notifyRemote();
    void remove(ProcessAnnex *pa);

    static uval64 GetIPCRetryIDs(ProcessAnnex *pa);

    void checkRemoteNotifications() {
	if (remoteIPCRetrySources != 0) {
	    notifyRemote();
	}
    }
};

#endif /* #ifndef __IPC_RETRY_MANAGER_H_ */
