# ######################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
#  $Id: genmangle.awk,v 1.11 2002/03/06 20:41:22 jimix Exp $
# ######################################################################

# generate a C++ compilable file in order to extract mangling
# information.  Almost all processing is done at the end.
# we first read in the input file 

# inputs to this function are:
#   the input file is the output of cpp5 -- semicolon delimited fields
#                    of the syntactic entities of the source file
#   -v filename -- base name of the source file
#   -v dirname -- current directory
#   -v sourcefilename -- complete name (and path) of source file

BEGIN{
  FS = ";" ;
  # so we can copy code
  inclstart = sprintf("^#ifndef[ ]+EXPORT_%s([ ]+|$)",toupper(filename));
  inclend   = sprintf("^#endif[ ]+.*EXPORT_%s",        toupper(filename));
  incldef   = sprintf("^#define[ ]+EXPORT_%s([ ]+|$)",toupper(filename));
}

# ######################################################################
#
#  main loop -- read input and save in internal arrays
#
{ 
  i=1;
  line++;
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
}

# ######################################################################
#
#  functions to support the output generation (done in END processing)

function error( errnum, string )
{
  filename = "/tmp/awk" rand();
  printf("Error<genmangle>%d: %s\n",errnum,string) > filename;
  close(filename);
  system( "chmod 777 " filename );
  system( "cat " filename " 1>&2 " );
  system( "rm " filename );
  exit 1;
}


function copyCode(  )
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
	printf("%s\n",tmp);
      } else if ( match(tmp,incldef ) > 0) {
	got_define = 1;
      }
    }
    if (copy_active == 1)
      printf("%s\n",tmp);
  }
  close(sourcefilename);
  printf("\n");
  if (copy_active == 1)
    error(22,sprintf("missing >#endif // EXPORT_%s\n",toupper(filename)));
  if ((got_define == 0) && (got_copy == 1))
    error(23,sprintf("missing >#define EXPORT_%s\n",toupper(filename)));
}

function doStandardStuff( file )
{
  printf("/* Automatically generated file by genmangle.awk from %s */\n", file);
  printf("#include \"sys/sysIncs.H\"\n\n");
  copyCode( );
}

function build__xclass( pclass )
{
  printf("\n\nclass __%s ", xclass);
  printf("{\npublic:\n");
 }

function build__xmethods( from , to )
{
  for( i = from; i <= to; i++ ) {
    printf("\t%s %s %s ;\n", rtype[i], funcn[i], class_signature[i]);
  }
 }

function build__variables_with_sig( from, to )
{
  printf("};\n\n");
  for( i = from; i <= to; i++ ) {
    if( 0 ) {  # this is obsolete but good to have around for extensions
      if( functype[i] == 1 ) {
	printf("%s (%s::*__Virtual__%s__Var%d)%s = &%s::%s;\n",
	       rtype[i], xclass, xclass, i, class_signature[i], xclass, funcn[i] );
      } else {
	printf("%s (Meta%s::*__Virtual__Meta%s__Var%d)%s = &Meta%s::%s;\n",
	       rtype[i], xclass, xclass, i, class_signature[i], xclass, funcn[i] );
      }
    }
    # this case sets the type of the variable to the type of the function
    printf("%s (__%s::*__%s__Var%d)%s = &__%s::%s;\n",
	   rtype[i], xclass, xclass, i, class_signature[i], xclass, funcn[i] );
    printf("\n");
  }
#  printf("void (Dummy%s::*__VAR_SIZE_VIRTUAL__%s)() = &Dummy%s::__SIZE_VIRTUAL__;\n", xclass, xclass, xclass);
#  printf("void (DummyMeta%s::*__VAR_SIZE_VIRTUAL__Meta%s)() = &DummyMeta%s::__SIZE_VIRTUAL__;\n", xclass, xclass, xclass);

}

function remove_parameter_defaults( from, to )
{
  for( i = from; i <= to; i++ ) {
    # strip parens
    sub(/\(/, "", class_signature[i])
      sub(/\)/, "", class_signature[i])

    sig = ""
    # split arguments into array 'args'
      arg_size = split(class_signature[i], args,",");

    for( j = 1; j <= arg_size; j++){
      if( j>1) sig = sig ",";
      $0 = args[j];

      if(/=/){
        split($0, sides,"=");
        sig = sig sides[1];
      }
      else{
        sig = sig args[j];
      }
    }
    class_signature[i] = "(" sig ")";
  }
}


# ######################################################################
#
#  END processing:  we have read in the entire input file and put
#                   it into a set of arrays (one for each field).
#

END{

  # first count how many classes there are and which lines
  # start and end each class.  We assume at least one class.
  numcls=1;
  clsstart[numcls] = 1;
  for( i = 2; i <= line; i++ ) {
    if (class_name[i] != class_name[i-1] ) {
      clsend[numcls] = i-1;
      numcls++;
      clsstart[numcls] = i;
    }
  }
  clsend[numcls] = line;

  doStandardStuff(sourcefilename);

  # for each class, generate a new class and its variables 
  for ( k = 1; k <= numcls; k++) {
    iclass = class_name[clsstart[k]];
    xclass = "X" iclass;
    build__xclass(parent_class[k]);
    build__xmethods(clsstart[k],clsend[k]);
    remove_parameter_defaults(clsstart[k],clsend[k]);
    build__variables_with_sig(clsstart[k],clsend[k]);
  }
}


