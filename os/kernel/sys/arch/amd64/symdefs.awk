# *********************************************************************
# K42: (C) Copyright IBM Corp. 2001.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the license along with K42; see the file
# LICENSE.html in the top-level directory for more details.
#
# $Id: symdefs.awk,v 1.1 2001/07/24 22:44:58 pdb Exp $
# *********************************************************************

BEGIN{ printf("#include \"asmConstants.H\"\n\n"); }

{
    printf("#ifndef %s_SIZE\n#define %s_SIZE 8\n#endif\n", $1,$1);
    printf(".globl %s\n", $1);
    printf(".size %s, %s_SIZE\n", $1, $1);
    printf("%s = %s\n", $1, $2);
    printf("\n");
}

