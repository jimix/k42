# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: gentmp.awk,v 1.15 2004/10/08 21:40:11 jk Exp $
# ############################################################################

BEGIN{
  FS = ";" ;
  iclass = "";
  XF  = dirname "/tmpl/" "TplX" filename ".I";
  MF  = dirname "/tmpl/" "TplMeta" filename ".I";

  xobjdefname = toupper( "TplX" filename "_DEFC");
  metadefname = toupper( "TplMeta" filename "_DEFC");
  parent_class="";
  xobj_parent="";
  meta_parent="";

# some other variables that help document the whole thing a bit better
# rather than using some integer convention

  IS_CONST_ARRAY  = 0;
  IS_STR_ARRAY    = 1;
  IS_VAR_ARRAY    = 2;

  lastclass = "";
}

function doMETAandXOBJstuff( file )
{
}

function warning( errnum, string )
{
  filename = "/tmp/awk" rand();
  if (errnum >= 10)
    printf("warning: %s::%s %s\n",class_name,funcn,class_signature) > filename;
  printf("warning(%d): %s\n",errnum,string) > filename;
  close(filename);
  system( "cat " filename " 1>&2 " );
  system( "chmod 777 " filename );
  system( "rm " filename );
}

function error( errnum, string )
{
  filename = "/tmp/awk" rand();
  printf("Error<gentmp>%d: %s\n",errnum,string) > filename;
  if (errnum >= 10)
    printf("  in %s::%s %s\n",class_name,funcn,class_signature) > filename;
  close(filename);
  system( "cat " filename " 1>&2 " );
  system( "chmod 777 " filename );
  system( "rm " filename );
  exit 1;
}


########################################################################
# Parameter separation and stub signature generation

function separateParams(sig,nump,paramset,nameset,firstarg,functype)     # out
{
# separate the parameters out of the signature such that the returns
# hold the following content
#   <paramset>: array of complete parameter declaration
#   <nameset> : array with the name of each parameter
#   <firstarg>: the first argument separated into its components
# the function returns the stripped StubObj signature


  pos = match(sig,"^[ ]*[(][ ]*");
  if (pos > 0) sigtmp = substr(sig,pos+RLENGTH,length(sig)-pos-RLENGTH+1);
  else         error(10,"problem with signature\n");
  pos = match(sigtmp," [)][^,()]*$" );
  if (pos == 0) pos = match(sigtmp,"[)][^,()]*$" );
  if (pos > 0)  sigtmp = substr(sigtmp,1,pos-1);
  else          error(11,"problem with signature\n");

  num1 = split( sigtmp, paramset, "," );
  if (num1 != nump)
    error(11,"problem with signature\n");

  newsig = "( ";
  xcnt = 0;

  for (j=1; j<= num1; j++) {

    if ( (j != calleridx) && (j != xhandleidx)) {
       # got to add this one to the new signature
      if ((j == 1) && (functype == "C")) {
        # do nothing, since we wanna filter this one out anyway since
	# we check later that this is a "const ObjectHandle &oh ..."
      } else {
	if (xcnt > 0) newsig = newsig ", ";
	newsig = newsig paramset[j];
	xcnt++;
      }
    }
    num2 = split( paramset[j], words, " " );

    if (j==1) split( paramset[j], firstarg, " " );
    paramset[j] = "";
    # now strip out the extra keywords and ref/ptr stuff..
    # got to keep the "*" for VAR_ARRAYS
    for (k=1; k <= num2; k++) {
      if (match( words[k], "^&"    ) > 0) continue;
      if (match( words[k], "^const") > 0) continue;
      if ((match( words[k], "^[*]" ) > 0) && (isarray[j] == 0)) continue;
      if (match(words[k], "^=") > 0) break; # initialization
      paramset[j] = paramset[j] " " words[k];
    }
    nameset[j] = words[k-1];
  }
  newsig = newsig " )";
  # Extract default parameter initializations from signature
  num2 = split(newsig, parts, " ");
  include = 1;
  newsig = "";
  for (k=1; k <= num2; k++){
    if (match( parts[k], "^=")) {
      include = 0;
    }
    if (include == 0 && match(parts[k],"^[,)]")) {
      include = 1;
    }
    if (include==1) {
      newsig = newsig " " parts[k];
    }
  }
  return newsig;
}

