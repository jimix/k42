/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProgExecArch.C,v 1.8 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: has functions that takes the address of the beginning
 *                     of an elf file and returns pointers to its data, text,
 *                     entry, bss, and other critical components.  Code modified
 *                     from code in elf.c taken from Toronto
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/sysTypes.H>
#include <elf.h>
#include "elf.h"
#include <usr/ProgExec.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <stub/StubFRComputation.H>
#include <sys/ProcessLinuxClient.H>
#include <usr/GDBIO.H>

#define WORDTYPE typeof(exectype.pointer)
#define AUXTYPE typeof(exectype.auxv)
//#define WORDTYPE typeof(EXECTYPE::pointer)


template<typename EXECTYPE>
static inline
void PushAux(uval type, uval val, uval &memTop)
{
    EXECTYPE exectype;
    memTop -= (2 * sizeof(WORDTYPE));
    ((WORDTYPE *) memTop)[0] = (WORDTYPE)(type);
    ((WORDTYPE *) memTop)[1] = (WORDTYPE)(val);
}

template<typename EXECTYPE>
SysStatus
ProgExec::PutAuxVector(uval &memTop, BinInfo &info)
{
    // We're pushing on in reverse order
    PushAux<EXECTYPE>(AT_NULL, 0, memTop);

    PushAux<EXECTYPE>(AT_HWCAP, 0, memTop);
    PushAux<EXECTYPE>(AT_PAGESZ, PAGE_SIZE, memTop);

    if (info.interp) {
	/* Provide ld.so load addr */
	PushAux<EXECTYPE>(
	    AT_BASE, info.interp->seg[info.interp->textSegIndex].vaddr,
	    memTop);
    }

    //Linux doesn't know what this means either
    PushAux<EXECTYPE>(AT_DCACHEBSIZE,
		      kernelInfoLocal.PCacheLineSize(), memTop);
    PushAux<EXECTYPE>(AT_ICACHEBSIZE,
		      kernelInfoLocal.PCacheLineSize(), memTop);
    PushAux<EXECTYPE>(AT_UCACHEBSIZE, 0, memTop);

    PushAux<EXECTYPE>(AT_PHNUM, info.phNum, memTop);
    PushAux<EXECTYPE>(AT_PHDR, info.phdrOffset, memTop);
    PushAux<EXECTYPE>(AT_ENTRY, info.entryPointSpec, memTop);

    /* Linux does this to allow aux vector to begin at nice alignment */
    PushAux<EXECTYPE>(AT_IGNOREPPC, AT_IGNOREPPC, memTop);
    PushAux<EXECTYPE>(AT_IGNOREPPC, AT_IGNOREPPC, memTop);
    return 0;
}

template<typename EXECTYPE, class HDR>
SysStatus
ProgExec::parsePHdr(HDR *phdr, uval phdr_num, ProgExec::BinInfo *info)
{
    /* we assume sections are all adjacent to each other */
    /* we assume only one text and one data/bss section */
    info->numSegs = 0;
    uval i;
    for (i=0; i<phdr_num; i++,phdr++) {
	if (phdr->p_type==PT_LOAD) {
	    passertMsg(info->numSegs < BinInfo::MAXNUMSEG,
		       "too many segments\n");
	    tassert((phdr->p_memsz >= phdr->p_filesz),
		    err_printf("seomthing's wrong with data segment\n"));
	    info->seg[info->numSegs].offset = phdr->p_offset;
	    info->seg[info->numSegs].vaddr = phdr->p_vaddr;
	    info->seg[info->numSegs].filesz = phdr->p_filesz;
	    info->seg[info->numSegs].memsz = phdr->p_memsz;
	    info->seg[info->numSegs].flags = phdr->p_flags;
	    info->numSegs += 1;
	} else 	if (phdr->p_type==PT_INTERP && info) {
	    info->interpOffset = phdr->p_offset;
	}
    }

    /*
     *FIXME Marc and Bryan 3/2/2004
     * In fixing a bug, we started to fix the load logic.
     * in theory, we should load multi segment modules in a systematic
     * way, looping over the segments respecting flags etc.  but the rest
     * of the code is too painful to change today, so we revert to the
     * TEXT, DATA assumptions of the rest of the code by requiring
     * exactly 2 segments and calling one TEXT, one DATA.
     * we do this by kludge - the TEXT is either the first r-x segment
     * or the first segment period, the other is DATA
     */
    passertMsg(info->numSegs == 2, "Only support 2 segment modules\n");
    
    info->textSegIndex = 0;
    for (i=0; i<info->numSegs; i++) {
	if ((info->seg[i].flags & ((PF_R) | (PF_W) | (PF_X))) ==
	    ((PF_R) | (PF_X))) {
	    info->textSegIndex = i;
	    break;
	}
    }
    info->dataSegIndex = info->textSegIndex ^ 1;	// couldn't resist
    return 0;
}

