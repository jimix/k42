#ifndef __LOGIN_H_
#define __LOGIN_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Login.H,v 1.16 2004/06/28 17:02:02 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Login Daemon
 ****************************************************************************/

class Login : private Obj {
    DEFINE_GLOBALPADDED_NEW(Login);
public:

    Login() {;}
    ~Login();

    static void ClassInit();
    static void DoRlogind(uval arg);
    static void DoRlogindConsole(uval arg);
    static  SysStatus __doLogin(FileLinuxRef fl,
				const char* uname, const char* term);
    static void ConsoleProcess(char* argv[], char* envp[]);
protected:
    static int Child(const char* ptyName, const char* lname,
		      const char* term, char **argv);
__xprotected:
    static SysStatusProcessID _GetLoginPID();
__xpublic:
    static __async SysStatus _StartConsoleLogin();
};


#endif /* #ifndef __LOGIN_H_ */
