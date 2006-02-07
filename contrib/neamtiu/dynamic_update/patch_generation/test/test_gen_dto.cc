/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test_gen_dto.cc,v 1.1 2005/09/01 21:29:35 neamtiu Exp $
 *****************************************************************************/

/******************************************************************************
Description:
    Test file for gen_dto (Data Transfer Object generation).

 *****************************************************************************/

class A { 
public:
  int a1;
  int a2;
};
class B : public A { 
private:
  int b;
};
class C : public B { 
protected:
  int c;
};

class D : public C {

public:
  int d;

  void foo();
  static void boo();
};

