# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: genhdr.awk,v 1.45 2003/11/12 02:29:10 marc Exp $
# ############################################################################

# data format: class_name parent_signature func rtype class_signature ppc ppc2 numparms refinfo1 ...
# variables passed in:
#    sourcefilename
#    dirname
#    filename

BEGIN{
  FS = ";" ;
  iclass = "";

  XF = dirname "/xobj/" "X" filename ".H";
  TXF = dirname "/xobj/" "TplX" filename ".H";
  MF = dirname "/meta/" "Meta" filename ".H";
  TMF = dirname "/meta/" "TplMeta" filename ".H";
  SF = dirname "/stub/" "Stub" filename ".H";

  xobjdefname =    toupper(       "__X_" filename "_H_");
  txobjdefname =   toupper(   "__TPL_X_" filename "_H_");
  metadefname =    toupper(    "__META_" filename "_H_");
  tplmetadefname = toupper("__TPL_META_" filename "_H_");
  stubdefname =    toupper(    "__STUB_" filename "_H_");

  xobj_parent="";
  meta_parent="";
  stub_parent="";
  xobj_auth="";
  
  inclstart = sprintf("^#ifndef[ ]+EXPORT_%s([ ]+|$)",toupper(filename));
  inclend   = sprintf("^#endif[ ]+.*EXPORT_%s",        toupper(filename));
  incldef   = sprintf("^#define[ ]+EXPORT_%s([ ]+|$)",toupper(filename));
  srand(); # set to current time
  unique = rand()*2147483648.0;
}

function copyCode( outf )
{
  copy_active = 0;
  got_copy    = 0;
  got_define  = 0;
    
  while( ( getline tmp < sourcefilename ) == 1 ) {
    if ( match(tmp,inclstart ) > 0) {
      copy_active = 1;
      got_copy = 1;
    } else {
      if ( match(tmp,inclend ) > 0) {
	copy_active = 0;
	printf("%s\n",tmp) > outf;
      } else if ( match(tmp,incldef ) > 0) {
	got_define = 1;
      }
    }
    if (copy_active == 1)
      printf("%s\n",tmp) > outf;
  }
  close(sourcefilename);
  printf("\n") > outf;
  if (copy_active == 1)
    error(22,sprintf("missing >#endif // EXPORT_\n",toupper(filename)));
  if ((got_define == 0) && (got_copy == 1))
    error(23,sprintf("missing >#define EXPORT_%s\n",toupper(filename)));
}

function getunique()
{
  unique = unique + 1.0;
  return (sprintf("%10.0f",unique));
}

function error( errnum, string )
{
  filename = "/tmp/awk" rand();
  printf("Error<genhdr>%d: %s\n",errnum,string) > filename;
  system( "cat " filename " 1>&2 " );
  close(filename);
  system( "rm " filename );
  exit 1;
}

{ 
  i=1;
  line++;
  mangledfunctionname[line]=$(i++);
  class_name[line]=$(i++);
  parent_class[line]=$(i++);
  parent_signature[line]=$(i++);
  xaccdef[line]=$(i++);
  xaccmeth[line]=$(i++);
  funcn[line]=$(i++);
  rtype[line]=$(i++); 
  class_signature[line]=$(i++);
  ppctype[line]=$(i++);
  functype[line]=$(i++);
  calleridx[line]=$(i++);
  xhandleidx[line]=$(i++);
}


######################################################################
#
#  Functions to build Meta.H and XObj.H
#
######################################################################

