#ifndef __POWERPC_MMU_H_
#define __POWERPC_MMU_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmu.h,v 1.8 2004/02/27 17:14:30 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: all constants and macros needed to interact with the
 *                     memory management unit.
 * **************************************************************************/

#define LOG_PGSIZE 12
#define PGSIZE (1<<LOG_PGSIZE)

#define LOG_NUM_PTES_IN_PTEG 3
#define NUM_PTES_IN_PTEG (((uval64)1) << LOG_NUM_PTES_IN_PTEG)

#define LOG_PTE_SIZE 4
#define PTE_SIZE (((uval64)1) << LOG_PTE_SIZE)

#define LOG_NUM_PTES_MIN 14	/* log of the minimum number of PTEs allowed */
#define NUM_PTES_MIN (((uval64)1) << LOG_NUM_PTES_MIN)

/* the below macros define how entries into the pte are made
 * the first value is the number of bits the entry must be shifted by
 * the vec is a vector of 1s of the right length to creat the mask
 * the mask is a mask that can be aaplied to the pte to leave only this entry
 * the get macro returns the value of the field
 * the set macro sets the value of the field
 * set and get macros sorted out for assembly reasons
 */

#define PTE_VSID_SHIFT (12)
#define PTE_VSID_VEC (0xfffffffffffffULL)
#define PTE_VSID_MASK (PTE_VSID_VEC << PTE_VSID_SHIFT)

#define PTE_API_SHIFT (7)
#define PTE_API_VEC (0x1f)
#define PTE_API_MASK (PTE_API_VEC << PTE_API_SHIFT)

#define PTE_H_SHIFT (1)
#define PTE_H_VEC (0x1)
#define PTE_H_MASK (PTE_H_VEC << PTE_H_SHIFT)

#define PTE_V_SHIFT (0)
#define PTE_V_VEC (0x1)
#define PTE_V_MASK (PTE_V_VEC << PTE_V_SHIFT)

#define PTE_RPN_SHIFT (12)
#define PTE_RPN_VEC (0xfffffffffffffULL)
#define PTE_RPN_MASK (PTE_RPN_VEC << PTE_RPN_SHIFT)

#define PTE_R_SHIFT (8)
#define PTE_R_VEC (0x1)
#define PTE_R_MASK (PTE_R_VEC << PTE_R_SHIFT)

#define PTE_C_SHIFT (7)
#define PTE_C_VEC (0x1)
#define PTE_C_MASK (PTE_C_VEC << PTE_C_SHIFT)

#define PTE_WIMG_SHIFT (3)
#define PTE_WIMG_VEC (0xf)
#define PTE_WIMG_MASK (PTE_WIMG_VEC << PTE_WIMG_SHIFT)

#define PTE_PP_SHIFT (0)
#define PTE_PP_VEC (0x3)
#define PTE_PP_MASK (PTE_PP_VEC << PTE_PP_SHIFT)

/* there are two sets of gets and sets for the pte
 * in general the first set _PTR_ are going to be more
 * expensive but are sometimes necessary assuming we
 * want to hide the definition of a PTE so try to use
 * the second set
 */
#define PTE_PTE_CLEAR(pte)         (pte->vsidWord = 0, pte->rpnWord = 0)
#define PTE_PTE_SET(newPTE, pte)   (pte->vsidWord = newPTE->vsidWord, \\
				pte->rpnWord = newPTE->rpnWord)
#define PTE_PTR_CLEAR(pte)         (pte->vsidWord = 0, pte->rpnWord = 0)
#define PTE_PTR_V_GET(pte)         ((pte->vsidWord & PTE_V_MASK) >> PTE_V_SHIFT)
#define PTE_PTR_V_SET(pte, val)    (pte->vsidWord |= ((val) << PTE_V_SHIFT))
#define PTE_PTR_VSID_GET(pte)      ((pte->vsidWord & PTE_VSID_MASK) >> PTE_VSID_SHIFT)
#define PTE_PTR_VSID_SET(pte, val) (pte->vsidWord |= ((val) << PTE_VSID_SHIFT))
#define PTE_PTR_H_GET(pte)         ((pte->vsidWord & PTE_H_MASK) >> PTE_H_SHIFT)
#define PTE_PTR_H_SET(pte, val)    (pte->vsidWord |= ((val) << PTE_H_SHIFT))
#define PTE_PTR_API_GET(pte)       ((pte->vsidWord & PTE_API_MASK) >> PTE_API_SHIFT)
#define PTE_PTR_API_SET(pte, val)  (pte->vsidWord |= ((val) << PTE_API_SHIFT))
#define PTE_PTR_RPN_GET(pte)       ((pte->rpnWord & PTE_RPN_MASK) >> PTE_RPN_SHIFT)
#define PTE_PTR_RPN_SET(pte, val)  (pte->rpnWord |= ((val) << PTE_RPN_SHIFT))
#define PTE_PTR_R_GET(pte)         ((pte->rpnWord & PTE_R_MASK) >> PTE_R_SHIFT)
#define PTE_PTR_R_SET(pte, val)    (pte->rpnWord |= ((val) << PTE_R_SHIFT))
#define PTE_PTR_C_GET(pte)         ((pte->rpnWord & PTE_C_MASK) >> PTE_C_SHIFT)
#define PTE_PTR_C_SET(pte, val)    (pte->rpnWord |= ((val) << PTE_C_SHIFT))
#define PTE_PTR_PP_GET(pte)        ((pte->rpnWord & PTE_PP_MASK) >> PTE_PP_SHIFT)
#define PTE_PTR_PP_SET(pte, val)   (pte->rpnWord |= ((val) << PTE_PP_SHIFT))
#define PTE_PTR_WIMG_GET(pte)      ((pte->rpnWord & PTE_WIMG_MASK) >> PTE_WIMG_SHIFT)
#define PTE_PTR_WIMG_SET(pte, val) (pte->rpnWord |= ((val) << PTE_WIMG_SHIFT))

