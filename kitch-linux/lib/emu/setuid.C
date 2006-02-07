/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: setuid.C,v 1.15 2005/08/17 18:39:33 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include "linuxEmul.H"
#include <sys/ProcessLinux.H>
#include <sys/ResMgrWrapper.H>


/* FIXME:
 * These need to be split out.
 */
extern "C" {
extern int __k42_linux_getresuid (uid_t *ruid, uid_t *euid, uid_t *suid);
extern int __k42_linux_getresgid (gid_t *gid, gid_t *egid, gid_t *sgid);
extern int __k42_linux_setfsuid (uid_t fsuid);
extern int __k42_linux_setgid (gid_t gid);
extern int __k42_linux_setregid (gid_t rgid, gid_t egid);
extern int __k42_linux_setresgid (gid_t rgid, gid_t egid, gid_t sgid);
extern int __k42_linux_setresuid (uid_t ruid, uid_t euid, uid_t suid);
extern int __k42_linux_setreuid (uid_t ruid, uid_t euid);
extern int __k42_linux_setuid (uid_t uid);
extern uid_t __k42_linux_geteuid (void);
extern gid_t __k42_linux_getegid (void);
extern uid_t __k42_linux_getuid (void);
extern gid_t __k42_linux_getgid (void);
extern int __k42_linux_setgroups (void);
extern int __k42_linux_getgroups (int size, const gid_t *list);
extern int __k42_linux_setfsgid (gid_t fsgid);
}

static sval set_helper(
	ProcessLinux::set_uids_gids_type type,
	uid_t uid, uid_t euid, uid_t suid, uid_t fsuid,
	gid_t gid, gid_t egid, gid_t sgid, gid_t fsgid)
{
    SysStatus rc, resMgrRC;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->set_uids_gids(
	type, uid, euid, suid, fsuid, gid, egid, sgid, fsgid);
    if (((type == ProcessLinux::SETREUID) ||
	    (type == ProcessLinux::SETRESUID)) &&
	(uid != uid_t(-1)) &&
       _SUCCESS(rc))
    {
	/*
	 * FIXME:  ProcessLinuxServer should contact the resource manager,
	 *         or vice versa.  We shouldn't rely on the user to contact
	 *         the resource manager.
	 */
	resMgrRC = DREFGOBJ(TheResourceManagerRef)->assignDomain(uid);
	passertWrn(_SUCCESS(resMgrRC), "ResMgr assignDomain failed.\n");
    }
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    return rc;
}

int
__k42_linux_getresgid (gid_t *gid, gid_t *egid, gid_t *sgid)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    *gid = linuxInfo.creds.gid;
    *egid = linuxInfo.creds.egid;
    *sgid = linuxInfo.creds.sgid;
    return rc;
}


int
__k42_linux_getresuid (uid_t *uid, uid_t *euid, uid_t *suid)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    *uid = linuxInfo.creds.uid;
    *euid = linuxInfo.creds.euid;
    *suid = linuxInfo.creds.suid;
    return rc;
}


uid_t
__k42_linux_getuid (void)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return uid_t(-_SGENCD(rc));
    }
    return linuxInfo.creds.uid;
}


int
__k42_linux_setfsuid (uid_t fsuid)
{
    /*
     * unlike the linux version, we return -1 if it fails
     * and set errno
     * FIXME MAA - crazy return values not implemented
     */
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETFSUID,
	0, 0, 0, fsuid, 0, 0, 0, 0);
    return (retvalue);
}


int
__k42_linux_setgid (gid_t gid)
{
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETGID,
	0, 0, 0, 0, gid, 0, 0, 0);
    return (retvalue);
}


int
__k42_linux_setgroups (void)
{
    #define VERBOSE_SETGROUPS
    #ifdef VERBOSE_SETGROUPS
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
    	tassertWrn(0, "%s not implemented\n", __func__);
    }
    #endif // VERBOSE_SETGROUPS

    return 0;
}


int
__k42_linux_setregid (gid_t gid, gid_t egid)
{
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETREGID,
	0, 0, 0, 0, gid, egid, 0, 0);
    return (retvalue);
}


int
__k42_linux_setresgid (gid_t gid, gid_t egid, gid_t sgid)
{
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETRESGID,
	0, 0, 0, 0, gid, egid, sgid, 0);
    return (retvalue);
}


int
__k42_linux_setresuid (uid_t uid, uid_t euid, uid_t suid)
{
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETRESUID,
	uid, euid, suid, 0, 0, 0, 0, 0);
    return (retvalue);
}


int
__k42_linux_setreuid (uid_t uid, uid_t euid)
{
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETREUID,
	uid, euid, 0, 0, 0, 0, 0, 0);
    return (retvalue);
}


int
__k42_linux_setuid (uid_t uid)
{
err_printf("%s uid=%d\n", __func__, uid);
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETUID,
	uid, 0, 0, 0, 0, 0, 0, 0);
    return (retvalue);
}


uid_t
__k42_linux_geteuid (void)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return uid_t(-_SGENCD(rc));
    }
    return linuxInfo.creds.euid;
}


gid_t
__k42_linux_getegid (void)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return uid_t(-_SGENCD(rc));
    }
    return linuxInfo.creds.egid;
}


gid_t
__k42_linux_getgid (void)
{
    SysStatus rc;
    ProcessLinux::LinuxInfo linuxInfo;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(0, linuxInfo);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return gid_t(-_SGENCD(rc));
    }
    return linuxInfo.creds.gid;
}


#if 0
int

__k42_linux_getgroups (int size, const gid_t *list)
{
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, 0));
}
#endif


int
__k42_linux_setfsgid (gid_t fsgid)
{
    /*
     * On error returns the current value of fsgid
     * FIXME MAA crazy return code not implemented
     */
    int retvalue;

    retvalue = set_helper(
	ProcessLinux::SETFSGID,
	0, 0, 0, 0, 0, 0, 0, fsgid);
    return (retvalue);
}


