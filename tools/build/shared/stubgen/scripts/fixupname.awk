# ############################################################################
# K42: (C) Copyright IBM Corp. 2000.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: fixupname.awk,v 1.8 2003/06/23 18:48:18 timvail Exp $
# ############################################################################
{
     name = $NF;
     idx1 = match( name, /__[0-9]*__/ );   # old: find "xxx__n class yyy"
     idx2 = match( name, /^_ZN[0-9]*__/ ); # new: find "_ZN class xxx yyy"
     if (idx1 != 0)
       {
	 # old style name mangling:  method __ class parameters
	 # the class is a name which will be of the form: n name
	 # where n is the number of characters in the name, and
	 # for our case, name is of the form __class

	 firstpart = substr( name, 1, idx1-1 );    # firstpart is "xxx"
	 len = RLENGTH - 4;                        # subtract the __ __ part
	 idx1 = idx1 + 2;                          # skip past the first __
	 num = substr( name, idx1, len );          # get the number n

         # the lastpart starts at the number idx1+len (which skips the __n
         # part -- idx1 is the __, and len is the length of the number n --
         # and then skips the number of characters in n, which then skips
         # the class name
	 lastpart = substr( name, idx1+len+num, length(name) - (idx1+len+num)+1);
	 print firstpart "_" lastpart;
       }
     else if (idx2 != 0)
       {
         # new style name mangling:  _Z N class method parameters 
	 # the class is a name which will be of the form: n name
	 # where n is the number of characters in the name.
	 len = RLENGTH - 5;                        # subtract the _ZN part
	 idx2 = idx2 + 3;                          # skip past the _ZN
	 num = substr( name, idx2, len );          # get the number n

         # the name starts at the number idx2+num 
	 idx1 = substr( name, idx2+len+num, length(name)-(idx2+len+num)+1);

	 idx2 = match( idx1, /[0-9]*/ );          # get just the name; skip n

	 idx2 = idx2 + RLENGTH;
	 lastpart = substr( idx1, idx2, length(idx1)-idx2);
	 print "_" lastpart;
       }
     else 
       {
	 # name does not match old or new -- skip it.
	 # should we print an error message?
	 print "*** ERROR -- extra name: $1";
       }
}
