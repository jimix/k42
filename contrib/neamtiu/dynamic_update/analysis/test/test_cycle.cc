/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test_cycle.cc,v 1.1 2005/09/01 21:29:26 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:
    Cycle in the method call graph. 
    Test file for cycle_detect.

 *****************************************************************************/

class A {
public:
  void fooA();
};


class B {
public:
  void B::fooB();
};

class C {
public:
  void C::fooC();
};

class D {
public:
  void D::fooD();
};


void A::fooA()
{
  B *b;
  b->fooB();
}

void B::fooB()
{
  C *c;
  c->fooC();
}

void C::fooC()
{
  D *d;
  d->fooD();
}

void D::fooD()
{
  A *a;
  a->fooA();
}


