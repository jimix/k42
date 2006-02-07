/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test.C,v 1.7 2001/04/11 19:31:23 dje Exp $
 *****************************************************************************/

#include <stdio.h>

class MyClassBase1 {
    int stuff[3];
public:
    MyClassBase1() { }
    ~MyClassBase1() { }
    virtual void virta(double a, double b) { }
    virtual void virt0(double a, double b) { }
    virtual void virt1(double a, double c) { }
    virtual void virt2(double a, double b) { }
    virtual void virt3(double a, double c) { }
};

class MyClassBase0 {
    int stuff[3];
public:
    MyClassBase0() { }
    ~MyClassBase0() { }
    virtual void virtbase(double a, double c) { }
};

class MyClass : public MyClassBase1, public MyClassBase0
//class MyClass : public MyClassBase1
//class MyClass
{
    int local;
    int local2;
    int local3;
public:
    MyClass();
    ~MyClass();
    void nonvirt0(double a, long b);
    void nonvirt1(double a, long b);
    virtual void virt0(double a, double b);
    virtual void virt1(double a, double c);
    virtual void virt2(double a, double b);
    virtual void virt3(double a, double c);
private:
    static int stuff;
};

int MyClass::stuff = 1;
int MyVar;

MyClass::MyClass()                       { local = 99; }
MyClass::~MyClass()                      { }
void MyClass::nonvirt0(double a, long b)    { MyVar = 1; }
void MyClass::nonvirt1(double a, long b) { }
void MyClass::virt0(double a, double b)  { stuff = 98; }
void MyClass::virt1(double a, double c)  { }
void MyClass::virt2(double a, double c)  { }
void MyClass::virt3(double a, double c)  { }

static void (MyClass::*nf1)(double,long)   = &MyClass::nonvirt0;
static void (MyClass::*nf2)(double,long)   = &MyClass::nonvirt1;

static void (MyClassBase1::*__Virtual__vbf1)(double,double) = &MyClassBase1::virt0;
static void (MyClass::*__Virtual__vf1)(double,double) = &MyClass::virt0;
static void (MyClass::*__Virtual__vf2)(double,double) = &MyClass::virt1;
static void (MyClass::*__Virtual__vf3)(double,double) = &MyClass::virt2;
static void (MyClass::*__Virtual__vf4)(double,double) = &MyClass::virt3;
static void (MyClass::*__Virtual__vf5)(double,double) = &MyClass::virtbase;

extern "C" {
void free(void*) { }
}

class MyClassVF : public MyClass {
public:
  virtual void vtbl_size();
};
static void (MyClassVF::*__VAR_SIZE_VIRTUAL__)() = & MyClassVF::vtbl_size;

#if __GNUC__ >= 3
typedef struct {
    union {
	long index;
	void *faddr;
    } u;
    long delta;
} Ptr_to_mem_func;
char *version = "GNUC 3";
void show_memfunc(Ptr_to_mem_func *f)
{
    if (f->u.index % sizeof(void*) == 0) {
	printf("nvfct  d%d f%x\n",f->delta,f->u.faddr);
    } else {
	printf("vfct d%d i%d\n",f->delta, (f->u.index - 1)/sizeof(void *));
    }
}
#else
typedef struct {
    short delta;
    short index;
    union {
	void* faddr;
	short v_off;
    };
} Ptr_to_mem_func;
char *version = "GNUC";
void show_memfunc(Ptr_to_mem_func *f)
{
    if (f->index < 0) {
	printf("nvfct  d%d f%x v%d\n",f->delta,f->faddr,f->v_off);
    } else {
	printf("vfct d%d i%d v%d\n",f->delta,f->index,f->v_off);
    }
}
#endif

main()
{
    show_memfunc((Ptr_to_mem_func*)&nf1);
    show_memfunc((Ptr_to_mem_func*)&nf2);

    show_memfunc((Ptr_to_mem_func*)&__Virtual__vbf1);
    show_memfunc((Ptr_to_mem_func*)&__Virtual__vf1);
    show_memfunc((Ptr_to_mem_func*)&__Virtual__vf2);
    show_memfunc((Ptr_to_mem_func*)&__Virtual__vf3);
    show_memfunc((Ptr_to_mem_func*)&__Virtual__vf4);
    show_memfunc((Ptr_to_mem_func*)&__Virtual__vf5);
}