function declarePPCargs(rctype,is_out,OF)
{
#  printf("// %d %d %d %d %d %d\n",nump,hasinargs,hasoutargs,parcount[0],parcount[1],parcount[2]) > OF;

  if (hasinargs == 1) {
    printf("  struct __inargs_t {\n")                                     > OF;
#   printf("    PPC_InCore core;\n")                                      > OF;
    for (j=1;j<=nump;j++)
      if ((inoutspec[j] <= 1) && (calleridx != j) && (xhandleidx != j)) {   # in/inout
	printf("   %s;\n",paramset[j])                                    > OF;
      }
    if (require_vtablen_in == 1)
      printf("    uval __vtablen;\n")                                     > OF;
    if (size_sizearr_in > 0)
      printf("    uval __sizearr[%d];\n",size_sizearr_in)                 > OF;
    printf("  };\n")                                                      > OF;
  }

  if (hasoutargs == 1) {
    printf("  struct __outargs_t {\n")                                    > OF;
#   printf("    PPC_OutCore core;\n")                                     > OF;
    for (j=1;j<=nump;j++)
      if ((inoutspec[j] >= 1) && (calleridx != j) && (xhandleidx != j)) {   # out/inout
	printf("   %s;\n",paramset[j])                                    > OF;
      }
    if (size_sizearr_out > 0)
      printf("    uval __sizearr[%d];\n",size_sizearr_out)                > OF;
    printf("  };\n")                                                      > OF;
  }
  printf("\n")                                                            > OF;
}

