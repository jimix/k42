/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxCreds.C,v 1.3 2004/09/14 21:14:50 butrico Exp $
 *****************************************************************************/
#include <sys/sysIncs.H>
#include "ProcessLinux.H"

/*
 * code cribbed from sys.c in linux with some changes.
 * no longer return old fsgid and old fsuid.
 * return codes now 0 for success, positive errno value otherwise
 */
sval
ProcessLinux::creds_t::sys_setresuid(uid_t new_uid, uid_t new_euid, uid_t new_suid)
{
	uid_t old_uid = uid;
	uid_t old_euid = euid;
	uid_t old_suid = suid;

	if (!capable(CAP_SETUID)) {
		if ((new_uid != (uid_t) -1) && (new_uid != uid) &&
		    (new_uid != euid) && (new_uid != suid))
			return EPERM;
		if ((new_euid != (uid_t) -1) && (new_euid != uid) &&
		    (new_euid != euid) && (new_euid != suid))
			return EPERM;
		if ((new_suid != (uid_t) -1) && (new_suid != uid) &&
		    (new_suid != euid) && (new_suid != suid))
			return EPERM;
	}
	if (new_uid != (uid_t) -1) {
		if (new_uid != uid && set_user(new_uid) < 0)
			return EAGAIN;
	}
	if (new_euid != (uid_t) -1) {
#if 0
		if (new_euid != euid)
			current->dumpable = 0;
#endif
		euid = new_euid;
		fsuid = new_euid;
	}
	if (new_suid != (uid_t) -1)
		suid = new_suid;

#if 0
	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_uid, old_euid, old_suid);
	}
#else
	//avoid unused warnings
	(void)old_uid;(void)old_euid; (void)old_suid;
#endif
	return 0;
}
/*
 * Unprivileged users may change the real uid to the effective uid
 * or vice versa.  (BSD-style)
 *
 * If you set the real uid at all, or set the effective uid to a value not
 * equal to the real uid, then the saved uid is set to the new effective uid.
 *
 * This makes it possible for a setuid program to completely drop its
 * privileges, which is often a useful assertion to make when you are doing
 * a security audit over a program.
 *
 * The general idea is that a program which uses just setreuid() will be
 * 100% compatible with BSD.  A program which uses just setuid() will be
 * 100% compatible with POSIX with saved IDs. 
 */

sval
ProcessLinux::creds_t::sys_setreuid(uid_t arg_uid, uid_t arg_euid)
{
	uid_t old_uid, old_euid, old_suid, new_uid, new_euid;

	new_uid = old_uid = uid;
	new_euid = old_euid = euid;
	old_suid = suid;

	if (arg_uid != (uid_t) -1) {
		new_uid = arg_uid;
		if ((old_uid != arg_uid) &&
		    (euid != arg_uid) &&
		    !capable(CAP_SETUID))
			return EPERM;
	}

	if (arg_euid != (uid_t) -1) {
		new_euid = arg_euid;
		if ((old_uid != arg_euid) &&
		    (euid != arg_euid) &&
		    (suid != arg_euid) &&
		    !capable(CAP_SETUID))
			return EPERM;
	}


	if (new_uid != old_uid && set_user(new_uid) < 0)
		return EAGAIN;
	
	fsuid = euid = new_euid;
	if (arg_uid != (uid_t) -1 ||
	    (arg_euid != (uid_t) -1 && arg_euid != old_uid))
		suid = euid;
	fsuid = euid;
#if 0
	if (euid != old_euid)
		current->dumpable = 0;
	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_uid, old_euid, old_suid);
	}
#else
	//avoid unused warnings
	(void)old_uid;(void)old_euid; (void)old_suid;
#endif

	return 0;
}
		
// we use this to implement exec of setuid programs quickly.
/* virtual */ sval
ProcessLinux::creds_t::insecure_sys_setreuid(uid_t arg_euid)
{
    uid_t old_uid, old_euid, old_suid, new_uid, new_euid;

    new_uid = old_uid = uid;
    new_euid = old_euid = euid;
    old_suid = suid;

    new_euid = arg_euid;
    fsuid = euid = new_euid;
    suid = euid;
    return 0;
}