function createMetaHeaderFile(from,to)
{
  if (from == 1) {
    printf("#ifndef %s\n", metadefname )                                  > MF;
    printf("#define %s\n\n", metadefname )                                > MF;
    
    printf("#include <meta/%s.H>\n",meta_parent)                          > MF;
    copyCode(MF);
  }

  printf("\nclass %s : public %s {\n", mclass, meta_parent)               > MF;
  printf("\tTYPE_DECL(%s,%s);\n",mclass,meta_parent)                      > MF;
  printf("\tstatic COVTableEntry *theVTBL;\n")                            > MF;
  printf("\tstatic COVTableEntry *theMVTBL;\n")                           > MF;
  printf("protected:\n")                                                  > MF;
  printf("\t%s(){};\n", mclass)                               > MF;
  printf("\t%s(MetaObj::FakeType t):%s(t){};\n", mclass,meta_parent) > MF;
  printf("\tstatic char static_buf[];\n")				> MF;
  printf("\tinline void * operator new(size_t size, void* buf){\n")	> MF;
  printf("\t\treturn (void*)buf;\n")					> MF;
  printf("\t};\n")							> MF;
  printf("public:\n")                                                     > MF;
  printf("\tstatic void init();\n")                     > MF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, BaseProcessRef pref, AccessRights, AccessRights);\n")      > MF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, ProcessID procID, AccessRights, AccessRights);\n")      > MF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, BaseProcessRef pref, AccessRights, AccessRights, uval userData);\n")      > MF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, ProcessID procID, AccessRights, AccessRights, uval userData);\n")      > MF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, BaseProcessRef pref, AccessRights a1, AccessRights a2){\n")      > MF;
  printf("\t\treturn %s::createXHandle(o, pref, a1, a2);\n",mclass)	>MF;
  printf("\t};\n")							>MF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, ProcessID procID, AccessRights a1, AccessRights a2){\n")      > MF;
  printf("\t\treturn %s::createXHandle(o, procID, a1, a2);\n",mclass)  >MF;
  printf("\t};\n")							>MF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, BaseProcessRef pref, AccessRights a1 , AccessRights a2, uval userData){\n")      > MF;
  printf("\t\treturn %s::createXHandle(o, pref, a1, a2, userData);\n",mclass)>MF;
  printf("\t};\n")							>MF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, ProcessID procID, AccessRights a1, AccessRights a2, uval userData){\n")      > MF;
  printf("\t\treturn %s::createXHandle(o, procID, a1, a2, userData);\n",mclass)  >MF;
  printf("\t};\n")							>MF;
# define access control list
  pos = match(xaccdef[from],"[(].*[)]");
  acll = substr(xaccdef[from],pos+1,RLENGTH-2);
  num1 = split( acll, acclist, ":");
  printf("\tenum {")                                                      > MF;
  for (i = 1; i <= num1 ; i++) {
    printf("\t%s\t = %s::_rightsAvail << %d,\n\t",
	   acclist[i],meta_parent,i-1)                                    > MF;
  }
  printf("\t_rightsAvail = %s::_rightsAvail << %d };\n\n",
	 meta_parent,num1)                                                > MF;
  for( i = from; i <= to; i++ ) {
    if( functype[i] != "V" ) { 
      printf("\tvirtual SysStatus %s(CommID);\n", mangledfunctionname[i]) > MF;
    }
  }
  
  printf("};\n\n") > MF;
  if (to == line) 
    printf("#endif /* %s */\n", metadefname ) > MF;
}


function createTplMetaHeaderFile(from,to)
{
  if (from == 1) {
    printf("#ifndef %s\n", tplmetadefname )                             > TMF;
    printf("#define %s\n\n", tplmetadefname )                           > TMF;
    
    printf("#include <meta/%s.H>\n",tmeta_parent_hdr)                   > TMF;
    copyCode(TMF);
  }

  printf("\ntemplate <class T>\n")					> TMF;
  printf("class %s : public %s {\n", tmclass, tmeta_parent)		> TMF;
  printf("\tTYPE_DECL(%s,%s);\n",tmclass,tmeta_parent)			> TMF;
  printf("\tstatic COVTableEntry *theVTBL;\n")                          > TMF;
  printf("\tstatic COVTableEntry *theMVTBL;\n")                         > TMF;
  printf("protected:\n")                                                > TMF;
  printf("\t%s(){};\n", tmclass)					> TMF;
  printf("\t%s(MetaObj::FakeType t):%s(t){};\n", tmclass,tmeta_parent)  > TMF;
  printf("\tstatic char static_buf[];\n")				> TMF;
  printf("\tinline void * operator new(size_t size, void* buf){\n")	> TMF;
  printf("\t\treturn (void*)buf;\n")					> TMF;
  printf("\t};\n")							> TMF;
  printf("public:\n")                                                   > TMF;
  printf("\tstatic void init();\n")					> TMF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, BaseProcessRef pref, AccessRights, AccessRights);\n")      > TMF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, ProcessID procID, AccessRights, AccessRights);\n")      > TMF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, BaseProcessRef pref, AccessRights, AccessRights, uval userData);\n")      > TMF;
  printf("\tstatic SysStatusXHandle createXHandle(ObjRef, ProcessID procID, AccessRights, AccessRights, uval userData);\n")      > TMF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, BaseProcessRef pref, AccessRights a1, AccessRights a2){\n")      > TMF;
  printf("\t\treturn %s::createXHandle(o, pref, a1, a2);\n",tmclass)	>TMF;
  printf("\t};\n")							>TMF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, ProcessID procID, AccessRights a1, AccessRights a2){\n")      > TMF;
  printf("\t\treturn %s::createXHandle(o, procID, a1, a2);\n",tmclass)  >TMF;
  printf("\t};\n")							>TMF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, BaseProcessRef pref, AccessRights a1 , AccessRights a2, uval userData){\n")      > TMF;
  printf("\t\treturn %s::createXHandle(o, pref, a1, a2, userData);\n",tmclass)>TMF;
  printf("\t};\n")							>TMF;
  printf("\tvirtual SysStatusXHandle virtCreateXHandle(ObjRef o, ProcessID procID, AccessRights a1, AccessRights a2, uval userData){\n")      > TMF;
  printf("\t\treturn %s::createXHandle(o, procID, a1, a2, userData);\n",tmclass)  >TMF;
  printf("\t};\n")							>TMF;
# define access control list
  pos = match(xaccdef[from],"[(].*[)]");
  acll = substr(xaccdef[from],pos+1,RLENGTH-2);
  num1 = split( acll, acclist, ":");
  printf("\tenum {")                                                    > TMF;
  for (i = 1; i <= num1 ; i++) {
    printf("\t%s\t = %s::_rightsAvail << %d,\n\t",
	   acclist[i],tmeta_parent,i-1)                                 > TMF;
  }
  printf("\t_rightsAvail = %s::_rightsAvail << %d };\n\n",
	 tmeta_parent,num1)                                             > TMF;
  for( i = from; i <= to; i++ ) {
    if( functype[i] != "V" ) { 
      printf("\tvirtual SysStatus %s(CommID);\n", mangledfunctionname[i])		> TMF;
    }
  }
  
  printf("};\n\n")							> TMF;
  if (to == line) 
    printf("#endif /* %s */\n", tplmetadefname )			> TMF;
}


