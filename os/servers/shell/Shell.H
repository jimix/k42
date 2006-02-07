#ifndef __SHELL_H_
#define __SHELL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Shell.H,v 1.15 2002/10/10 13:09:27 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/types.H>

// FIXME make dynamic
#define INPUT_CMD_BUF_SIZE 512
#define MAX_HISTORY_LEN 5

// FIXME make dynamic
#define MAX_ARGS 8
#define MAX_ARG_LEN 32

class Shell {
    // FIXME make dynamic
    char prompt[256];

    void setPrompt();
    SysStatus doBuiltInCmds(uval, char *[]);
    SysStatus doExecutable(uval, char *[]);
    SysStatus processCmdLine(char *);

    void executeBuiltin(uval, uval, char *[]);

public:
    // FIXME properly done with exit
    uval runningShell;
    char hist[MAX_HISTORY_LEN][INPUT_CMD_BUF_SIZE];
    uval cmdCount;

    Shell();
    void shell(uval argc, const char *argv[]);

    void cd(char *);
    void rm(char *);
    void regress(char *);
    void help(char *);
    void man(char *);
};

#endif /* #ifndef __SHELL_H_ */
