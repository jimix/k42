#ifndef __MMU_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __MMU_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmu.H,v 1.25 2005/04/29 00:59:51 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: all constants and macros needed to interact with the
 *                     memory management unit.
 * **************************************************************************/

#ifndef __ASSEMBLER__

struct PTE {
    uval64 vsidWord, rpnWord;
    void clear() {vsidWord = 0; rpnWord = 0;};
};

struct PTEG {
    struct PTE entry[8];
};

#endif /* #ifndef __ASSEMBLER__ */

#define NUM_PTES_IN_PTEG 8
#define LOG_NUM_PTES_IN_PTEG 3
#define LOG_PTEG_SIZE 7

/* we are characterizing the size of the hash table by the
 * log of the number PTEs it is going to contain
 *   pte_size - number of bytes of a pte entry
 *   log_pte_size - log of the above number
 *   base_log_pte - log of the number of ptes for minimum table
 * NOTE also these numbers imply allignment constraints for the page table */
#define PTE_SIZE 16       /* change all or none */
#define LOG_PTE_SIZE 4   /* change all or none */
#define BASE_LOG_PTE 14  /* change all or none */


/* the below macros define how entries into the pte are made
 * the first value is the number of bits the entry must be shifted by
 * the vec is a vector of 1s of the right length to creat the mask
 * the mask is a mask that can be aaplied to the pte to leave only this entry
 * the get macro returns the value of the field
 * the set macro sets the value of the field
 * set and get macros sorted out for assembly reasons
 */

#define PTE_VSID_SHIFT (12)
#define PTE_VSID_VEC  (0xfffffffffffffULL)
#define PTE_VSID_MASK (PTE_VSID_VEC << PTE_VSID_SHIFT)
#define PTE_VSID_LOCK (0x8000000000000ULL)

#define PTE_API_SHIFT (7)
#define PTE_API_VEC (0x1f)
#define PTE_API_MASK (PTE_API_VEC << PTE_API_SHIFT)

#define PTE_L_SHIFT (2)
#define PTE_L_VEC (0x1)
#define PTE_L_MASK (PTE_L_VEC << PTE_L_SHIFT)

#define PTE_H_SHIFT (1)
#define PTE_H_VEC (0x1)
#define PTE_H_MASK (PTE_H_VEC << PTE_H_SHIFT)

#define PTE_V_SHIFT (0)
#define PTE_V_VEC (0x1)
#define PTE_V_MASK (PTE_V_VEC << PTE_V_SHIFT)

#define PTE_BOLTED_SHIFT (4)
#define PTE_BOLTED_VEC (0x1)
#define PTE_BOLTED_MASK (PTE_BOLTED_VEC << PTE_BOLTED_SHIFT)

#define PTE_LAST_SHIFT (3)
#define PTE_LAST_VEC (0x1)
#define PTE_LAST_MASK (PTE_LAST_VEC << PTE_LAST_SHIFT)

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

#ifndef __ASSEMBLER__

/* there are two sets of gets and sets for the pte
 * in general the first set _PTR_ are going to be more
 * expensive but are sometimes necessary assuming we
 * want to hide the definition of a PTE so try to use
 * the second set
 */
/* N.B. only use clear on pte temps, not on the table itself */

#if 0 //MAA
#define PTE_PTR_CLEAR(pte)         ((pte)->vsidWord = 0, (pte)->rpnWord = 0)
#endif /* #if 0 //MAA */

#define PTE_PTR_IS_BOLTED(pte)     ((pte)->vsidWord & PTE_BOLTED_MASK)
#define PTE_PTR_IS_REFERENCED(pte) ((pte)->rpnWord & PTE_R_MASK)
#define PTE_PTR_IS_LAST(pte)       ((pte)->vsidWord & PTE_LAST_MASK)
#define PTE_IS_BOLTED(pte)     (pte.vsidWord & PTE_BOLTED_MASK)
#define PTE_IS_REFERENCED(pte) (pte.rpnWord & PTE_R_MASK)
#define PTE_IS_LAST(pte)       (pte.vsidWord & PTE_LAST_MASK)