SysStatus
ProgExec::ConfigStack(ProgExec::ExecInfo *info)
{
    SysStatus rc;
    info->stack.offset = ~0ULL;
    info->stack.vaddr  = (1ULL<<32ULL) - USR_STACK_SIZE;
    info->stack.memsz   = USR_STACK_SIZE;

    rc = StubFRComputation::_Create(info->localStackFR);

    return rc;
}

SysStatus
ProgExec::ParseExecutable(uval vaddr, ObjectHandle execFR,
			  ProgExec::BinInfo *info)
{
    Elf64_Ehdr *eh64;
    uval valid;				/* flag indicating validity of file */
    SysStatus rc;
    info->interpOffset = 0;
    info->archFlags = 0;

    eh64 = (Elf64_Ehdr *)vaddr;

    /* consistency checks */
    valid = 1;
    valid = valid & eh64->e_ident[EI_MAG0]==0x7f;
    valid = valid & eh64->e_ident[EI_MAG1]=='E';
    valid = valid & eh64->e_ident[EI_MAG2]=='L';
    valid = valid & eh64->e_ident[EI_MAG3]=='F';

    if (!valid) {
	return _SERROR(2760, 0, EINVAL);
    }

    valid = valid & eh64->e_ident[EI_DATA]==ELFDATA2MSB;
    tassertMsg(valid, "loader only support big-endian executables.\n");

    valid = valid & eh64->e_ident[EI_VERSION]==EV_CURRENT;
    tassertMsg(valid, "loader only supports current version ELF files.\n");

    tassertMsg(eh64->e_phoff!=0, "loader found null program header.\n");

    if (!valid) {
	return _SERROR(2761, 0, EINVAL);
    }


    if (eh64->e_ident[EI_CLASS]==ELFCLASS64) {
	Elf64_Phdr *phdr = (Elf64_Phdr *)(vaddr+eh64->e_phoff);

	rc = parsePHdr<ExecTypes64>(phdr, eh64->e_phnum, info);
	_IF_FAILURE_RET(rc);

	uval64 *entryFunc;
	entryFunc = (uval64 *) (vaddr +
				info->seg[info->dataSegIndex].offset +
				(eh64->e_entry -
				 info->seg[info->dataSegIndex].vaddr));

	info->phdrOffset = eh64->e_phoff + info->seg[info->textSegIndex].vaddr;

	info->phNum = eh64->e_phnum;
	info->entryPointSpec = eh64->e_entry;
	info->entryPointDesc.iar = (codeAddress)entryFunc[0];
	info->entryPointDesc.toc = entryFunc[1];
    } else if (eh64->e_ident[EI_CLASS]==ELFCLASS32) {
	Elf32_Ehdr *eh32 = (Elf32_Ehdr*)eh64;
	Elf32_Phdr *phdr = (Elf32_Phdr *)(vaddr+eh32->e_phoff);
	rc = parsePHdr<ExecTypes32>(phdr, eh32->e_phnum, info);
	_IF_FAILURE_RET(rc);

	info->phdrOffset = eh32->e_phoff +
	    info->seg[info->textSegIndex].vaddr;
	info->phNum = eh32->e_phnum;
	info->entryPointSpec = eh32->e_entry;
	info->entryPointDesc.iar = (codeAddress)(uval64)eh32->e_entry;
	info->entryPointDesc.toc = 0ULL;

	info->archFlags |= Exec32Bit;

    }

    info->localFR = execFR;

    return 0;
}


static uval
VecLen(char *const *vec)
{
    uval i = 0;
    while (vec[i++]);
    return i;
}


// Does not handle null-termination.  Assumes vecSize is correct.
template<typename EXECTYPE, typename WORD>
static SysStatus
PushStrings(char *const inVec[], WORD* outVec, uval vecSize,
	    uval &remoteTop, uval &localTop, uval bottom)
{
    uval offset = localTop - remoteTop;
    uval top = localTop;
    while (vecSize) {
	--vecSize;
	int len = strlen(inVec[vecSize])+1;
	top -= len;
	top &= ~(sizeof(WORD)-1); //Align down to 8-bytes

	if (top<bottom) return _SERROR(1324, 0, ENOMEM);
	memcpy((char*)top, inVec[vecSize], len);
	//outVec contains translated addresses
	outVec[vecSize] = (WORD)(top - offset);
    }
    remoteTop -= localTop - top;
    localTop = uval(top);
    return 0;
}