function createStandardFunc()
{
  printf("TYPE_IMPL_TEMPLATE(%s,%s,INSTNAME,1);\n\n",TMclass,Mclass) > XF;
  printf("TYPE_IMPL_X_TEMPLATE(%s,%s);\n\n",xclass, mclass)		  > XF;
  printf("template<> COVTableEntry *%s<INSTNAME>::theVTBL = NULL;\n",TMclass)  > XF;
  printf("template<> COVTableEntry *%s<INSTNAME>::theMVTBL = NULL;\n",TMclass) > XF;

  printf("template<> char %s<INSTNAME>::static_buf[sizeof(%s<INSTNAME>)]\n",TMclass,TMclass)		  > XF;
  printf("\t__attribute__((aligned(sizeof(uval))));\n")			  > XF;

  printf("\n")								  > XF;
  printf("void\n")							  > XF;
  printf("%s<INSTNAME>::init()\n{\n",TMclass)				  > XF;
  if (parent_class != "Obj") {
      printf("    %s<INSTNAME>::init();\n",Meta_parent)			  > XF;
  }
  printf("    if (theMVTBL == NULL) {\n")                                 > XF;
  printf("        %s<INSTNAME> mobj(MetaObj::MAGIC); //tmp obj for vtbl\n",TMclass)> XF;
  printf("        theMVTBL = XBaseObj::GetFTable(&mobj);\n")              > XF;
  printf("    } else {                    \n")                            > XF;
  printf("        return; //init called twice\n")			  > XF;
  printf("    }\n")                                                       > XF;
  printf("    // now allocate a object handle for it                 \n") > XF;
  printf("    ObjectHandle xoh;                                      \n") > XF;
  printf("    XHandle xh;                                            \n") > XF;
  printf("                                                           \n") > XF;
  printf("    xh = DREFGOBJ(TheXHandleTransRef)->alloc(              \n") > XF;
  printf("            (ObjRef)GOBJ(TheProcessRef),                   \n") > XF;
  printf("            GOBJ(TheProcessRef),                           \n") > XF;
  printf("            0,0,theMVTBL,_NUM_METHODS_META%s);\n",toupper(class_name))  > XF;
  printf("    %s<INSTNAME> *self = new(static_buf) %s<INSTNAME>;\n",TMclass,TMclass) > XF;
  printf("                                                           \n") > XF;
  printf("    xoh.initWithMyPID(xh);                                 \n") > XF;
  printf("    DREFGOBJ(TheTypeMgrRef)->registerTypeHdlr(typeID(), xoh, (uval)self);\n") > XF;
  printf("}\n\n")                                                         > XF;

  printf("template <class T>\n")					  > XF;
  printf("SysStatusXHandle %s::createXHandle(ObjRef iobj,\n",mclass)      > XF;
  printf("           ProcessID procID,\n")                                > XF;
  printf("           AccessRights matched, AccessRights unmatched)\n{ \n")> XF;
  printf("     BaseProcessRef pref;\n")                                   > XF;
  printf("     SysStatus rc = DREFGOBJ(TheProcessSetRef)->\n")            > XF;
  printf("                    getRefFromPID(procID, pref);\n")            > XF;
  printf("     if(_FAILURE(rc)) return rc;\n")                            > XF;
  printf("     SysStatusXHandle xrc = createXHandle(iobj, pref,\n")       > XF;
  printf("                                matched, unmatched);\n")        > XF;
  printf("     return xrc;                                           \n") > XF;
  printf("                                                           \n") > XF;
  printf("}\n\n")                                                         > XF;

  printf("template <class T>\n")					  > XF;
  printf("SysStatusXHandle %s::createXHandle(ObjRef iobj,\n",mclass)      > XF;
  printf("           ProcessID procID,\n")                                > XF;
  printf("           AccessRights matched, AccessRights unmatched,\n")    > XF;
  printf("           uval userData)\n{ \n")> XF;
  printf("     BaseProcessRef pref;\n")                                   > XF;
  printf("     SysStatus rc = DREFGOBJ(TheProcessSetRef)->\n")            > XF;
  printf("                    getRefFromPID(procID, pref);\n")            > XF;
  printf("     if(_FAILURE(rc)) return rc;\n")                            > XF;
  printf("     SysStatusXHandle xrc = createXHandle(iobj, pref,\n")       > XF;
  printf("                          matched, unmatched, userData);\n")    > XF;
  printf("     return xrc;                                           \n") > XF;
  printf("                                                           \n") > XF;
  printf("}\n\n")                                                         > XF;

  printf("template <class T>\n")					  > XF;
  printf("SysStatusXHandle %s::createXHandle(ObjRef iobj, BaseProcessRef pref,\n",mclass)> XF;
  printf("           AccessRights matched, AccessRights unmatched)\n{ \n")> XF;
  printf("    if (theVTBL == NULL) {\n")                                  > XF;
  printf("        %s xobj; // create tmp object to get to vtbl\n",xclass) > XF;
  printf("        theVTBL = XBaseObj::GetFTable(&xobj);\n")               > XF;
  printf("    }\n")                                                       > XF;
  printf("                                                           \n") > XF;
  printf("    SysStatusXHandle xrc =                                  \n") > XF;
  printf("     DREFGOBJ(TheXHandleTransRef)->alloc(iobj,pref,     \n")    > XF;
  printf("                     matched,unmatched,theVTBL, \n")            > XF;
  printf("                     _NUM_METHODS_%s);\n",  toupper(class_name))    > XF;
  printf("    return xrc;\n")                                             > XF;
  printf("}\n\n")                                                         > XF;

  printf("template <class T>\n")					  > XF;
  printf("SysStatusXHandle %s::createXHandle(ObjRef iobj, BaseProcessRef pref,\n",mclass)> XF;
  printf("           AccessRights matched, AccessRights unmatched,    \n")> XF;
  printf("                                          uval userData)\n{ \n")> XF;
  printf("    if (theVTBL == NULL) {\n")                                  > XF;
  printf("        %s xobj; // create tmp object to get to vtbl\n",xclass) > XF;
  printf("        theVTBL = XBaseObj::GetFTable(&xobj);\n")               > XF;
  printf("    }\n")                                                       > XF;
  printf("                                                           \n") > XF;
  printf("    SysStatusXHandle xrc =                                  \n") > XF;
  printf("     DREFGOBJ(TheXHandleTransRef)->alloc(iobj,pref,     \n")    > XF;
  printf("                     matched,unmatched,theVTBL, \n")            > XF;
  printf("               _NUM_METHODS_%s, userData);\n",  toupper(class_name))> XF;
  printf("    return xrc;\n")                                             > XF;
  printf("}\n\n")                                                         > XF;

}

function createImplementation()
{
# run through all the functions and create an XObj implementation from

  printf("/* THIS FILE IS AUTOMATICALLY GENERATED ==> DO NOT MODIFY */\n\n") > XF;
  printf("#ifndef %s\n",xobjdefname)					  > XF;
  printf("#define %s\n",xobjdefname)					  > XF;
  printf("#ifndef INSTNAME\n")						  > XF;
  printf("#error define INSTNAME to specify target of template TplX%s\n",filename) > XF;
  printf("#endif //INSTNAME\n")						  > XF;
  printf("#define _IN_STUBGEN\n")                                         > XF;
  printf("#include <sys/ppccore.H>\n")                                    > XF;
  printf("#include <cobj/XHandleTrans.H>\n")                              > XF;
  printf("#include <sys/ProcessSet.H>\n")                                 > XF;

  close(sourcefilename);

  printf("#include <xobj/TplX%s.H> /* auto */\n",filename)                > XF;
  printf("#include <meta/TplMeta%s.H> /* auto */\n",filename)             > XF;
  printf("#include <meta/Meta%s.H> /* auto */\n",filename)                > XF;
  if (parent_class != "Obj") {
      printf("#include <tmpl/TplX%s.I> /* auto */\n",parent_class)        > XF;
  }
  printf("\n\n")                                                          > XF;
  printf("template class %s<INSTNAME>;\n",TMclass)			  > XF;
  printf("// generated valid method limits\n\n")                          > XF;

  while( ( getline tmp < methnumsfile ) == 1 ) {
    printf("%s\n", tmp)                                                   > XF;
  }
  close(methnumsfile);
  printf("\n\n")                                                          > XF;


}


