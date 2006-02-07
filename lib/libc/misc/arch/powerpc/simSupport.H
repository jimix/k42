#ifndef __SIM_SUPPORT_H_
#define __SIM_SUPPORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: simSupport.H,v 1.17 2004/09/29 08:32:53 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: low-level interface to simos
 * **************************************************************************/


//FIXME all these constant should come from simos

#define SIMOS_BREAKPOINT   asm(".long 0x7C0007CE")
#define REAL_MODE_SIMOS_CALL 0
#define VIRT_MODE_SIMOS_CALL 1

#define SIMOS_CONS_FUN		   0	/* Console output		*/

// SimOS functions
#define SimDeviceIntRegisterCode 3     /*  3 */
#define SimDeviceIntDeregisterCode 4   /*  4 */
#define SimDeviceEnableIntCode 5       /*  5 */
#define SimDeviceDisableIntCode 6      /*  6 */
#define SimGetInterruptingLevelCode 11 /* 11 */
#define SimSetExternalLevelMaskCode 12 /* 12 */
#define SimSetInterruptCode 13         /* 13 */
#define SimMapLevelCode 14             /* 14 */
#define SimResetInterruptCode 15       /* 15 */
#define SimInterruptEOICode 21         /* 21 */
#define SimBlockIOCode 23              /* 23 */
#define SimGetCookieCode 24            /* 24 */
#define SimExitCode 31                 /* 31 */
#define SimReadCharStdinK 60	       /* 60 */ /* k42 codes in 60s */
#define SimThinIPReadK 61	       /* 61 */ /* a k42 thinIP read */
#define SimThinIPWriteK 62	       /* 62 */ /* a k42 thinIP write */
#define SimStartCPUK 64	               /* 64 */ /* start another cpu */
#define SimSendIPIK 65	               /* 65 */ /* send IPI */
#define SimGetInstrCountK 66           /* 66 */ /* k42 get instr count */
#define SimGetNumbPhysProcsK 67        /* 67 */ /* k42 get instr count */
#define SimGetMachAttrK 68             /* 68 */ /* get a machine attribute */
#define SimPhysMemCopyK 69             /* 69 */ /* a fast phys-phys mem copy */
#define SimGetTimeK 70                 /* 70 */ /* get a time */
#define SimPhysMemSetK 71              /* 71 */ /* a fast set memory */
#define SimTraceFileCtlK 72            /* 72 */ /* allows open, write, close */
#define SimDiskReadK42K 80	       /* 80 */ /* read sectors from simulated disk */
#define SimDiskWriteK42K 81	       /* 81 */ /* write sectors to simulated disk */
#define SimDiskOpenK42K 82             /* 82 */ /* open disk if available */
#define SimDiskCloseK42K 83            /* 83 */ /* close disk */

#define SimNumbPhysCpusK 0
#define SimMemorySizeK 1

// define file control codes
#define SimTraceFileCtlOpenK 1
#define SimTraceFileCtlWriteK 2
#define SimTraceFileCtlCloseK 3

// SimOS device types
#define SimConsoleDeviceID 0
#define SimEtherDeviceID 2

// SimOS constants
#define ETHER_CMND_TX_FRAME 1
#define ETHER_CMND_RX_FRAME 2
#define ETHER_CMND_GET_MAC  6

// MAMBO constants
//FIXME all these constant should come from mambo
#define SimCallTCLCode         86
#define SimBogusDiskReadCode   116
#define SimBogusDiskWriteCode  117
#define SimBogusDiskInfoCode   118
#define BD_SECT_SZ 512
#define BD_MAX_BUF 4096
#define BD_INFO_SYNC   0
#define BD_INFO_STATUS 1
#define BD_INFO_BLKSZ  2
#define BD_INFO_DEVSZ  3
#define BD_INFO_CHANGE 4

#ifdef __cplusplus
extern "C"
#endif /* #ifdef __cplusplus */
sval SimOSSupport(unsigned int foo, ...);

#ifdef __cplusplus
extern "C"
#endif /* #ifdef __cplusplus */
sval MamboSupport(unsigned int foo, ...);

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */
    
    int MamboCallthru2(int command, unsigned long arg1, unsigned long arg2);
    int MamboCallthru3(int command, unsigned long arg1, unsigned long arg2,
		       unsigned long arg3);
    
    int MamboBogusDiskRead( int devno, void *buf, unsigned long sect,
			    unsigned long nrsect);

    int MamboBogusDiskWrite(int devno, void *buf, unsigned long sect,
			    unsigned long nrsect);
    
    int MamboBogusDiskInfo( int op, int devno);

    int MamboRunTCLCommand(char *tclCmd, uval tclCmdLen);

    int MamboGetEnv(const char *varName, int varNameLen, char *varValue);

    int MamboGetKParmsSize(unsigned int *blockSize);
    int MamboGetKParms(void *dataBlock);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */


#endif /* #ifndef __SIM_SUPPORT_H_ */
