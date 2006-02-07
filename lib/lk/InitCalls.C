/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: InitCalls.C,v 1.2 2004/09/11 01:02:16 mostrows Exp $
 *****************************************************************************/
#include "lkIncs.H"
#include "InitCalls.H"
#include "LinuxEnv.H"

struct InitCallHead:public AutoListHead{
    DEFINE_GLOBAL_NEW(InitCallHead);
};
InitCallHead* head = NULL;
SysStatus RegisterInitCall(InitCall *ic) {
    if (!head) {
	head = new InitCallHead();
    }
    head->append(ic);
}

extern unsigned char __pre_init_call1[0];
extern unsigned char __pre_init_call2[0];
extern unsigned char __pre_init_call3[0];
extern unsigned char __pre_init_call4[0];
extern unsigned char __pre_init_call5[0];
extern unsigned char __pre_init_call6[0];
extern unsigned char __pre_init_call7[0];

extern unsigned char __post_init_call1[0];
extern unsigned char __post_init_call2[0];
extern unsigned char __post_init_call3[0];
extern unsigned char __post_init_call4[0];
extern unsigned char __post_init_call5[0];
extern unsigned char __post_init_call6[0];
extern unsigned char __post_init_call7[0];

SysStatus

RunInitCalls() {
    struct bounds_s{
	unsigned char *pre;
	unsigned char *post;
    };
    bounds_s bounds[] = { {__pre_init_call1, __post_init_call1},
			  {__pre_init_call2, __post_init_call2},
			  {__pre_init_call3, __post_init_call3},
			  {__pre_init_call4, __post_init_call4},
			  {__pre_init_call5, __post_init_call5},
			  {__pre_init_call6, __post_init_call6},
			  {__pre_init_call7, __post_init_call7},
			  { NULL, NULL } };
    uval i = 0;
    if (head==NULL) return 0;

    while (bounds[i].pre) {
	int (**initcall)(void) = (typeof(initcall))bounds[i].pre;
	int (**end)(void) = (typeof(initcall))bounds[i].post;
	while (initcall<end) {
	    InitCall *ic = (InitCall*)head->next();
	    while (ic) {
		if (ic->addr == (void*)initcall) {
		    (*initcall)();
		    tassertMsg(preempt_count()==0, "Unclean init call: %p\n",
			       initcall);

		    InitCall *tmp = ic;
		    ic = (InitCall*)ic->next();
		    tmp->detach();
		    delete tmp;
		} else {
		    ic = (InitCall*)ic->next();
		}
	    }
	    ++initcall;
	}
	++i;
    }
}