function find_var_index(varname)
{
  for ( k=1 ; k<=nump ; k++)
    if (varname == nameset[k]) return(k);
#  warning(19,sprintf("<%s> not a parameter, assume constant",varname));
  return(-1);
}

{
  ##START##
  i=1;
  line++;

# read all the arguments for the next function

  vindex              = $(i++);
  mangledfunctionname = $(i++);
  class_name          = $(i++);
  parent_class        = $(i++);
  parent_signature    = $(i++);
  xaccdef             = $(i++);
  xaccmeth            = $(i++);
  funcn               = $(i++);
  rtype               = $(i++);
  class_signature     = $(i++);
  ppctype             = $(i++);
  functype            = $(i++);
  calleridx           = $(i++);
  xhandleidx          = $(i++);
  stabspec            = $(i++);
  nump                = $(i++);
  parstr              = $(i++);
  allarrayspec        = $(i++);


  if (class_name != lastclass ) {
    nclass = class_name;
    iclass =        nclass;
    iclass = "T";
    sclass = "Stub" nclass;
    xclass = "TplX"    nclass "<T>";
    mclass = "TplMeta" nclass "<T>";
    TMclass = "TplMeta" nclass;
    Mclass = "Meta" nclass;
    Xclass = "TplX" nclass;

    if (parent_class != "Obj") {
	stub_parent = "Stub" parent_class;
	xobj_parent = "TplX"    parent_class "<T>";
	meta_parent = "TplMeta" parent_class "<T>";
        Meta_parent = "TplMeta" parent_class;
    }else{
	stub_parent = "Stub" parent_class;
	xobj_parent = "X"    parent_class;
	meta_parent = "Meta" parent_class;
	Meta_parent = "Meta" parent_class;
    }

    nclass = "T";
  }

  if (line == 1) {
    doMETAandXOBJstuff( sourcefilename );
    createImplementation();
  }

  if (class_name != lastclass )
    createStandardFunc();

  lastclass = class_name;

#-----------------------------------------------------------------
# diguest the parameters and transform into easier representations
# to remember:
#-----------------------------------------------------------------

  split(stabspec, A_stabspec, ":");
  ioarrcnt[0]      = A_stabspec[1];
  ioarrcnt[1]      = A_stabspec[2];
  ioarrcnt[2]      = A_stabspec[3];
  size_sizearr_in  = A_stabspec[4];
  size_sizearr_out = A_stabspec[5];


  stabmgt_in  = (ioarrcnt[0] > 0) || (ioarrcnt[1] > 0);
  stabmgt_out = (ioarrcnt[1] > 0) || (ioarrcnt[2] > 0);

  split(parstr,       parspec,      " ");
  split(allarrayspec, arrayspecsep, " ");

  gotRCPAR = 0;
  parcount[0] = parcount[1] = parcount[2] = 0;
  require_vtablen_in = 0;
  hiddenArgs = 0;
  for ( j=1 ; j<=nump ; j++ ) {
    isarray[j]   = substr(parspec[j],1,1);
    prspec[j]    = substr(parspec[j],2,1);
    inoutspec[j] = substr(parspec[j],3,1);
    parcount[inoutspec[j]]++;
    if (prspec[j] == 1) dref[j]="*"; else dref[j]="";
    if ((prspec[j] == 1) && (isarray[j] == 0)) aref[j]="&"; else aref[j]="";
    if (isarray[j] > 1) {
      # this is a __CALLER_PID or an XHANDLE spec..
      # reduce the parcount[0] to correct
      hiddenArgs++;
      parcount[0]--;
      isarray[j] = isarray[j] - 2;
    }
    if (isarray[j] == 1) {
      split(arrayspecsep[j], arrayspec, ":");
      a_inlength[j]    = arrayspec[1];
      a_outlength[j]   = arrayspec[2];
      a_maxlen[j]      = arrayspec[3];
      a_spec_in[j]     = substr(arrayspec[4],1,1);
      a_spec_out[j]    = substr(arrayspec[4],2,1);
      a_spec_max[j]    = substr(arrayspec[4],3,1);
      a_stabin_idx[j]  = arrayspec[5];
      a_stabout_idx[j] = arrayspec[6];
      a_in_var_idx[j]  = 0;
      a_out_var_idx[j] = 0;
      if (inoutspec[j] == 0) require_vtablen_in=1; # always do require
    }
  }
  hasinargs = (parcount[0] > 0) || (parcount[1] > 0);
  hasoutargs = (parcount[1] > 0) || (parcount[2] > 0);

  stubsig = separateParams(class_signature,nump,paramset,nameset,firstarg,functype);

  # identify the indices of parameters used as length/max indication
  for ( j=1 ; j<=nump ; j++ ) {
    if (isarray[j] == 1) {
      if ((inoutspec[j] <= 1) && (a_spec_in[j] == IS_VAR_ARRAY)) {
	k = find_var_index(a_inlength[j]);
	if (k == -1) {
	  a_spec_in[j] = IS_CONST_ARRAY;  # assume constant
	} else {
	  a_in_var_idx[j] = k;
	  if ((inoutspec[k] == 2) || (isarray[k] == 1)) {
	    error(20,sprintf("length parameter<%s> for <%s> must be an in parameter",
			     a_inlength[j],nameset[j]));
	  }
	}
      }
      if ((inoutspec[j] >= 1) && (a_spec_out[j] == IS_VAR_ARRAY)) {
	k = a_out_var_idx[j] = find_var_index(a_outlength[j]);
	if (k == -1) {
	  if (a_outlength[j] == "__rc") {
	    if (gotRCPAR == 1)
	      error(20,sprintf("Only one __rc parameter allowed\n"));
	    else
	      gotRCPAR = 1;
	  } else {
	    error(21,sprintf("length parameter<%s> for <%s> is not a parameter",
			     a_outlength[j],nameset[j]));
	  }
	} else {
	  if ((inoutspec[k] == 0) || (isarray[k] == 1)) {
	    error(21,sprintf("length parameter<%s> for <%s> must be an out parameter",
			     a_outlength[j],nameset[j]));
	  }
	}
      }
      if ((inoutspec[j] >= 1) && (a_spec_max[j] == IS_VAR_ARRAY)) {
	k = find_var_index(a_maxlen[j]);
	if ( k == -1) {
	  a_spec_max[j] = IS_CONST_ARRAY;
	} else {
	  a_max_var_idx[j] = k;
	  if ((inoutspec[k] == 2) || (isarray[k] == 1)) {
	    error(21,sprintf("length parameter<%s> for <%s> must be an in parameter",
			     a_maxlen[j],nameset[j]));
	  }
	}
      }
    }
  }

  if ((ppctype != "S") && (parcount[0]+hiddenArgs != nump)) {
    error(13,sprintf("Non-synchronous PPC only allows __in parameters\n"));
  }
  if (functype == "C") {
    # verify that the arg is "__out ObjectHandle &oh"
    if ((nump < 1) ||
	(inoutspec[1] != 2) ||
	(firstarg[1] != "ObjectHandle") ||
	(firstarg[2] != "&") ||
	((firstarg[3] != "oh") && (firstarg[3] != "__genname_0")))
      {
	error(12,sprintf("%s::%s%s first arg must be (__out ObjectHandle & oh) %s\n",
			 iclass,funcn,class_signature,inoutspec[1]));
      }
  }

#--------------------------------------------------------------
# XOBJ and META
#--------------------------------------------------------------


  printf("template <class T>\n")					  > XF;
  if (functype == "V")
    printf("SysStatus %s::%s(CommID callerID) {\n",xclass,mangledfunctionname)         > XF;
  else
    printf("SysStatus %s::%s(CommID callerID) {\n",mclass,mangledfunctionname)         > XF;

  declarePPCargs(type,1,XF);

  printf("  SysStatus __ppc_rc;\n")                                       > XF;
  if (hasinargs == 1)
    printf("  __inargs_t  __inargs;\n")                                   > XF;
  if (hasoutargs == 1)
    printf("  __outargs_t __outargs;\n")                                  > XF;
  if (stabmgt_out == 1)
    printf("  uval __len,__alen;\n")                                      > XF;

    # do the authentication based on what the method specification indicated

  authenticate_err_exit = 0;

  if (functype == "V") {
    printf("  const AccessRights rights = ")                              > XF;
    pos = match(xaccmeth,"[(].*[)]");
    acll = substr(xaccmeth,pos+1,RLENGTH-2);
    num1 = split( acll, acclist, ":");
    for (i = 1; i <= num1 ; i++) {
      printf("%s::%s",mclass,acclist[i])                                  > XF;
      if (i<num1) printf(" | ")                                           > XF;
    }
    printf(";\n")                                                         > XF;
    printf("  if (((SysTypes::PID_FROM_COMMID(callerID) == ")             > XF;
    printf(                                     "this->__matchPID) &&\n")       > XF;
    printf("                    ((rights & this->__mrights) == rights)) ||\n")  > XF;
    printf("        ((rights & this->__urights) == rights))\n")           > XF;
  }
  printf("  {\n")                                                         > XF;

  if (hasinargs == 1)
    printf("    memcpy(&__inargs,PPCPAGE_DATA,sizeof(__inargs));\n")      > XF;

  # Xobj needs a bit different handling of the following case then stub

  printf("\n    /* STABMGT = %d %d */\n",stabmgt_in,stabmgt_out)          > XF;

  #------------------------------#
  #-- SYMBOL TABLE MANAGEMENT ---#
  #------------------------------#

  checked_overflow     = 0;
  checked_ppc_overflow = 0;
  if (stabmgt_in == 1) {
    # we know this is __in parameters only, so we can copy in one chunk
    # also know that all inoutspec must be == 0
    printf("    char* __stab = PPCPAGE_DATA + sizeof(__inargs);\n")       > XF;
    printf("    char* __lstab;\n")                                        > XF;
    if (require_vtablen_in == 1)
      printf("    uval __ptrdiff;\n")                                     > XF;
    printf("    /* compute variable sized max fields */\n")               > XF;
    varlenstr = "";
    for (j=1;j<=nump;j++) {
      if ((inoutspec[j] >= 1) && (isarray[j] == 1)) {
	if (a_spec_max[j] == IS_CONST_ARRAY) lenstr = "";
	else                                 lenstr = "__inargs.";
	varlenstr = varlenstr " + SGALIGN(" lenstr a_maxlen[j] "*sizeof(*__outargs." nameset[j] "))";
      }
    }
    if (varlenstr != "") {
      varlenstr = substr(varlenstr,4); # get ride of first +
      printf("    uval __reserve = %s;\n",varlenstr)                      > XF;
      printf("    FITS_PPCPAGE(sizeof(__outargs) + __reserve);\n")        > XF;
      checked_ppc_overflow = 1;
      if (require_vtablen_in == 1)
	varlenstr = "__inargs.__vtablen + __reserve";
      else
	varlenstr = "__reserve";
    } else {
      if (require_vtablen_in == 1)
	varlenstr = "__inargs.__vtablen";
      else
        error("ERROR<gentmp.awk> #100");
    }
    printf("    ALLOCA(__lstab,%s);\n",varlenstr)                         > XF;

    #-------------------------------------------------------------------------
    # now copy variable sized arrays which are input args (strings and arrays)
    #-------------------------------------------------------------------------

    if (require_vtablen_in == 1) {
      printf("    __ptrdiff = __lstab-__stab;\n")                         > XF;
      printf("    memcpy(__lstab,__stab,__inargs.__vtablen);\n")          > XF;
      for (j=1;j<=nump;j++)
	if ((inoutspec[j] == 0) && (isarray[j] == 1))
	  printf("    SGX_ADJ_PTR(%s);\n",nameset[j])                     > XF;
      printf("    __lstab += SGALIGN(__inargs.__vtablen);\n")             > XF;
    }

    #-------------------------------------------------------------------------
    # now copy variable sized arrays which are inout args (strings and arrays)
    #-------------------------------------------------------------------------

    for (j=1;j<=nump;j++) {
      if ((inoutspec[j] == 1) && (isarray[j] == 1)) {
	if (a_spec_max[j] == IS_VAR_ARRAY) maxlenpref = "__inargs.";
	else                               maxlenpref = "";
	if (a_spec_in[j] == IS_VAR_ARRAY)  inlenpref  = "__inargs.";
	else                               inlenpref  = "";
	checked_overflow = 1;
	if      (a_spec_in[j] == IS_STR_ARRAY) {
	  printf("    SGX_GET_STR(%s,%d,%s%s,%d);\n",nameset[j],
		 a_stabin_idx[j],maxlenpref,a_maxlen[j],j)                > XF;
	}
	else if (a_spec_in[j] == IS_CONST_ARRAY) {
	  printf("    SGX_GET_FARRAY(%s,%s%s,%s%s,%d);\n",nameset[j],inlenpref,
		 a_inlength[j],maxlenpref,a_maxlen[j],j)                  > XF;
	}
	else if (a_spec_in[j] == IS_VAR_ARRAY) {
	  printf("    SGX_GET_VARRAY(%s,%s%s,%s%s,%d);\n",nameset[j],inlenpref,
		 a_inlength[j],maxlenpref,a_maxlen[j],j)                  > XF;
	}
      }
    }

  }

  # Do PPC reset for security even if there are no arguments
  printf("    RESET_PPC();\n")                                          > XF;

  # before calling we have to copy the INOUT arguments other than strings
  # and we have to assign OUT arguments to their slot on the LSTAB

  if ((stabmgt_in == 0) && (stabmgt_out == 1)) {
    # we for sure have not allocated LSTAB so do it now
    printf("    char* __lstab;\n")                                        > XF;
    printf("    /* compute variable sized max fields */\n")               > XF;
    varlenstr = "";
    for (j=1;j<=nump;j++) {
      if ((inoutspec[j] >= 1) && (isarray[j] == 1)) {
	if (a_spec_max[j] == IS_CONST_ARRAY) lenstr = "";
	else                                 lenstr = "__inargs.";
	varlenstr = varlenstr " + SGALIGN(" lenstr a_maxlen[j] "*sizeof(*__outargs." nameset[j] "))";
      }
    }
    printf("    uval __reserve = sizeof(__outargs)%s;\n",varlenstr)       > XF;
    printf("    FITS_PPCPAGE(__reserve);\n")                              > XF;
    printf("    ALLOCA(__lstab,__reserve);\n")                            > XF;
    checked_ppc_overflow = 1;
  }
  for (j=1;j<=nump;j++) {
    if ((inoutspec[j] == 1) && (isarray[j] == 0))
      printf("    __outargs.%s = __inargs.%s;\n",nameset[j],nameset[j])   > XF;
    if ((inoutspec[j] == 2) && (isarray[j] == 1)) {
      if (a_spec_max[j] == IS_VAR_ARRAY) maxlenpref = "__inargs.";
      else                               maxlenpref = "";
      if      (a_spec_in[j] == IS_STR_ARRAY) {
	printf("    SGX_RES_STR(%s,%s%s);\n",
	       nameset[j],maxlenpref,a_maxlen[j])                         > XF;
      }
      else if (a_spec_in[j] == IS_CONST_ARRAY) {
	printf("    SGX_RES_FARRAY(%s,%s%s);\n",
	       nameset[j],maxlenpref,a_maxlen[j])                         > XF;
      }
      else if (a_spec_in[j] == IS_VAR_ARRAY) {
	printf("    SGX_RES_VARRAY(%s,%s%s);\n",
	       nameset[j],maxlenpref,a_maxlen[j])                         > XF;
      }
    }
  }
  if ((stabmgt_in == 0) && (stabmgt_out == 0)) {
#  printf("    FITS_PPCPAGE(sizeof(__outargs));\n")                       > XF;
#      checked_ppc_overflow = 1;
  }

#-----------------------------------
# now the actual function invocation
#-----------------------------------

  if (functype == "V")
    printf("    __ppc_rc = DREF((%s**)this->__iobj)->%s( ",iclass,funcn)  > XF;
  else
    printf("    __ppc_rc = %s::%s( ",iclass,funcn)                        > XF;

  for (j=1;j<=nump;j++) {
    if (j == calleridx) {
      printf("SysTypes::PID_FROM_COMMID(callerID)")                       > XF;
    } else if (j == xhandleidx) {
      printf("this->getXHandle()")                                        > XF;
    } else {
      if (inoutspec[j] == 0)
	printf("%s__inargs.%s",aref[j],nameset[j])                        > XF;
      else
	printf("%s__outargs.%s",aref[j],nameset[j])                       > XF;
    }
    if (j<nump) printf(", ")                                              > XF;
  }
  printf(" );\n")                                                         > XF;
  if (hasoutargs == 1)
    printf("    SET_PPC_LENGTH(sizeof(__outargs));\n")                    > XF;

#-------------------------------------------
# do we have to return any string parameters
#-------------------------------------------

  if (stabmgt_out == 1) {
    #-------------------------------------------------------------------------
    # now copy variable sized arrays which are out args (strings and arrays)
    #-------------------------------------------------------------------------

    if (stabmgt_in == 0)
      printf("    char *__stab;\n")                                       > XF;
    printf("    __stab = PPCPAGE_DATA + sizeof(__outargs);\n")            > XF;
    for (j=1;j<=nump;j++) {
      if ((inoutspec[j] >= 1) && (isarray[j] == 1)) {
	if      (a_spec_out[j] == IS_STR_ARRAY) {
	  printf("    SGX_PUT_STR(%s,%d);\n",nameset[j],a_stabout_idx[j]) > XF;
	}
	else if (a_spec_out[j] == IS_CONST_ARRAY) {
	  printf("    SGX_PUT_FARRAY(%s,%s);\n",nameset[j],a_outlength[j])> XF;
	}
	else if (a_spec_out[j] == IS_VAR_ARRAY) {
	  if (a_outlength[j] == "__rc") outlenpref = "__ppc_rc";
	  else                          outlenpref = "__outargs." a_outlength[j];
	  printf("    SGX_PUT_VARRAY(%s,%s);\n",nameset[j],outlenpref)    > XF;
	}
      }
    }
  }

  # now that all pointers are correct we can copy __outargs
  if ((ppctype == "S") && (hasoutargs == 1))
    printf("    COPY_TO_PPC_PAGE(&__outargs,sizeof(__outargs));\n")       > XF;
  printf("    return __ppc_rc;\n")                                        > XF;

  if (functype == "V") {
    printf("  } else {\n")                                                > XF;

##just for debugging
    printf("    RESET_PPC(); //PPC page needed by cprintf\n")		  > XF;
    printf("    cprintf(\"CAN'T MATCH CommID in ")                        > XF;
    printf(              "%s::%s\\n\");\n",xclass,mangledfunctionname)    > XF;
    printf("    cprintf(\" calledPID 0x%%lx")                             > XF;
    printf(              " callerPID 0x%%lx")                             > XF;
    printf(              " matchPID 0x%%x")                               > XF;
    printf(              " rights 0x%%lx")                                > XF;
    printf(              " mrights 0x%%lx")                               > XF;
    printf(              " urights 0x%%lx")                               > XF;
    printf(              "\\n\",\n")                                      > XF;
    printf("            DREFGOBJ(TheProcessRef)->getPID(),\n")            > XF;
    printf("            SysTypes::PID_FROM_COMMID(callerID),")            > XF;
    printf(              "this->__matchPID,\n")                           > XF;
    printf("            rights,this->__mrights,this->__urights);\n")      > XF;

##just for debugging
    if (hasoutargs == 1) {
      printf("    __ppc_rc = _SERROR(0,0,EPERM);\n")                      > XF;
      printf("    goto __exit_xobj;\n")                                   > XF;
      authenticate_err_exit = 1;
    } else {
      printf("    return _SERROR(0,0,EPERM);\n")                          > XF;
    }
  }
  printf("  }\n")                                                         > XF

  if ((checked_ppc_overflow == 1)) {
    printf("__ppc_overflow: /* label for PPC overflow */\n")              > XF;
    printf("  __ppc_rc = _SERROR(0,3,ENOSPC);\n")                         > XF;
  }
  if ((checked_overflow == 1) || (authenticate_err_exit == 1))
    printf("__exit_xobj: /* exit for buffer overflow and authenticate errors */\n")        > XF;
  if ((checked_overflow == 1) || (checked_ppc_overflow == 1) ||
      (authenticate_err_exit == 1)) {
    printf("  RESET_PPC_LENGTH(); /* avoids assert in SET_PPC_LENGTH*/\n")> XF;
    printf("  SET_PPC_LENGTH(sizeof(__outargs));\n")                      > XF;
    printf("  COPY_TO_PPC_PAGE(&__outargs,sizeof(__outargs));\n")         > XF;
    printf("  return __ppc_rc;\n")                                        > XF;
  }
  if (functype != "V") {
    printf("  (void)callerID;\n")                                         > XF;
  }
  printf("};\n\n")                                                        > XF;
}

END{
  if (class_name != "") {
    printf("#endif /* %s */\n", xobjdefname)				  > XF;
  }
}