/* virtual */ sval
ProcessLinux::creds_t::insecure_sys_setregid(gid_t arg_egid)
{
    fsgid = egid = arg_egid;
    sgid = egid;
    return 0;
}

/*
 * setuid() is implemented like SysV with SAVED_IDS 
 * 
 * Note that SAVED_ID's is deficient in that a setuid root program
 * like sendmail, for example, cannot set its uid to be a normal 
 * user and then switch back, because if you're root, setuid() sets
 * the saved uid too.  If you don't like this, blame the bright people
 * in the POSIX committee and/or USG.  Note that the BSD-style setreuid()
 * will allow a root program to temporarily drop privileges and be able to
 * regain them by swapping the real and effective uid.  
 */
sval
ProcessLinux::creds_t::sys_setuid(uid_t arg_uid)
{
	uid_t old_euid = euid;
	uid_t old_uid, old_suid, new_uid;

	old_uid = new_uid = uid;
	old_suid = suid;
	if (capable(CAP_SETUID)) {
		if (arg_uid != old_uid && set_user(arg_uid) < 0)
			return EAGAIN;
		suid = arg_uid;
	} else if ((arg_uid != uid) && (arg_uid != suid))
		return EPERM;

	fsuid = euid = arg_uid;

#if 0
	if (old_euid != arg_uid)
		current->dumpable = 0;
#else
	//avoid unused warnings
	(void)old_euid;
#endif

#if 0
	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		cap_emulate_setxuid(old_uid, old_euid, old_suid);
	}
#endif

	return 0;
}

sval
ProcessLinux::creds_t::set_user(uid_t new_uid)
{
#if 0
	struct user_struct *new_user, *old_user;

	/* What if a process setreuid()'s and this brings the
	 * new uid over his NPROC rlimit?  We can check this now
	 * cheaply with the new uid cache, so if it matters
	 * we should be checking for it.  -DaveM
	 */
	new_user = alloc_uid(new_uid);
	if (!new_user)
		return EAGAIN;
	old_user = current->user;
	atomic_dec(&old_user->processes);
	atomic_inc(&new_user->processes);

	current->uid = new_uid;
	current->user = new_user;
	free_uid(old_user);
	return 0;
#else
	uid = new_uid;
	return 0;
#endif
}

/*
 * Same as above, but for rgid, egid, sgid.
 */
sval
ProcessLinux::creds_t::sys_setresgid(gid_t arg_gid, gid_t arg_egid, gid_t arg_sgid)
{
       if (!capable(CAP_SETGID)) {
		if ((arg_gid != (gid_t) -1) && (arg_gid != gid) &&
		    (arg_gid != egid) && (arg_gid != sgid))
			return EPERM;
		if ((arg_egid != (gid_t) -1) && (arg_egid != gid) &&
		    (arg_egid != egid) && (arg_egid != sgid))
			return EPERM;
		if ((arg_sgid != (gid_t) -1) && (arg_sgid != gid) &&
		    (arg_sgid != egid) && (arg_sgid != sgid))
			return EPERM;
	}
	if (arg_gid != (gid_t) -1)
		gid = arg_gid;
	if (arg_egid != (gid_t) -1) {
#if 0
		if (arg_egid != egid)
		    current->dumpable = 0;
#endif
		egid = arg_egid;
		fsgid = arg_egid;
	}
	if (arg_sgid != (gid_t) -1)
		sgid = arg_sgid;
	return 0;
}

/*
 * "setfsuid()" sets the fsuid - the uid used for filesystem checks. This
 * is used for "access()" and for the NFS daemon (letting nfsd stay at
 * whatever uid it wants to). It normally shadows "euid", except when
 * explicitly set by setfsuid() or for access..
 */