function createXObjHeaderFile(from,to)
{
  if (from == 1) {
    printf("/* DO NOT MODIFY THIS AUTOMATICALLY GENERATED FILE */\n")     > XF;
    
# commented out the next four lines of code because now that we have a
#  baseObj we are including Xobj and StubObj everywhere and they are
#  now stub compiled and thus were turning up the below error
# for now we make the code only valid in the STUB COMPILATION PROCESS
#    printf("#ifndef _IN_STUBGEN\n")                                       > XF;
#    printf("#error \"illegal include of <%s>\"\n",XF)                     > XF;
#    printf("#endif /* _IN_STUBGEN */\n")                                  > XF;
  
    printf("#ifndef %s\n", xobjdefname )                                  > XF;
    printf("#define %s\n\n", xobjdefname )                                > XF;
    printf("#include <meta/%s.H>\n",meta_parent)                          > XF;
    printf("#include <xobj/%s.H>\n",xobj_parent)                          > XF;
  }

  printf("\nstruct %s : public %s {\n", xclass, xobj_parent)              > XF;
  printf("public:\n")							  > XF;
  printf("    virtual SysStatus XtypeName(char *buf, uval buflen);\n")	  > XF;
  printf("    virtual TypeID    XtypeID();\n")				  > XF;
# define constructor and functions
  for( i = from; i <= to; i++ ) {
    if   (functype[i] == "V") 
      printf("\tvirtual SysStatus %s(CommID);\n", mangledfunctionname[i])          > XF;
  }
  
  printf("};\n\n")                                                        > XF;
  if (to == line)
    printf("#endif /* %s */\n", xobjdefname )                             > XF;
}



function createTplXObjHeaderFile(from,to)
{
  if (from == 1) {
    printf("/* DO NOT MODIFY THIS AUTOMATICALLY GENERATED FILE */\n")    > TXF;
    
# commented out the next four lines of code because now that we have a
#  baseObj we are including Xobj and StubObj everywhere and they are
#  now stub compiled and thus were turning up the below error
# for now we make the code only valid in the STUB COMPILATION PROCESS
#    printf("#ifndef _IN_STUBGEN\n")                                     > TXF;
#    printf("#error \"illegal include of <%s>\"\n",TXF)                  > TXF;
#    printf("#endif /* _IN_STUBGEN */\n")                                > TXF;
  
    printf("#ifndef %s\n", txobjdefname )                                > TXF;
    printf("#define %s\n\n", txobjdefname )                              > TXF;
    printf("#include <meta/%s.H>\n",tmeta_parent_hdr)                    > TXF;
    printf("#include <xobj/%s.H>\n",txobj_parent_hdr)                    > TXF;
  }

  printf("\ntemplate <class T>\n")					 > TXF;
  printf("struct %s : public %s {\n", txclass, txobj_parent)		> TXF;
  printf("public:\n")							>TXF;
  printf("    virtual SysStatus XtypeName(char *buf, uval buflen);\n")	>TXF;
  printf("    virtual TypeID    XtypeID();\n")				>TXF;

# define constructor and functions
  for( i = from; i <= to; i++ ) {
    if   (functype[i] == "V") 
      printf("\tvirtual SysStatus %s(CommID);\n", mangledfunctionname[i])            > TXF;
  }
  
  printf("};\n\n")                                                       > TXF;
  if (to == line)
    printf("#endif /* %s */\n", txobjdefname )                           > TXF;
}