#define PTE_CLEAR(pte)         (pte.vsidWord = 0, pte.rpnWord = 0)
#define PTE_SET(newPTE, pte)   (pte->vsidWord = newPTE.vsidWord, \
				pte->rpnWord = newPTE.rpnWord)
#define PTE_V_GET(pte)         ((pte.vsidWord & PTE_V_MASK) >> PTE_V_SHIFT)
#define PTE_V_SET(pte, val)    (pte.vsidWord |= ((val) << PTE_V_SHIFT))
#define PTE_VSID_GET(pte)      ((pte.vsidWord & PTE_VSID_MASK) >> PTE_VSID_SHIFT)
#define PTE_VSID_SET(pte, val) (pte.vsidWord |= ((val) << PTE_VSID_SHIFT))
#define PTE_H_GET(pte)         ((pte.vsidWord & PTE_H_MASK) >> PTE_H_SHIFT)
#define PTE_H_SET(pte, val)    (pte.vsidWord |= ((val) << PTE_H_SHIFT))
#define PTE_API_GET(pte)       ((pte.vsidWord & PTE_API_MASK) >> PTE_API_SHIFT)
#define PTE_API_SET(pte, val)  (pte.vsidWord |= ((val) << PTE_API_SHIFT))
#define PTE_RPN_GET(pte)       ((pte.rpnWord & PTE_RPN_MASK) >> PTE_RPN_SHIFT)
#define PTE_RPN_SET(pte, val)  (pte.rpnWord |= ((val) << PTE_RPN_SHIFT))
#define PTE_R_GET(pte)         ((pte.rpnWord & PTE_R_MASK) >> PTE_R_SHIFT)
#define PTE_R_SET(pte, val)    (pte.rpnWord |= ((val) << PTE_R_SHIFT))
#define PTE_C_GET(pte)         ((pte.rpnWord & PTE_C_MASK) >> PTE_C_SHIFT)
#define PTE_C_SET(pte, val)    (pte.rpnWord |= ((val) << PTE_C_SHIFT))
#define PTE_PP_GET(pte)        ((pte.rpnWord & PTE_PP_MASK) >> PTE_PP_SHIFT)
#define PTE_PP_SET(pte, val)   (pte.rpnWord |= ((val) << PTE_PP_SHIFT))
#define PTE_WIMG_GET(pte)      ((pte.rpnWord & PTE_WIMG_MASK) >> PTE_WIMG_SHIFT)
#define PTE_WIMG_SET(pte, val) (pte.rpnWord |= ((val) << PTE_WIMG_SHIFT))

/* used to turn a vsid into a number usable in the hash function */
#define VSID_HASH_MASK (0x0007fffffffffULL)

/* used to shift the htab mask to the place needed by the hashing function */
#define HTABMASK_SHIFT (11)

/* used to take a vaddr so it can be used in the hashing function */
#define EA_HASH_VEC (0xffff)
#define EA_HASH_SHIFT (12)
#define EA_HASH_MASK (EA_HASH_VEC << EA_HASH_SHIFT)

#define VADDR_TO_API(vaddr)  (((vaddr) & API_MASK) >> API_SHIFT)

/* used to turn a vaddr into an api for a pte */
#define API_VEC   (0x1f)
#define API_SHIFT (23)
#define API_MASK  (API_VEC << API_SHIFT)

/* real page number shift to create the rpn field of the pte */
#define RPN_SHIFT (12)

struct PTE {
    uval64 vsidWord, rpnWord;
};

struct PTEG {
    struct PTE entry[8];
};

/***************************************************************************
 *
 * Segment Table
 *
 **************************************************************************/

#define LOG_NUM_STES_IN_STEG 3
#define NUM_STES_IN_STEG (((uval64)1) << LOG_NUM_STES_IN_STEG)

#define LOG_STE_SIZE 4
#define STE_SIZE (((uval64)1) << LOG_STE_SIZE)

#define STE_ESID_SHIFT (28)
#define STE_ESID_VEC (0xFFFFFFFFFULL)
#define STE_ESID_MASK (STE_ESID_VEC << STE_ESID_SHIFT)

