#ifndef __UTILS_H_
#define __UTILS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Utils.H,v 1.1 2004/02/27 17:14:23 mostrows Exp $
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

extern int Linux_OnSim();
extern void Linux_GetThinEnvVar(const char *envVarName, char *envVarValue);
extern int Linux_InetAddr(const char *addr);
extern int printk(const char *fmt, ...);
extern int pow2(int x);
#ifdef __cplusplus
} // extern "C"
#endif /* #ifdef __cplusplus */
#endif /* #ifndef __UTILS_H_ */
