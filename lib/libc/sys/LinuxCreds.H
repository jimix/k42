#ifndef __LINUX_CREDS_H_
#define __LINUX_CREDS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxCreds.H,v 1.4 2004/09/14 21:14:51 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: use only inside of ProcessLinux
 * **************************************************************************/
    typedef uval32 kernel_cap_t;
    struct creds_t {
	enum {
	    /**
	    ** POSIX-draft defined capabilities.
	    **/

/* In a system with the [_POSIX_CHOWN_RESTRICTED] option defined, this
   overrides the restriction of changing file ownership and group
   ownership. */

	    CAP_CHOWN=0,

/* Override all DAC access, including ACL execute access if
   [_POSIX_ACL] is defined. Excluding DAC access covered by
   CAP_LINUX_IMMUTABLE. */

	    CAP_DAC_OVERRIDE=1,

/* Overrides all DAC restrictions regarding read and search on files
   and directories, including ACL restrictions if [_POSIX_ACL] is
   defined. Excluding DAC access covered by CAP_LINUX_IMMUTABLE. */

	    CAP_DAC_READ_SEARCH=2,

/* Overrides all restrictions about allowed operations on files, where
   file owner ID must be equal to the user ID, except where CAP_FSETID
   is applicable. It doesn't override MAC and DAC restrictions. */

	    CAP_FOWNER=3,

/* Overrides the following restrictions that the effective user ID
   shall match the file owner ID when setting the S_ISUID and S_ISGID
   bits on that file; that the effective group ID (or one of the
   supplementary group IDs shall match the file owner ID when setting
   the S_ISGID bit on that file; that the S_ISUID and S_ISGID bits are
   cleared on successful return from chown(2). */

	    CAP_FSETID=4,

/* Used to decide between falling back on the old suser() or fsuser(). */

	    CAP_FS_MASK=0x1f,

/* Overrides the restriction that the real or effective user ID of a
   process sending a signal must match the real or effective user ID
   of the process receiving the signal. */

	    CAP_KILL=5,

/* Allows setgid(2) manipulation */
/* Allows setgroups(2) */
/* Allows forged gids on socket credentials passing. */

	    CAP_SETGID=6,

/* Allows set*uid(2) manipulation (including fsuid). */
/* Allows forged pids on socket credentials passing. */

	    CAP_SETUID=7,


/**
** Linux-specific capabilities
**/

/* Transfer any capability in your permitted set to any pid,
   remove any capability in your permitted set from any pid */

	    CAP_SETPCAP=8,

/* Allow modification of S_IMMUTABLE and S_APPEND file attributes */

	    CAP_LINUX_IMMUTABLE=9,

/* Allows binding to TCP/UDP sockets below 1024 */

	    CAP_NET_BIND_SERVICE=10,

/* Allow broadcasting, listen to multicast */

	    CAP_NET_BROADCAST=11,

/* Allow interface configuration */
/* Allow administration of IP firewall, masquerading and accounting */
/* Allow setting debug option on sockets */
/* Allow modification of routing tables */
/* Allow setting arbitrary process / process group ownership on
   sockets */
/* Allow binding to any address for transparent proxying */
/* Allow setting TOS (type of service) */
/* Allow setting promiscuous mode */
/* Allow clearing driver statistics */
/* Allow multicasting */
/* Allow read/write of device-specific registers */

	    CAP_NET_ADMIN=12,

/* Allow use of RAW sockets */
/* Allow use of PACKET sockets */

	    CAP_NET_RAW=13,

/* Allow locking of shared memory segments */
/* Allow mlock and mlockall (which doesn't really have anything to do
   with IPC) */

	    CAP_IPC_LOCK=14,

/* Override IPC ownership checks */

	    CAP_IPC_OWNER=15,

/* Insert and remove kernel modules */

	    CAP_SYS_MODULE=16,

/* Allow ioperm/iopl access */

	    CAP_SYS_RAWIO=17,

/* Allow use of chroot() */

	    CAP_SYS_CHROOT=18,

/* Allow ptrace() of any process */

	    CAP_SYS_PTRACE=19,

/* Allow configuration of process accounting */

	    CAP_SYS_PACCT=20,

/* Allow configuration of the secure attention key */
/* Allow administration of the random device */
/* Allow device administration (mknod)*/
/* Allow examination and configuration of disk quotas */
/* Allow configuring the kernel's syslog (printk behaviour) */
/* Allow setting the domainname */
/* Allow setting the hostname */
/* Allow calling bdflush() */
/* Allow mount() and umount(), setting up new smb connection */
/* Allow some autofs root ioctls */
/* Allow nfsservctl */
/* Allow VM86_REQUEST_IRQ */
/* Allow to read/write pci config on alpha */
/* Allow irix_prctl on mips (setstacksize) */
/* Allow flushing all cache on m68k (sys_cacheflush) */
/* Allow removing semaphores */
/* Used instead of CAP_CHOWN to "chown" IPC message queues, semaphores
   and shared memory */
/* Allow locking/unlocking of shared memory segment */
/* Allow turning swap on/off */
/* Allow forged pids on socket credentials passing */
/* Allow setting readahead and flushing buffers on block devices */
/* Allow setting geometry in floppy driver */
/* Allow turning DMA on/off in xd driver */
/* Allow administration of md devices (mostly the above, but some
   extra ioctls) */
/* Allow tuning the ide driver */
/* Allow access to the nvram device */
/* Allow administration of apm_bios, serial and bttv (TV) device */
/* Allow manufacturer commands in isdn CAPI support driver */
/* Allow reading non-standardized portions of pci configuration space */
/* Allow DDI debug ioctl on sbpcd driver */
/* Allow setting up serial ports */
/* Allow sending raw qic-117 commands */
/* Allow enabling/disabling tagged queuing on SCSI controllers and sending
   arbitrary SCSI commands */
/* Allow setting encryption key on loopback filesystem */

	    CAP_SYS_ADMIN=21,

/* Allow use of reboot() */

	    CAP_SYS_BOOT=22,

/* Allow raising priority and setting priority on other (different
   UID) processes */
/* Allow use of FIFO and round-robin (realtime) scheduling on own
   processes and setting the scheduling algorithm used by another
   process. */

	    CAP_SYS_NICE=23,

/* Override resource limits. Set resource limits. */
/* Override quota limits. */
/* Override reserved space on ext2 filesystem */
/* NOTE: ext2 honors fsuid when checking for resource overrides, so
   you can override using fsuid too */
/* Override size restrictions on IPC message queues */
/* Allow more than 64hz interrupts from the real-time clock */
/* Override max number of consoles on console allocation */
/* Override max number of keymaps */

	    CAP_SYS_RESOURCE=24,

/* Allow manipulation of system clock */
/* Allow irix_stime on mips */
/* Allow setting the real-time clock */

	    CAP_SYS_TIME=25,

/* Allow configuration of tty devices */
/* Allow vhangup() of tty */

	    CAP_SYS_TTY_CONFIG=26};
	uid_t uid,euid,suid,fsuid;
	gid_t gid,egid,sgid,fsgid;
	int ngroups;
	gid_t	groups[_POSIX_NGROUPS_MAX];
	kernel_cap_t   cap_effective, cap_inheritable, cap_permitted;
	int keep_capabilities:1;
	uval capable(uval cap) {
	    return (CAP_FS_MASK&(1<<cap))?fsuid==0:euid==0;}
//FIXME MAA proper capability support needs to be copied in from Linux
//	    return (cap_effective&(1<<cap));}
	sval sys_setresuid(uid_t ruid, uid_t euid, uid_t suid);
	sval sys_setreuid(uid_t arg_uid, uid_t arg_euid);
	sval insecure_sys_setreuid(uid_t arg_euid);
	sval insecure_sys_setregid(gid_t arg_egid);
	sval sys_setuid(uid_t uid);
	sval set_user(uid_t new_ruid);
	sval sys_setresgid(gid_t arg_gid, gid_t arg_egid, gid_t arg_sgid);
	sval sys_setfsuid(uid_t uid);
	sval sys_setfsgid(gid_t gid);
	sval sys_setregid(gid_t arg_gid, gid_t arg_egid);
	sval sys_setgid(gid_t arg_gid);
	void rootPrivs() {
	    uid=euid=suid=fsuid=gid=egid=sgid=fsgid=0;
	    //FIXME MAA set creds
	};
    };
#endif /* #ifndef __LINUX_CREDS_H_ */