#define STE_V_SHIFT (7)
#define STE_V_VEC (1)
#define STE_V_MASK (STE_V_VEC << STE_V_SHIFT)

#define STE_T_SHIFT (6)
#define STE_T_VEC (1)
#define STE_T_MASK (STE_T_VEC << STE_T_SHIFT)

#define STE_KS_SHIFT (5)
#define STE_KS_VEC (1)
#define STE_KS_MASK (STE_KS_VEC << STE_KS_SHIFT)

#define STE_KP_SHIFT (4)
#define STE_KP_VEC (1)
#define STE_KP_MASK (STE_KP_VEC << STE_KP_SHIFT)

#define STE_VSID_SHIFT (12)
#define STE_VSID_VEC (0xFFFFFFFFFFFFFULL)
#define STE_VSID_MASK (STE_VSID_VEC << STE_VSID_SHIFT)

#define STE_PTR_CLEAR(ste)         (ste->esidWord = 0, ste->vsidWord = 0)
#define STE_PTR_SET(newSTE, ste)   (ste->esidWord = newSTE->esidWord, \
                                    ste->vsidWord = newSTE->vsidWord)

#define STE_PTR_ESID_GET(ste)      ((ste->esidWord >> STE_ESID_SHIFT) & STE_ESID_VEC)
#define STE_PTR_ESID_SET(ste, val) (ste->esidWord |= (((val) & STE_ESID_VEC) << STE_ESID_SHIFT))

#define STE_PTR_V_GET(ste)         ((ste->esidWord >> STE_V_SHIFT) & STE_V_VEC)
#define STE_PTR_V_SET(ste, val)    (ste->esidWord |= (((val) & STE_V_VEC) << STE_V_SHIFT))

#define STE_PTR_T_GET(ste)         ((ste->esidWord >> STE_T_SHIFT) & STE_T_VEC)
#define STE_PTR_T_SET(ste, val)    (ste->esidWord |= (((val) & STE_T_VEC) << STE_T_SHIFT))

#define STE_PTR_KS_GET(ste)        ((ste->esidWord >> STE_KS_SHIFT) & STE_KS_VEC)
#define STE_PTR_KS_SET(ste, val)   (ste->esidWord |= (((val) & STE_KS_VEC) << STE_KS_SHIFT))

#define STE_PTR_KP_GET(ste)        ((ste->esidWord >> STE_KP_SHIFT) & STE_KP_VEC)
#define STE_PTR_KP_SET(ste, val)   (ste->esidWord |= (((val) & STE_KP_VEC) << STE_KP_SHIFT))

#define STE_PTR_VSID_GET(ste)      ((ste->vsidWord >> STE_VSID_SHIFT) & STE_VSID_VEC)
#define STE_PTR_VSID_SET(ste, val) (ste->vsidWord |= (((val) & STE_VSID_VEC) << STE_VSID_SHIFT))

#define STE_CLEAR(ste)             (ste.esidWord = 0, ste.vsidWord = 0)
#define STE_SET(newSTE, ste)       (ste->esidWord = newSTE.esidWord, \
                                    ste->vsidWord = newSTE.vsidWord)

#define STE_ESID_GET(ste)          ((ste.esidWord >> STE_ESID_SHIFT) & STE_ESID_VEC)
#define STE_ESID_SET(ste, val)     (ste.esidWord |= (((val) & STE_ESID_VEC) << STE_ESID_SHIFT))

#define STE_V_GET(ste)             ((ste.esidWord >> STE_V_SHIFT) & STE_V_VEC)
#define STE_V_SET(ste, val)        (ste.esidWord |= (((val) & STE_V_VEC) << STE_V_SHIFT))

#define STE_T_GET(ste)             ((ste.esidWord >> STE_T_SHIFT) & STE_T_VEC)
#define STE_T_SET(ste, val)        (ste.esidWord |= (((val) & STE_T_VEC) << STE_T_SHIFT))

#define STE_KS_GET(ste)            ((ste.esidWord >> STE_KS_SHIFT) & STE_KS_VEC)
#define STE_KS_SET(ste, val)       (ste.esidWord |= (((val) & STE_KS_VEC) << STE_KS_SHIFT))

#define STE_KP_GET(ste)            ((ste.esidWord >> STE_KP_SHIFT) & STE_KP_VEC)
#define STE_KP_SET(ste, val)       (ste.esidWord |= (((val) & STE_KP_VEC) << STE_KP_SHIFT))

#define STE_VSID_GET(ste)          ((ste.vsidWord >> STE_VSID_SHIFT) & STE_VSID_VEC)
#define STE_VSID_SET(ste, val)     (ste.vsidWord |= (((val) & STE_ESID_VEC) << STE_VSID_SHIFT))

struct STE {
    uval64 esidWord, vsidWord;
};

struct STEG {
    struct STE entry[8];
};

#endif /* #ifndef __POWERPC_MMU_H_ */