sval
ProcessLinux::creds_t::sys_setfsuid(uid_t uid)
{
	uid_t old_fsuid;

	old_fsuid = fsuid;
	if (uid == uid || uid == euid ||
	    uid == suid || uid == fsuid || 
	    capable(CAP_SETUID))
		fsuid = uid;
#if 0
	if (fsuid != old_fsuid)
	    current->dumpable = 0;
#endif

	/* We emulate fsuid by essentially doing a scaled-down version
	 * of what we did in setresuid and friends. However, we only
	 * operate on the fs-specific bits of the process' effective
	 * capabilities 
	 *
	 * FIXME - is fsuser used for all CAP_FS_MASK capabilities?
	 *          if not, we might be a bit too harsh here.
	 */
	
#if 0
	if (!issecure(SECURE_NO_SETUID_FIXUP)) {
		if (old_fsuid == 0 && fsuid != 0) {
			cap_t(cap_effective) &= ~CAP_FS_MASK;
		}
		if (old_fsuid != 0 && fsuid == 0) {
			cap_t(cap_effective) |=
				(cap_t(cap_permitted) & CAP_FS_MASK);
		}
	}
#endif

	return 0;
}

sval
ProcessLinux::creds_t::sys_setfsgid(gid_t arg_gid)
{
	gid_t old_fsgid;

	old_fsgid = fsgid;
	if (arg_gid == gid || arg_gid == egid ||
	    arg_gid == sgid || arg_gid == fsgid || 
	    capable(CAP_SETGID))
		fsgid = arg_gid;
#if 0
	if (fsgid != old_fsgid)
		current->dumpable = 0;
#endif

	return 0;
}

/*
 * Unprivileged users may change the real gid to the effective gid
 * or vice versa.  (BSD-style)
 *
 * If you set the real gid at all, or set the effective gid to a value not
 * equal to the real gid, then the saved gid is set to the new effective gid.
 *
 * This makes it possible for a setgid program to completely drop its
 * privileges, which is often a useful assertion to make when you are doing
 * a security audit over a program.
 *
 * The general idea is that a program which uses just setregid() will be
 * 100% compatible with BSD.  A program which uses just setgid() will be
 * 100% compatible with POSIX with saved IDs. 
 *
 * SMP: There are not races, the GIDs are checked only by filesystem
 *      operations (as far as semantic preservation is concerned).
 */
sval
ProcessLinux::creds_t::sys_setregid(gid_t arg_gid, gid_t arg_egid)
{
	gid_t old_gid = gid;
	gid_t old_egid = egid;

	if (arg_gid != (gid_t) -1) {
		if ((old_gid == arg_gid) ||
		    (egid==arg_gid) ||
		    capable(CAP_SETGID))
			gid = arg_gid;
		else
			return EPERM;
	}
	if (arg_egid != (gid_t) -1) {
		if ((old_gid == arg_egid) ||
		    (egid == arg_egid) ||
		    (sgid == arg_egid) ||
		    capable(CAP_SETGID))
			fsgid = egid = arg_egid;
		else {
			gid = old_gid;
			return EPERM;
		}
	}
	if (arg_gid != (gid_t) -1 ||
	    (arg_egid != (gid_t) -1 && arg_egid != old_gid))
		sgid = egid;
	fsgid = egid;
#if 0
	if (egid != old_egid)
		current->dumpable = 0;
#else
	(void)old_egid;
#endif
	return 0;
}

/*
 * setgid() is implemented like SysV w/ SAVED_IDS 
 *
 * SMP: Same implicit races as above.
 */
sval
ProcessLinux::creds_t::sys_setgid(gid_t arg_gid)
{
	int old_egid = egid;

	if (capable(CAP_SETGID))
		gid = egid = sgid = fsgid = arg_gid;
	else if ((arg_gid == gid) || (arg_gid == sgid))
		egid = fsgid = arg_gid;
	else
		return EPERM;

#if 0
	if (egid != old_egid)
		current->dumpable = 0;
#else
	(void)old_egid;
#endif
	return 0;
}
