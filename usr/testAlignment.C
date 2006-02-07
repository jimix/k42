/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testAlignment.C,v 1.5 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test mis-aligned memory accesses
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherDefault.H>
#include <sys/systemAccess.H>
#include <stdio.h>
#include <stdlib.h>

void
initUval64(uval addr, uval64 value)
{
    for (uval i = 0; i < 8; i++) ((uval8 *)addr)[i] = ((uval8 *)(&value))[i];
}

void
verifyUval64(uval addr, uval64 value)
{
    uval64 tmp;
    for (uval i = 0; i < 8; i++) ((uval8 *)(&tmp))[i] = ((uval8 *)addr)[i];
    if (tmp != value) {
	printf("(0x%lx) is 0x%llx, should be 0x%llx.\n", addr, tmp, value);
    }
}

void
verifyDouble(uval addr, double value)
{
    double tmp;
    for (uval i = 0; i < 8; i++) ((uval8 *)(&tmp))[i] = ((uval8 *)addr)[i];
    if (tmp != value) {
	printf("(0x%lx) is %f, should be %f.\n", addr, tmp, value);
    }
}

void
verifyGPR(uval reg, uval64 value, uval64 expected)
{
    if (value != expected) {
	printf("(r%ld) is 0x%llx, should be 0x%llx.\n", reg, value, expected);
    }
}

void
verifyFPR(uval reg, double value, double expected)
{
    if (value != expected) {
	printf("(f%ld) is %f, should be %f.\n", reg, value, expected);
    }
}

