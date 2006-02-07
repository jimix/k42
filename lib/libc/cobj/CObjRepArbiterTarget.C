/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRepArbiterTarget.C,v 1.1 2004/01/24 20:58:15 bob Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: arbiter clustered object
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/COVTable.H>
#include <cobj/CObjRepArbiter.H>
#include <cobj/CObjRepArbiterTarget.H>
#include "arbiterMethods.H"

// Sets the virtual function table entry at offset OP
#define SET_ARBITER_METHOD(OP) \
 ((COVTable *)vtable)->vt[OP].setFunc((uval)&arbiterMethod ## OP)

/*static*/ COVTable* CObjRepArbiterTarget::vtable = 0;

// CObjRepArbiterTarget(rep)
//
// Init a ArbiterTarget; this is pretty simple since all it has to store is
// the ref for the arbiter object. It passes everything off to that. No local
// data since that has to be on a per-VP basis and this is a single rep. It's
// just more convenient to put it in the arbiter.
CObjRepArbiterTarget::CObjRepArbiterTarget(ArbiterRef cr):
    arbitrater(cr){

    TempStack** prevNext = &tempStacks;
    for(uval i = 0; i < numStacks; i++){
        TempStack* ts = new TempStack();
        tassert(ts, err_printf("out of memory creating TempStacks\n"));
        ts->stackBase = (uval)ts->stack + TempStack::stackSize;
        ts->recurCount = 0;
        ts->stackOffset = ts->stackBase - 0x128;
        *prevNext = ts;
        prevNext = &(ts->next);
#ifndef NDEBUG
        // trash stack with preset pattern
        memset((void*)ts->stack, 0xbf, TempStack::stackSize);
        ts->truebottomSP = (uval)ts->stack;
        ts->bottomSP = ts->truebottomSP + TempStack::stackReserved;
#endif
        // magic number 0x128 from ABI convention somewhere...
        // FIXME this is probably (hahaha) powerpc specific
    }
    *prevNext = 0;
    initVTable();
    //accessTestPointer();
}

//void CObjRepArbiterTarget::accessTestPointer(){
    //*((sval*)testPointer) = 8;
//}

CObjRepArbiterTarget::~CObjRepArbiterTarget(){
    uval i = 0;
    for(TempStack* k = tempStacks; tempStacks; k = tempStacks){
        tempStacks = tempStacks->next;
        delete k;
        i++;
    }
    // I hope it isn't possible for stacks to be stranded here...
    tassert(i == numStacks, err_printf("destroyed wrong number of stacks\n"));
}

/*extern "C" {
    uval getTempStack(CObjRepArbiterTarget* rct, ThreadID tid){
        stackLock.lock();
        for(int i = 0; i < rct->numStacks; i++)
            if(rct->tempStacks[i].tid == -1 || rct->tempStacks[i].tid == tid){
                tempStacks[i].tid = tid;
                stackLock.unlock();
                return rct->tempStacks[i];
            }
        stackLock.unlock();
        tassert(0, err_printf("ran out of stacks in getTempStack!\n"));
        return 0;
    }
}
*/

// initVTable()
//
// This function initializes the VFT of the CObjRepArbiterTarget class by
// overwriting the entries with arbitratering stubs. Copied from some
// other code (I'm not sure where; it appears in several places).
void
CObjRepArbiterTarget::initVTable()
{
    if (vtable == NULL) {
	// We only need to overwrite the vtable once for all instances
	// of this class, as the vtable is shared by all instances.
	vtable = *reinterpret_cast<COVTable **>(this);

        // note: we do not set the entries at 0 and 1, since 0 is
        //       reserved, and 1 is the destructor
        SET_ARBITER_METHOD(2);
	SET_ARBITER_METHOD(3);
	SET_ARBITER_METHOD(4);
	SET_ARBITER_METHOD(5);
	SET_ARBITER_METHOD(6);
	SET_ARBITER_METHOD(7);
	SET_ARBITER_METHOD(8);
	SET_ARBITER_METHOD(9);

	SET_ARBITER_METHOD(10);
	SET_ARBITER_METHOD(11);
	SET_ARBITER_METHOD(12);
	SET_ARBITER_METHOD(13);
	SET_ARBITER_METHOD(14);
	SET_ARBITER_METHOD(15);
	SET_ARBITER_METHOD(16);
	SET_ARBITER_METHOD(17);
	SET_ARBITER_METHOD(18);
	SET_ARBITER_METHOD(19);

	SET_ARBITER_METHOD(20);
	SET_ARBITER_METHOD(21);
	SET_ARBITER_METHOD(22);
	SET_ARBITER_METHOD(23);
	SET_ARBITER_METHOD(24);
	SET_ARBITER_METHOD(25);
	SET_ARBITER_METHOD(26);
	SET_ARBITER_METHOD(27);
	SET_ARBITER_METHOD(28);
	SET_ARBITER_METHOD(29);

	SET_ARBITER_METHOD(30);
	SET_ARBITER_METHOD(31);
	SET_ARBITER_METHOD(32);
	SET_ARBITER_METHOD(33);
	SET_ARBITER_METHOD(34);
	SET_ARBITER_METHOD(35);
	SET_ARBITER_METHOD(36);
	SET_ARBITER_METHOD(37);
	SET_ARBITER_METHOD(38);
	SET_ARBITER_METHOD(39);

	SET_ARBITER_METHOD(40);
	SET_ARBITER_METHOD(41);
	SET_ARBITER_METHOD(42);
	SET_ARBITER_METHOD(43);
	SET_ARBITER_METHOD(44);
	SET_ARBITER_METHOD(45);
	SET_ARBITER_METHOD(46);
	SET_ARBITER_METHOD(47);
	SET_ARBITER_METHOD(48);
	SET_ARBITER_METHOD(49);

	SET_ARBITER_METHOD(50);
	SET_ARBITER_METHOD(51);
	SET_ARBITER_METHOD(52);
	SET_ARBITER_METHOD(53);
	SET_ARBITER_METHOD(54);
	SET_ARBITER_METHOD(55);
	SET_ARBITER_METHOD(56);
	SET_ARBITER_METHOD(57);
	SET_ARBITER_METHOD(58);
	SET_ARBITER_METHOD(59);

	SET_ARBITER_METHOD(60);
	SET_ARBITER_METHOD(61);
	SET_ARBITER_METHOD(62);
	SET_ARBITER_METHOD(63);
	SET_ARBITER_METHOD(64);
	SET_ARBITER_METHOD(65);
	SET_ARBITER_METHOD(66);
	SET_ARBITER_METHOD(67);
	SET_ARBITER_METHOD(68);
	SET_ARBITER_METHOD(69);

	SET_ARBITER_METHOD(70);
	SET_ARBITER_METHOD(71);
	SET_ARBITER_METHOD(72);
	SET_ARBITER_METHOD(73);
	SET_ARBITER_METHOD(74);
	SET_ARBITER_METHOD(75);
	SET_ARBITER_METHOD(76);
	SET_ARBITER_METHOD(77);
	SET_ARBITER_METHOD(78);
	SET_ARBITER_METHOD(79);

	SET_ARBITER_METHOD(80);
	SET_ARBITER_METHOD(81);
	SET_ARBITER_METHOD(82);
	SET_ARBITER_METHOD(83);
	SET_ARBITER_METHOD(84);
	SET_ARBITER_METHOD(85);
	SET_ARBITER_METHOD(86);
	SET_ARBITER_METHOD(87);
	SET_ARBITER_METHOD(88);
	SET_ARBITER_METHOD(89);

	SET_ARBITER_METHOD(90);
	SET_ARBITER_METHOD(91);
	SET_ARBITER_METHOD(92);
	SET_ARBITER_METHOD(93);
	SET_ARBITER_METHOD(94);
	SET_ARBITER_METHOD(95);
	SET_ARBITER_METHOD(96);
	SET_ARBITER_METHOD(97);
	SET_ARBITER_METHOD(98);
	SET_ARBITER_METHOD(99);

	SET_ARBITER_METHOD(100);
	SET_ARBITER_METHOD(101);
	SET_ARBITER_METHOD(102);
	SET_ARBITER_METHOD(103);
	SET_ARBITER_METHOD(104);
	SET_ARBITER_METHOD(105);
	SET_ARBITER_METHOD(106);
	SET_ARBITER_METHOD(107);
	SET_ARBITER_METHOD(108);
	SET_ARBITER_METHOD(109);

	SET_ARBITER_METHOD(110);
	SET_ARBITER_METHOD(111);
	SET_ARBITER_METHOD(112);
	SET_ARBITER_METHOD(113);
	SET_ARBITER_METHOD(114);
	SET_ARBITER_METHOD(115);
	SET_ARBITER_METHOD(116);
	SET_ARBITER_METHOD(117);
	SET_ARBITER_METHOD(118);
	SET_ARBITER_METHOD(119);

	SET_ARBITER_METHOD(120);
	SET_ARBITER_METHOD(121);
	SET_ARBITER_METHOD(122);
	SET_ARBITER_METHOD(123);
	SET_ARBITER_METHOD(124);
	SET_ARBITER_METHOD(125);
	SET_ARBITER_METHOD(126);
	SET_ARBITER_METHOD(127);
	SET_ARBITER_METHOD(128);
	SET_ARBITER_METHOD(129);

	SET_ARBITER_METHOD(130);
	SET_ARBITER_METHOD(131);
	SET_ARBITER_METHOD(132);
	SET_ARBITER_METHOD(133);
	SET_ARBITER_METHOD(134);
	SET_ARBITER_METHOD(135);
	SET_ARBITER_METHOD(136);
	SET_ARBITER_METHOD(137);
	SET_ARBITER_METHOD(138);
	SET_ARBITER_METHOD(139);

	SET_ARBITER_METHOD(140);
	SET_ARBITER_METHOD(141);
	SET_ARBITER_METHOD(142);
	SET_ARBITER_METHOD(143);
	SET_ARBITER_METHOD(144);
	SET_ARBITER_METHOD(145);
	SET_ARBITER_METHOD(146);
	SET_ARBITER_METHOD(147);
	SET_ARBITER_METHOD(148);
	SET_ARBITER_METHOD(149);

	SET_ARBITER_METHOD(150);
	SET_ARBITER_METHOD(151);
	SET_ARBITER_METHOD(152);
	SET_ARBITER_METHOD(153);
	SET_ARBITER_METHOD(154);
	SET_ARBITER_METHOD(155);
	SET_ARBITER_METHOD(156);
	SET_ARBITER_METHOD(157);
	SET_ARBITER_METHOD(158);
	SET_ARBITER_METHOD(159);

	SET_ARBITER_METHOD(160);
	SET_ARBITER_METHOD(161);
	SET_ARBITER_METHOD(162);
	SET_ARBITER_METHOD(163);
	SET_ARBITER_METHOD(164);
	SET_ARBITER_METHOD(165);
	SET_ARBITER_METHOD(166);
	SET_ARBITER_METHOD(167);
	SET_ARBITER_METHOD(168);
	SET_ARBITER_METHOD(169);

	SET_ARBITER_METHOD(170);
	SET_ARBITER_METHOD(171);
	SET_ARBITER_METHOD(172);
	SET_ARBITER_METHOD(173);
	SET_ARBITER_METHOD(174);
	SET_ARBITER_METHOD(175);
	SET_ARBITER_METHOD(176);
	SET_ARBITER_METHOD(177);
	SET_ARBITER_METHOD(178);
	SET_ARBITER_METHOD(179);

	SET_ARBITER_METHOD(180);
	SET_ARBITER_METHOD(181);
	SET_ARBITER_METHOD(182);
	SET_ARBITER_METHOD(183);
	SET_ARBITER_METHOD(184);
	SET_ARBITER_METHOD(185);
	SET_ARBITER_METHOD(186);
	SET_ARBITER_METHOD(187);
	SET_ARBITER_METHOD(188);
	SET_ARBITER_METHOD(189);

	SET_ARBITER_METHOD(190);
	SET_ARBITER_METHOD(191);
	SET_ARBITER_METHOD(192);
	SET_ARBITER_METHOD(193);
	SET_ARBITER_METHOD(194);
	SET_ARBITER_METHOD(195);
	SET_ARBITER_METHOD(196);
	SET_ARBITER_METHOD(197);
	SET_ARBITER_METHOD(198);
	SET_ARBITER_METHOD(199);

	SET_ARBITER_METHOD(200);
	SET_ARBITER_METHOD(201);
	SET_ARBITER_METHOD(202);
	SET_ARBITER_METHOD(203);
	SET_ARBITER_METHOD(204);
	SET_ARBITER_METHOD(205);
	SET_ARBITER_METHOD(206);
	SET_ARBITER_METHOD(207);
	SET_ARBITER_METHOD(208);
	SET_ARBITER_METHOD(209);

	SET_ARBITER_METHOD(210);
	SET_ARBITER_METHOD(211);
	SET_ARBITER_METHOD(212);
	SET_ARBITER_METHOD(213);
	SET_ARBITER_METHOD(214);
	SET_ARBITER_METHOD(215);
	SET_ARBITER_METHOD(216);
	SET_ARBITER_METHOD(217);
	SET_ARBITER_METHOD(218);
	SET_ARBITER_METHOD(219);

	SET_ARBITER_METHOD(220);
	SET_ARBITER_METHOD(221);
	SET_ARBITER_METHOD(222);
	SET_ARBITER_METHOD(223);
	SET_ARBITER_METHOD(224);
	SET_ARBITER_METHOD(225);
	SET_ARBITER_METHOD(226);
	SET_ARBITER_METHOD(227);
	SET_ARBITER_METHOD(228);
	SET_ARBITER_METHOD(229);

	SET_ARBITER_METHOD(230);
	SET_ARBITER_METHOD(231);
	SET_ARBITER_METHOD(232);
	SET_ARBITER_METHOD(233);
	SET_ARBITER_METHOD(234);
	SET_ARBITER_METHOD(235);
	SET_ARBITER_METHOD(236);
	SET_ARBITER_METHOD(237);
	SET_ARBITER_METHOD(238);
	SET_ARBITER_METHOD(239);

	SET_ARBITER_METHOD(240);
	SET_ARBITER_METHOD(241);
	SET_ARBITER_METHOD(242);
	SET_ARBITER_METHOD(243);
	SET_ARBITER_METHOD(244);
	SET_ARBITER_METHOD(245);
	SET_ARBITER_METHOD(246);
	SET_ARBITER_METHOD(247);
	SET_ARBITER_METHOD(248);
	SET_ARBITER_METHOD(249);

	SET_ARBITER_METHOD(250);
	SET_ARBITER_METHOD(251);
	SET_ARBITER_METHOD(252);
	SET_ARBITER_METHOD(253);
	SET_ARBITER_METHOD(254);
	SET_ARBITER_METHOD(255);
    }
}