/*
 * this update is not fully synchronized.  It assumes that only
 * one processor is using the page table and that change bits are
 * not being used
 * see the architecture book if this changes
 */
static __inline__ void
PTE_PTR_SET(PTE* newPTE, PTE* pteToSet)
{
    __asm __volatile("\n"
	"# PTE_PTR_SET		\n"
	"	std %1,0(%0)	\n"
	"	eieio		\n"
	"	std %2,8(%0)	\n"
	"	eieio		\n"
	"	std %3,0(%0)	\n"
	"# end PTE_PTR_SET	\n"
	:
	:
	"b" (pteToSet),
	"r" (PTE_BOLTED_VEC << PTE_BOLTED_SHIFT),
	"r" (newPTE->rpnWord),
	"r" (newPTE->vsidWord));
}

#define PTE_PTR_CLEAR(pte)         ((pte)->vsidWord = 0, (pte)->rpnWord = 0)
#define PTE_PTR_V_GET(pte)         (((pte)->vsidWord & PTE_V_MASK) >> PTE_V_SHIFT)
#define PTE_PTR_V_SET(pte, val)    ((pte)->vsidWord |= ((val) << PTE_V_SHIFT))
#define PTE_PTR_VSID_GET(pte)      (((pte)->vsidWord & PTE_VSID_MASK) >> PTE_VSID_SHIFT)
#define PTE_PTR_VSID_SET(pte, val) ((pte)->vsidWord |= ((val) << PTE_VSID_SHIFT))
#define PTE_PTR_L_GET(pte)         (((pte)->vsidWord & PTE_L_MASK) >> PTE_L_SHIFT)
#define PTE_PTR_L_SET(pte, val)    ((pte)->vsidWord |= ((val) << PTE_L_SHIFT))
#define PTE_PTR_H_GET(pte)         (((pte)->vsidWord & PTE_H_MASK) >> PTE_H_SHIFT)
#define PTE_PTR_H_SET(pte, val)    ((pte)->vsidWord |= ((val) << PTE_H_SHIFT))
#define PTE_PTR_BOLTED_GET(pte)    (((pte)->vsidWord & PTE_BOLTED_MASK) >> PTE_BOLTED_SHIFT)
#define PTE_PTR_BOLTED_SET(pte, val)((pte)->vsidWord |= ((val) << PTE_BOLTED_SHIFT))
#define PTE_PTR_LAST_GET(pte)    (((pte)->vsidWord & PTE_LAST_MASK) >> PTE_LAST_SHIFT)
#define PTE_PTR_LAST_SET(pte, val)((pte)->vsidWord |= ((val) << PTE_LAST_SHIFT))
#define PTE_PTR_API_GET(pte)       (((pte)->vsidWord & PTE_API_MASK) >> PTE_API_SHIFT)
#define PTE_PTR_API_SET(pte, val)  ((pte)->vsidWord |= ((val) << PTE_API_SHIFT))
#define PTE_PTR_RPN_GET(pte)       (((pte)->rpnWord & PTE_RPN_MASK) >> PTE_RPN_SHIFT)
#define PTE_PTR_RPN_SET(pte, val)  ((pte)->rpnWord |= ((val) << PTE_RPN_SHIFT))
#define PTE_PTR_R_GET(pte)         (((pte)->rpnWord & PTE_R_MASK) >> PTE_R_SHIFT)
#define PTE_PTR_R_SET(pte, val)    ((pte)->rpnWord |= ((val) << PTE_R_SHIFT))
#define PTE_PTR_R_CLEAR(pte)       ((pte)->rpnWord &= ~PTE_R_MASK)
#define PTE_PTR_C_GET(pte)         (((pte)->rpnWord & PTE_C_MASK) >> PTE_C_SHIFT)
#define PTE_PTR_C_SET(pte, val)    ((pte)->rpnWord |= ((val) << PTE_C_SHIFT))
#define PTE_PTR_C_CLEAR(pte)       ((pte)->rpnWord &= ~PTE_C_MASK)
#define PTE_PTR_PP_GET(pte)        (((pte)->rpnWord & PTE_PP_MASK) >> PTE_PP_SHIFT)
#define PTE_PTR_PP_SET(pte, val)   ((pte)->rpnWord |= ((val) << PTE_PP_SHIFT))
#define PTE_PTR_WIMG_GET(pte)      (((pte)->rpnWord & PTE_WIMG_MASK) >> PTE_WIMG_SHIFT)
#define PTE_PTR_WIMG_SET(pte, val) ((pte)->rpnWord |= ((val) << PTE_WIMG_SHIFT))