#define TEST_SRC_GPR(reg, offset, base, value, tmp) \
    asm volatile ("mr %0," #reg "\n" \
		  "mr " #reg ",%1\n" \
		  "std " #reg ",%2(%3)\n" \
		  "mr " #reg ",%0" \
		  : "=&r"(tmp) \
		  : "r"(value), "i"(offset), "b"(base) \
		  : "r" #reg); \
    verifyUval64(base+offset, value)

#define TEST_DST_GPR(reg, offset, base, value, tmp, result) \
    asm volatile ("mr %0," #reg "\n" \
		  "ld " #reg ",%2(%3)\n" \
		  "mr %1," #reg "\n" \
		  "mr " #reg ",%0" \
		  : "=&r"(tmp), "=&r"(result) \
		  : "i"(offset), "b"(base) \
		  : "r" #reg); \
    verifyGPR(reg, result, value)

#define TEST_SRC_FPR(reg, offset, base, value, tmp) \
    asm volatile ("fmr %0," #reg "\n" \
		  "fmr " #reg ",%1\n" \
		  "stfd " #reg ",%2(%3)\n" \
		  "fmr " #reg ",%0" \
		  : "=&f"(tmp) \
		  : "f"(value), "i"(offset), "b"(base) \
		  : "fr" #reg); \
    verifyDouble(base+offset, value)

#define TEST_DST_FPR(reg, offset, base, value, tmp, result) \
    asm volatile ("fmr %0," #reg "\n" \
		  "lfd " #reg ",%2(%3)\n" \
		  "fmr %1," #reg "\n" \
		  "fmr " #reg ",%0" \
		  : "=&f"(tmp), "=&f"(result) \
		  : "i"(offset), "b"(base) \
		  : "fr" #reg); \
    verifyDouble(base+offset, value)

#define TEST_UPDT_GPR(reg, offset, base, value, tmp, result) \
    asm volatile ("mr %0," #reg "\n" \
		  "mr " #reg ",%4\n" \
		  "stdu %2,%3(" #reg ")\n" \
		  "mr %1," #reg "\n" \
		  "mr " #reg ",%0" \
		  : "=&r"(tmp), "=&r"(result) \
		  : "r"(value), "i"(offset), "b"(base) \
		  : "r" #reg); \
    verifyGPR(reg, result, base+offset); \
    verifyUval64(base+offset, value)

#define TEST_ST_GPR(opcode, offset, base, value, expected) \
    asm volatile (#opcode " %0,%1(%2)" \
		  : \
		  : "r"(value), "i"(offset), "b"(base)); \
    verifyUval64(base+offset, expected)

#define TEST_ST_X_GPR(opcode, offset, base, value, expected) \
    asm volatile (#opcode " %0,%2,%1" \
		  : \
		  : "r"(value), "r"(offset), "b"(base)); \
    verifyUval64(base+offset, expected)

#define TEST_ST_U_GPR(opcode, offset, base, tmp, value, expected) \
    asm volatile ("mr %0,%3; " #opcode " %1,%2(%0)" \
		  : "=&b"(tmp) \
		  : "r"(value), "i"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyUval64(base+offset, expected)

#define TEST_ST_U_X_GPR(opcode, offset, base, tmp, value, expected) \
    asm volatile ("mr %0,%3; " #opcode " %1,%0,%2" \
		  : "=&b"(tmp) \
		  : "r"(value), "r"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyUval64(base+offset, expected)

#define TEST_ST_FPR(opcode, offset, base, value, expected) \
    asm volatile (#opcode " %0,%1(%2)" \
		  : \
		  : "f"(value), "i"(offset), "b"(base)); \
    verifyUval64(base+offset, expected)

#define TEST_ST_X_FPR(opcode, offset, base, value, expected) \
    asm volatile (#opcode " %0,%2,%1" \
		  : \
		  : "f"(value), "r"(offset), "b"(base)); \
    verifyUval64(base+offset, expected)

#define TEST_ST_U_FPR(opcode, offset, base, tmp, value, expected) \
    asm volatile ("mr %0,%3; " #opcode " %1,%2(%0)" \
		  : "=&b"(tmp) \
		  : "f"(value), "i"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyUval64(base+offset, expected)

#define TEST_ST_U_X_FPR(opcode, offset, base, tmp, value, expected) \
    asm volatile ("mr %0,%3; " #opcode " %1,%0,%2" \
		  : "=&b"(tmp) \
		  : "f"(value), "r"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyUval64(base+offset, expected)

#define TEST_LD_GPR(opcode, offset, base, result, value, expected) \
    initUval64(base+offset, value); \
    asm volatile (#opcode " %0,%1(%2)" \
		  : "=&r"(result) \
		  : "i"(offset), "b"(base)); \
    verifyGPR(uval(-1), result, expected)

#define TEST_LD_X_GPR(opcode, offset, base, result, value, expected) \
    initUval64(base+offset, value); \
    asm volatile (#opcode " %0,%2,%1" \
		  : "=&r"(result) \
		  : "r"(offset), "b"(base)); \
    verifyGPR(uval(-1), result, expected)

#define TEST_LD_U_GPR(opcode, offset, base, result, tmp, value, expected) \
    initUval64(base+offset, value); \
    asm volatile ("mr %0,%3; " #opcode " %1,%2(%0)" \
		  : "=&b"(tmp), "=&r"(result) \
		  : "i"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyGPR(uval(-1), result, expected)

#define TEST_LD_U_X_GPR(opcode, offset, base, result, tmp, value, expected) \
    initUval64(base+offset, value); \
    asm volatile ("mr %0,%3; " #opcode " %1,%0,%2" \
		  : "=&b"(tmp), "=&r"(result) \
		  : "r"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyGPR(uval(-1), result, expected)

#define TEST_LD_FPR(opcode, offset, base, result, value, expected) \
    initUval64(base+offset, value); \
    asm volatile (#opcode " %0,%1(%2)" \
		  : "=&f"(result) \
		  : "i"(offset), "b"(base)); \
    verifyFPR(uval(-1), result, expected)

#define TEST_LD_X_FPR(opcode, offset, base, result, value, expected) \
    initUval64(base+offset, value); \
    asm volatile (#opcode " %0,%2,%1" \
		  : "=&f"(result) \
		  : "r"(offset), "b"(base)); \
    verifyFPR(uval(-1), result, expected)

#define TEST_LD_U_FPR(opcode, offset, base, result, tmp, value, expected) \
    initUval64(base+offset, value); \
    asm volatile ("mr %0,%3; " #opcode " %1,%2(%0)" \
		  : "=&b"(tmp), "=&f"(result) \
		  : "i"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyFPR(uval(-1), result, expected)

#define TEST_LD_U_X_FPR(opcode, offset, base, result, tmp, value, expected) \
    initUval64(base+offset, value); \
    asm volatile ("mr %0,%3; " #opcode " %1,%0,%2" \
		  : "=&b"(tmp), "=&f"(result) \
		  : "r"(offset), "b"(base)); \
    verifyGPR(uval(-1), tmp, base+offset); \
    verifyFPR(uval(-1), result, expected)

int
main(int argc, char *argv[], char *envp[])
{
    NativeProcess();

    uval space;
    uval64 tmpgpr, resgpr;
    double tmpfpr, resfpr;

    space = (((uval) malloc(0x40000 * 0x1000)) + 0xfff) & ~0xfff;

    // We need to test the use of r13 as the source, destination, and base
    // register of mis-aligned accesses.  But we can't mess with it without
    // being disabled.

    Scheduler::Disable();
    Thread *curThreadSave = CurrentThread;

    TEST_SRC_GPR( 0, 0x000, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 1, 0x010, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 2, 0x020, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 3, 0x030, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 4, 0x040, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 5, 0x050, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 6, 0x060, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 7, 0x070, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 8, 0x080, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR( 9, 0x090, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(10, 0x0a0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(11, 0x0b0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(12, 0x0c0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(13, 0x0d0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(14, 0x0e0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(15, 0x0f0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(16, 0x100, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(17, 0x110, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(18, 0x120, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(19, 0x130, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(20, 0x140, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(21, 0x150, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(22, 0x160, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(23, 0x170, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(24, 0x180, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(25, 0x190, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(26, 0x1a0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(27, 0x1b0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(28, 0x1c0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(29, 0x1d0, space+2, 0xfedcba9876543210ull, tmpgpr);
//    TEST_SRC_GPR(30, 0x1e0, space+2, 0xfedcba9876543210ull, tmpgpr);
    TEST_SRC_GPR(31, 0x1f0, space+2, 0xfedcba9876543210ull, tmpgpr);

    TEST_DST_GPR( 0, 0x000, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 1, 0x010, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 2, 0x020, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 3, 0x030, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 4, 0x040, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 5, 0x050, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 6, 0x060, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 7, 0x070, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 8, 0x080, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR( 9, 0x090, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(10, 0x0a0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(11, 0x0b0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(12, 0x0c0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(13, 0x0d0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(14, 0x0e0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(15, 0x0f0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(16, 0x100, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(17, 0x110, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(18, 0x120, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(19, 0x130, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(20, 0x140, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(21, 0x150, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(22, 0x160, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(23, 0x170, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(24, 0x180, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(25, 0x190, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(26, 0x1a0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(27, 0x1b0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(28, 0x1c0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(29, 0x1d0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
//    TEST_DST_GPR(30, 0x1e0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_DST_GPR(31, 0x1f0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);

    TEST_SRC_FPR( 0, 0x200, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 1, 0x210, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 2, 0x220, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 3, 0x230, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 4, 0x240, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 5, 0x250, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 6, 0x260, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 7, 0x270, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 8, 0x280, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR( 9, 0x290, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(10, 0x2a0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(11, 0x2b0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(12, 0x2c0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(13, 0x2d0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(14, 0x2e0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(15, 0x2f0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(16, 0x300, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(17, 0x310, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(18, 0x320, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(19, 0x330, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(20, 0x340, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(21, 0x350, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(22, 0x360, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(23, 0x370, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(24, 0x380, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(25, 0x390, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(26, 0x3a0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(27, 0x3b0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(28, 0x3c0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(29, 0x3d0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(30, 0x3e0, space+2, 1.0/3.0, tmpfpr);
    TEST_SRC_FPR(31, 0x3f0, space+2, 1.0/3.0, tmpfpr);

    TEST_DST_FPR( 0, 0x200, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 1, 0x210, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 2, 0x220, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 3, 0x230, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 4, 0x240, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 5, 0x250, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 6, 0x260, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 7, 0x270, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 8, 0x280, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR( 9, 0x290, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(10, 0x2a0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(11, 0x2b0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(12, 0x2c0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(13, 0x2d0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(14, 0x2e0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(15, 0x2f0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(16, 0x300, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(17, 0x310, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(18, 0x320, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(19, 0x330, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(20, 0x340, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(21, 0x350, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(22, 0x360, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(23, 0x370, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(24, 0x380, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(25, 0x390, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(26, 0x3a0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(27, 0x3b0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(28, 0x3c0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(29, 0x3d0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(30, 0x3e0, space+2, 1.0/3.0, tmpfpr, resfpr);
    TEST_DST_FPR(31, 0x3f0, space+2, 1.0/3.0, tmpfpr, resfpr);

    //TEST_UPDT_GPR( 0, 0x400, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 1, 0x410, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 2, 0x420, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 3, 0x430, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 4, 0x440, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 5, 0x450, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 6, 0x460, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 7, 0x470, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 8, 0x480, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR( 9, 0x490, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(10, 0x4a0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(11, 0x4b0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(12, 0x4c0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(13, 0x4d0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(14, 0x4e0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(15, 0x4f0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(16, 0x500, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(17, 0x510, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(18, 0x520, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(19, 0x530, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(20, 0x540, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(21, 0x550, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(22, 0x560, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(23, 0x570, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(24, 0x580, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(25, 0x590, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(26, 0x5a0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(27, 0x5b0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(28, 0x5c0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(29, 0x5d0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
//    TEST_UPDT_GPR(30, 0x5e0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);
    TEST_UPDT_GPR(31, 0x5f0, space+2, 0xfedcba9876543210ull, tmpgpr, resgpr);

    CurrentThread = curThreadSave;
    Scheduler::Enable();

    TEST_ST_GPR(sth, 0x1000, space+0x01fff,
				0xfedcba9876543210ull, 0x3210000000000000ull);
    TEST_ST_GPR(stw, 0x1000, space+0x02fff,
				0xfedcba9876543210ull, 0x7654321000000000ull);
    TEST_ST_GPR(std, 0x1000, space+0x03fff,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_ST_X_GPR(sthx, 0x1000, space+0x04fff,
				0xfedcba9876543210ull, 0x3210000000000000ull);
    TEST_ST_X_GPR(stwx, 0x1000, space+0x05fff,
				0xfedcba9876543210ull, 0x7654321000000000ull);
    TEST_ST_X_GPR(stdx, 0x1000, space+0x06fff,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_ST_X_GPR(sthbrx, 0x1000, space+0x07fff,
				0xfedcba9876543210ull, 0x1032000000000000ull);
    TEST_ST_X_GPR(stwbrx, 0x1000, space+0x08fff,
				0xfedcba9876543210ull, 0x1032547600000000ull);
    TEST_ST_U_GPR(sthu, 0x1000, space+0x09fff, tmpgpr,
				0xfedcba9876543210ull, 0x3210000000000000ull);
    TEST_ST_U_GPR(stwu, 0x1000, space+0x0afff, tmpgpr,
				0xfedcba9876543210ull, 0x7654321000000000ull);
    TEST_ST_U_GPR(stdu, 0x1000, space+0x0bfff, tmpgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_ST_U_X_GPR(sthux, 0x1000, space+0x0cfff, tmpgpr,
				0xfedcba9876543210ull, 0x3210000000000000ull);
    TEST_ST_U_X_GPR(stwux, 0x1000, space+0x0dfff, tmpgpr,
				0xfedcba9876543210ull, 0x7654321000000000ull);
    TEST_ST_U_X_GPR(stdux, 0x1000, space+0x0efff, tmpgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);

    double dval = 1.0/3.0;

    uval64 double_expected;
    uval64 float_expected;

    *((double *)(&double_expected)) = dval;
    float_expected = 0;
    *((float *)(&float_expected)) = float(dval);

    TEST_ST_FPR(stfs, 0x1000, space+0x12fff, dval, float_expected);
    TEST_ST_FPR(stfd, 0x1000, space+0x13fff, dval, double_expected);
    TEST_ST_X_FPR(stfsx, 0x1000, space+0x15fff, dval, float_expected);
    TEST_ST_X_FPR(stfdx, 0x1000, space+0x16fff, dval, double_expected);
    TEST_ST_U_FPR(stfsu, 0x1000, space+0x1afff, tmpgpr, dval, float_expected);
    TEST_ST_U_FPR(stfdu, 0x1000, space+0x1bfff, tmpgpr, dval, double_expected);
    TEST_ST_U_X_FPR(stfsux, 0x1000, space+0x1dfff, tmpgpr,
					dval, float_expected);
    TEST_ST_U_X_FPR(stfdux, 0x1000, space+0x1efff, tmpgpr,
					dval, double_expected);

    TEST_LD_GPR(lhz, 0x1000, space+0x21fff, resgpr,
				0xfedcba9876543210ull, 0x000000000000fedcull);
    TEST_LD_GPR(lha, 0x1000, space+0x22fff, resgpr,
				0xfedcba9876543210ull, 0xfffffffffffffedcull);
    TEST_LD_GPR(lwz, 0x1000, space+0x23fff, resgpr,
				0xfedcba9876543210ull, 0x00000000fedcba98);
    TEST_LD_GPR(lwa, 0x1000, space+0x24fff, resgpr,
				0xfedcba9876543210ull, 0xfffffffffedcba98);
    TEST_LD_GPR(ld, 0x1000, space+0x25fff, resgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_LD_X_GPR(lhzx, 0x1000, space+0x26fff, resgpr,
				0xfedcba9876543210ull, 0x000000000000fedcull);
    TEST_LD_X_GPR(lhax, 0x1000, space+0x27fff, resgpr,
				0xfedcba9876543210ull, 0xfffffffffffffedcull);
    TEST_LD_X_GPR(lwzx, 0x1000, space+0x28fff, resgpr,
				0xfedcba9876543210ull, 0x00000000fedcba98);
    TEST_LD_X_GPR(lwax, 0x1000, space+0x29fff, resgpr,
				0xfedcba9876543210ull, 0xfffffffffedcba98);
    TEST_LD_X_GPR(ldx, 0x1000, space+0x2afff, resgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_LD_X_GPR(lhbrx, 0x1000, space+0x2bfff, resgpr,
				0xfedcba9876543210ull, 0x000000000000dcfeull);
    TEST_LD_X_GPR(lwbrx, 0x1000, space+0x2cfff, resgpr,
				0xfedcba9876543210ull, 0x0000000098badcfeull);
    TEST_LD_U_GPR(lhzu, 0x1000, space+0x2dfff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0x000000000000fedcull);
    TEST_LD_U_GPR(lhau, 0x1000, space+0x2efff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0xfffffffffffffedcull);
    TEST_LD_U_GPR(lwzu, 0x1000, space+0x2ffff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0x00000000fedcba98);
    TEST_LD_U_GPR(ldu, 0x1000, space+0x30fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);
    TEST_LD_U_X_GPR(lhzux, 0x1000, space+0x31fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0x000000000000fedcull);
    TEST_LD_U_X_GPR(lhaux, 0x1000, space+0x32fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0xfffffffffffffedcull);
    TEST_LD_U_X_GPR(lwzux, 0x1000, space+0x33fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0x00000000fedcba98);
    TEST_LD_U_X_GPR(lwaux, 0x1000, space+0x34fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0xfffffffffedcba98);
    TEST_LD_U_X_GPR(ldux, 0x1000, space+0x35fff, resgpr, tmpgpr,
				0xfedcba9876543210ull, 0xfedcba9876543210ull);

    TEST_LD_FPR(lfs, 0x1000, space+0x40fff, resfpr,
				float_expected, float(dval));
    TEST_LD_FPR(lfd, 0x1000, space+0x41fff, resfpr,
				double_expected, dval);
    TEST_LD_X_FPR(lfsx, 0x1000, space+0x42fff, resfpr,
				float_expected, float(dval));
    TEST_LD_X_FPR(lfdx, 0x1000, space+0x43fff, resfpr,
				double_expected, dval);
    TEST_LD_U_FPR(lfsu, 0x1000, space+0x44fff, resfpr, tmpgpr,
				float_expected, float(dval));
    TEST_LD_U_FPR(lfdu, 0x1000, space+0x45fff, resfpr, tmpgpr,
				double_expected, dval);
    TEST_LD_U_X_FPR(lfsux, 0x1000, space+0x46fff, resfpr, tmpgpr,
				float_expected, float(dval));
    TEST_LD_U_X_FPR(lfdux, 0x1000, space+0x47fff, resfpr, tmpgpr,
				double_expected, dval);

    return 0;
}
