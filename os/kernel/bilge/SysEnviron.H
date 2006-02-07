#ifndef __SYS_ENVIRON_H
#define __SYS_ENVIRON_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/


class SysEnviron {
protected:
    static uval OnMambo;
    static uval OnHardware;
    static uval OnHypervisor;
    static uval Thinwire;

public:
    static void ClassInit(VPNum vp);
    static void InitThinwire(VPNum vp);
    static SysStatus GetTimeOfDay(struct timeval &tv);
    static uval SuspendThinWireDaemon();
    static uval RestartThinWireDaemon();
};
#endif /* #ifndef __SYS_ENVIRON_H */