//
// argv, envp are NULL-terminated
//
template<typename EXECTYPE>
SysStatus
__SetupStack(uval stackBottomLocal, uval &__stackTopLocal,
	     uval &__stackTop, ProgExec::XferInfo *info,
	     ProgExec::ArgDesc* args)
{
    EXECTYPE exectype; // To make WORDTYPE work

    SysStatus rc;
    ProgExec::BinInfo &exec = info->exec.prog;
    uval stackTop = __stackTop;
    uval stackTopLocal = __stackTopLocal;
    char **envp;
    char **argv, **argvp;
    // Put argc, argv on top of stack (envp, aux vec to follow)
    envp = args->getEnvp();
    uval envcChild = VecLen(envp);
    WORDTYPE* envpChild = (WORDTYPE*)allocGlobal(sizeof(WORDTYPE)*envcChild);

    rc = PushStrings<EXECTYPE,WORDTYPE>(envp, envpChild, envcChild-1, stackTop,
					stackTopLocal, stackBottomLocal);

    _IF_FAILURE_RET(rc);
    envpChild[envcChild-1] = (WORDTYPE)NULL;

    argv = args->getArgv();
    argvp = args->getArgvPrefix();
    
    uval argcChild = VecLen(argv);
    uval argcpCount = 0;
    if(argvp) {
	argcpCount = VecLen(argvp)-1; // don't include null entry
	argcChild += argcpCount;
    }
    
    WORDTYPE* argvChild = (WORDTYPE*)allocGlobal(sizeof(WORDTYPE)*argcChild);

    rc = PushStrings<EXECTYPE,WORDTYPE>(argv, argvChild + argcpCount,
					argcChild-argcpCount-1, stackTop,
					stackTopLocal, stackBottomLocal);

    _IF_FAILURE_RET(rc);
    argvChild[argcChild -1] = (WORDTYPE)NULL;

    if(argcpCount) {
	/*
	 * A prefix, containing the shell interpreter args, is present
	 * so push it on top of the original parameters
	 */
	rc = PushStrings<EXECTYPE,WORDTYPE>(argvp, argvChild,
					argcpCount, stackTop,
					stackTopLocal, stackBottomLocal);
	_IF_FAILURE_RET(rc);
    }
	
    // Put in architecture specific aux vector
    uval origLocal = stackTopLocal;
    rc = ProgExec::PutAuxVector<EXECTYPE>(stackTopLocal, exec);
    _IF_FAILURE_RET(rc);

    stackTop -= (origLocal - stackTopLocal);
    info->auxv = (ElfW(auxv_t)*)stackTop;


    stackTopLocal -= envcChild * sizeof(WORDTYPE);
    stackTop -= envcChild * sizeof(WORDTYPE);
    memcpy((char*)stackTopLocal,envpChild, envcChild * sizeof(WORDTYPE));

    info->envp = (char**)stackTop;

    stackTopLocal -= argcChild * sizeof(WORDTYPE);
    stackTop -= argcChild * sizeof(WORDTYPE);
    memcpy((char*)stackTopLocal,argvChild, argcChild * sizeof(WORDTYPE));
    info->argv = (char**)stackTop;

    stackTopLocal -= sizeof(WORDTYPE);
    stackTop -= sizeof(WORDTYPE);
    *(WORDTYPE*)stackTopLocal = (WORDTYPE)(argcChild-1); //store argc on stack
    info->argc = argcChild-1;

    __stackTopLocal = stackTopLocal;
    __stackTop = stackTop;
    freeGlobal(envpChild, envcChild*sizeof(WORDTYPE));
    freeGlobal(argvChild, argcChild*sizeof(WORDTYPE));
    return 0;
}




SysStatus
ProgExec::SetupStack(uval stackBottomLocal, uval &__stackTopLocal,
		     uval &__stackTop, XferInfo *info,
		     ProgExec::ArgDesc *args)
{
    if (info->exec.prog.archFlags & Exec32Bit) {
	return __SetupStack<ExecTypes32>(stackBottomLocal, __stackTopLocal,
					 __stackTop, info, args);
    }

    return __SetupStack<ExecTypes64>(stackBottomLocal, __stackTopLocal,
				     __stackTop, info, args);

}