########################################################################
# functions to strip the signature of various baggage
#

function stripSigBaggage(sig,isProcessID,isXHandleID)
{
  if ((isProcessID == 0) && (isXHandleID == 0)) return sig;
  
  pos = match(sig,"^[ ]*[(][ ]*");
  if (pos > 0) sigtmp = substr(sig,pos+RLENGTH,length(sig)-pos-RLENGTH+1);
  else         error(10,"problem with signature\n");
  pos = match(sigtmp," [)][^,()]*$" );
  if (pos == 0) pos = match(sigtmp,"[)][^,()]*$" );
  if (pos > 0)  sigtmp = substr(sigtmp,1,pos-1);
  else          error(11,"problem with signature\n");

  newsig = "( ";
  num1 = split( sigtmp, paramset, "," );
  cnt  = 0;
  for (j=1; j<= num1; j++) {

    if ((j != isProcessID) && (j != isXHandleID)) {
      if (cnt > 0) newsig = newsig ", ";
      newsig = newsig paramset[j];
      cnt++;
    }
  }
  newsig = newsig " )";
  return newsig;
}

function stripsigkey( sig, callerid, xhandleid )
{
# strip the signature from potentail trailing Keyworks (ASYNC etc. )
  pos = match( sig, "[_a-zA-Z][_a-zA-Z0-9]* $" );
  if( pos == 0 ) {
    if ((callerid > 0) || (xhandleid > 0))
      return stripSigBaggage(sig,callerid,xhandleid);
    else              return sig;
  }
  return stripSigBaggage(substr(sig, 1, pos),callerid,xhandleid);
}

function stripsignew( sig, callerid, xhandleid )
{
  tmp = stripsigkey(sig,callerid,xhandleid);
  pos = match( tmp, "[,](.)*" );
  if( pos == 0 ) {
    pos = match( tmp, "[)](.)*" );
    tmp2 = "( " substr(tmp, pos, RLENGTH) ;
  } else {
    tmp2 = "(" substr(tmp, pos+1, RLENGTH-1) ;
  }
  return tmp2;
}

function createStubHeaderFile(from,to)
{
  if (from == 1) {
    printf("#ifndef %s\n", stubdefname ) > SF;
    printf("#define %s\n\n", stubdefname ) > SF;
    
    printf("#include <stub/%s.H>\n",stub_parent) > SF;

    copyCode( SF );
  }
  
  printf("\nclass %s : public %s {\n", sclass, stub_parent)  > SF;
  printf("\tTYPE_DECL(%s,%s);\n",sclass,stub_parent)        > SF;
  printf("\tstatic ObjectHandle __metaoh;\n")                > SF;

  printf("public:\n\t%s(StubObjConstr) : %s(StubObj::UNINITIALIZED) { }\n",
	 sclass,stub_parent)                             > SF;
  
  printf("\tstatic void __Reset__metaoh();\n")                 > SF;

  printf("\tstatic void __Get__metaoh(ObjectHandle& oh);\n")   > SF;
  
  for( i = from; i <= to; i++ ) {
    if   (functype[i] == "V") 
      printf("\tvirtual %s %s%s;\n",
	     rtype[i], funcn[i],stripsigkey(class_signature[i],
					   calleridx[i],xhandleidx[i])) > SF;
    else if (functype[i] == "S") 
      printf("\tstatic  %s %s%s;\n",
	     rtype[i], funcn[i],stripsigkey(class_signature[i],
					   calleridx[i],xhandleidx[i])) > SF;
    else 
      printf("\t%s%s;\n",
	     sclass, stripsignew(class_signature[i],calleridx[i],xhandleidx[i])) > SF;
  }
  
  printf("};\n\n")                                       > SF;
  if (to == line) 
    printf("#endif /* %s */\n", stubdefname )              > SF;
}