#define PTE_SET(newPTE, pte)   PTE_PTR_SET(&(newPTE), (pte))
#define PTE_V_GET(pte)         (((pte).vsidWord & PTE_V_MASK) >> PTE_V_SHIFT)
#define PTE_V_SET(pte, val)    ((pte).vsidWord |= ((val) << PTE_V_SHIFT))
#define PTE_VSID_GET(pte)      (((pte).vsidWord & PTE_VSID_MASK) >> PTE_VSID_SHIFT)
#define PTE_VSID_SET(pte, val) ((pte).vsidWord |= ((val) << PTE_VSID_SHIFT))
#define PTE_L_GET(pte)         (((pte).vsidWord & PTE_L_MASK) >> PTE_L_SHIFT)
#define PTE_L_SET(pte, val)    ((pte).vsidWord |= ((val) << PTE_L_SHIFT))
#define PTE_H_GET(pte)         (((pte).vsidWord & PTE_H_MASK) >> PTE_H_SHIFT)
#define PTE_H_SET(pte, val)    ((pte).vsidWord |= ((val) << PTE_H_SHIFT))
#define PTE_BOLTED_GET(pte)    (((pte).vsidWord & PTE_BOLTED_MASK) >> PTE_BOLTED_SHIFT)
#define PTE_BOLTED_SET(pte,val)((pte).vsidWord |= ((val) << PTE_BOLTED_SHIFT))
#define PTE_LAST_GET(pte)      (((pte).vsidWord & PTE_LAST_MASK) >> PTE_LAST_SHIFT)
#define PTE_LAST_SET(pte,val)  ((pte).vsidWord |= ((val) << PTE_LAST_SHIFT))
#define PTE_LAST_CLEAR(pte)       ((pte).vsidWord &= ~PTE_LAST_MASK)
#define PTE_API_GET(pte)       (((pte).vsidWord & PTE_API_MASK) >> PTE_API_SHIFT)
#define PTE_API_SET(pte, val)  ((pte).vsidWord |= ((val) << PTE_API_SHIFT))
#define PTE_RPN_GET(pte)       (((pte).rpnWord & PTE_RPN_MASK) >> PTE_RPN_SHIFT)
#define PTE_RPN_SET(pte, val)  ((pte).rpnWord |= ((val) << PTE_RPN_SHIFT))
#define PTE_R_GET(pte)         (((pte).rpnWord & PTE_R_MASK) >> PTE_R_SHIFT)
#define PTE_R_SET(pte, val)    ((pte).rpnWord |= ((val) << PTE_R_SHIFT))
#define PTE_R_CLEAR(pte)       ((pte).rpnWord &= ~PTE_R_MASK)
#define PTE_C_GET(pte)         (((pte).rpnWord & PTE_C_MASK) >> PTE_C_SHIFT)
#define PTE_C_SET(pte, val)    ((pte).rpnWord |= ((val) << PTE_C_SHIFT))
#define PTE_C_CLEAR(pte)       ((pte).rpnWord &= ~PTE_C_MASK)
#define PTE_PP_GET(pte)        (((pte).rpnWord & PTE_PP_MASK) >> PTE_PP_SHIFT)
#define PTE_PP_SET(pte, val)   ((pte).rpnWord |= ((val) << PTE_PP_SHIFT))
#define PTE_WIMG_GET(pte)      (((pte).rpnWord & PTE_WIMG_MASK) >> PTE_WIMG_SHIFT)
#define PTE_WIMG_SET(pte, val) ((pte).rpnWord |= ((val) << PTE_WIMG_SHIFT))

#endif /* #ifndef __ASSEMBLER__ */

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
