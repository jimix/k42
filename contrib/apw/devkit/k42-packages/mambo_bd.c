/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * A simple stub written by Livio Soares.
 *
 *****************************************************************************/

#include <linux/module.h>

int __init mbd_init(void)
{
	return 0;
}

module_init(mbd_init);