########################################################################
#
#   stuff to generate the temp .C file to determine the virtual function
#   indices
########################################################################

function doMETAandXOBJstuff( file )
{
}

function build__tmpfile( from, to)
{
  if (from == 1) {
    printf("#include <sys/sysIncs.H>\n");
    printf("#include \"%s\"\n",XF);
  }
  
#  printf("\nvoid %s_func( %s *x ){ %s y = *x; (void)y;}\n",
#	 xclass,xclass,xclass);
  printf("class Dummy%s : public %s {\npublic:\n",xclass,xclass);
  printf("\tDummy%s() : %s() {};\n",xclass,xclass);
  printf("\tvirtual void __SIZE_VIRTUAL__();\n};\n\n");


# build metaxclass

  printf("class %s : public %s {\npublic:\n", mclass,meta_parent);
  for( i = from; i <= to; i++ ) {
    if( functype[i] != "V" ) 
	printf("\tvirtual SysStatus %s(CommID);\n",mangledfunctionname[i]);
  }
#  printf("\tvirtual ~%s();\n", mclass);
  printf("};\n\n");
  printf("class Dummy%s : public %s {\npublic:\n",mclass,mclass);
  printf("\tvirtual void __SIZE_VIRTUAL__();\n};\n\n");
#  printf("void %s_morefunc( %s *x ){ %s y = *x;}\n",mclass,mclass,mclass);

# build the variables
  for( i = from; i <= to; i++ ) {
    if( functype[i] == "V" ) {
      printf("void (%s::*__Virtual__%s__Var%d)() = "           \
	     "(void (%s::*)())(&%s::%s);\n",  	               \
	     xclass, xclass, i, xclass, xclass, mangledfunctionname[i] );
    } else {
      printf("void (%s::*__Virtual__%s__Var%d)() = "            \
	     "(void (%s::*)())(&%s::%s);\n",  	                \
	     mclass, mclass, i, mclass, mclass, mangledfunctionname[i] );
    }
    printf("\n");
  }
  printf("void (Dummy%s::*__VAR_SIZE_VIRTUAL__%s)() = &Dummy%s::__SIZE_VIRTUAL__;\n", xclass, nclass, xclass);
  printf("void (Dummy%s::*__VAR_SIZE_VIRTUAL__%s)() = &Dummy%s::__SIZE_VIRTUAL__;\n", mclass, mclass, mclass);
}

END{
  if (line < 1) error(2,"Nothing exported");
  
  numcls=1;
  clsstart[numcls] = 1;
  for( k = 2; k <= line; k++ ) {
    if (class_name[k] != class_name[k-1] ) {
      clsend[numcls] = k-1;
      numcls++;
      clsstart[numcls] = k;
    }

    if (match(rtype[k],"^SysStatus.*") <= 0 )
      error(4,sprintf("Function \"%s %s::%s%s\" must return <SysStatus>!",
		      rtype[k],class_name[k],funcn[k],class_signature[k]));

  }
  clsend[numcls] = line;

  doMETAandXOBJstuff(sourcefilename);
  
  for ( k=1; k <= numcls; k++) {
    nclass = class_name[clsstart[k]];
    iclass      =        nclass;
    sclass      = "Stub" nclass;
    xclass      = "X"    nclass;
    mclass      = "Meta" nclass;
    txclass     = "TplX"    nclass;
    tmclass     = "TplMeta" nclass;
    pclass      = parent_class[clsstart[k]];
    stub_parent = "Stub" pclass;
    xobj_parent = "X"    pclass;
    meta_parent = "Meta" pclass;

    if( pclass != "Obj") {
        tmeta_parent= "TplMeta" pclass "<T>";
        tmeta_parent_hdr= "TplMeta" pclass;
        txobj_parent= "TplX"    pclass "<T>";
        txobj_parent_hdr= "TplX"    pclass ;
    }else{
        tmeta_parent= "Meta" pclass;
        tmeta_parent_hdr= "Meta" pclass;
        txobj_parent= "X"    pclass ;
        txobj_parent_hdr= "X"    pclass ;
    }

    createMetaHeaderFile(clsstart[k],clsend[k]);
    createTplMetaHeaderFile(clsstart[k],clsend[k]);
    createXObjHeaderFile(clsstart[k],clsend[k]);
    createTplXObjHeaderFile(clsstart[k],clsend[k]);
    createStubHeaderFile(clsstart[k],clsend[k]);
    build__tmpfile(clsstart[k],clsend[k]);
  }
}


