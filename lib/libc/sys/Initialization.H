#ifndef __INITIALIZATION_H_
#define __INITIALIZATION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Initialization.H,v 1.7 2004/11/15 04:21:22 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Definition of sequence of services to be started.
 * **************************************************************************/

enum {
    INIT_KICKOFF,		/* 0  starter for init sequence */
    INIT_BASESERVERS_START,	/* 1  start baseServers process */
    INIT_MNTPTMGR,		/* 2  start MountPointMgr */
    INIT_DEVFS,			/* 3  start devfs, mount /dev */
    INIT_LINUXINIT,		/* 4  start deferred linux kernel modules */
    INIT_PIPE,			/* 5  start pipe services */
    INIT_SOCKET,		/* 6  start socket services */
    INIT_IPSOCK,		/* 7  start TCP/IP services */
    INIT_TOYBLOCKDEV,		/* 8  start toy disks */
    INIT_PTY,			/* 9  start pty/tty services */
    INIT_SYSVSEM,		/* 10 start SysV semaphores */
    INIT_SYSVMEM,		/* 11 start SysV shared memory */
    INIT_SYSVMSG,               /* 12 start SysV messages */
    INIT_FSSERVICE,		/* 13 start file system services */
    INIT_KFS,			/* 14 make KFS mounts */
    INIT_NFS,			/* 15 make additional NFS mounts */
    INIT_EXT2,			/* 16 make additional NFS mounts */
    INIT_SWAPFILE,		/* 17 create and register swap file */
    INIT_SCRIPT,		/* 18 run sysinit initialization script */
    INIT_RLOGIN,		/* 19 start rlogin */

    /* initr reaper goes into an infinite loop, so have to put new entries
       before it */
    INIT_REAPER,		/* 19 begin reaping processes in init */
};


#endif /* __INITIALIZATION_H_*/
