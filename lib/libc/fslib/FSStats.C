/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSStats.C,v 1.4 2003/08/02 21:28:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <fslib/fs_defines.H>

#ifdef GATHERING_STATS
#include <sys/sysIncs.H>
#include "FSStats.H"

void
FSStats::printStats()
{

    err_printf("lookupSuccess is %ld, lookupFailure is %ld\n",
	       stats[LOOKUP_SUCCESS], stats[LOOKUP_FAILURE]);
    err_printf("openOp is %ld, openOpRdonly is %ld\n",
	       stats[OPEN], stats[OPEN_RDONLY]);
    err_printf("About file clients created:\n");
    err_printf("\tLAZY_INIT  %ld\n", stats[CLIENT_CREATED_LAZY_INIT]);
    err_printf("\tNON_SHARED  %ld\n", stats[CLIENT_CREATED_NON_SHARED]);
    err_printf("\tSHARED  %ld\n", stats[CLIENT_CREATED_SHARED]);
    err_printf("\tFIXED_SHARED  %ld\n", stats[CLIENT_CREATED_FIXED_SHARED]);
    err_printf("\ttotal  %ld\n", stats[CLIENT_CREATED]);
    err_printf("About file clients changes\n"
	       "\tSwitch operations %ld\n"
	       "\tBecome NON_SHARED %ld\n"
	       "\tBecome SHARED %ld\n",
	       stats[CLIENT_SWITCH],
	       stats[CLIENT_BECAME_NON_SHARED],
	       stats[CLIENT_BECAME_SHARED]);
    err_printf("dups is %ld\n", stats[DUP]);
    err_printf("number of registerCallBack for switching usetype: %ld\n",
	       stats[REGISTER_CALLBACK]);
    err_printf("number of ackUseType: %ld\n", stats[ACK_USETYPE]);
    err_printf("number of _getLengthOffset invocations: %ld\n",
	       stats[LENGTH_OFFSET]);
    err_printf("max size for write operation when shared: %ld\n",
	       stats[MAX_WRITE_SIZE_SHARED]);
    err_printf("\tnumber of times: %ld\n", nb_max_write_shared);
    err_printf("\tsecond max is %ld\n", second_max_write_shared);
    err_printf("max size for write operation when non-shared: %ld\n",
	       stats[MAX_WRITE_SIZE_NON_SHARED]);
    err_printf("Stat for file sizes at open:\n"
	       "\t< 3k %ld\n\t< 10k %ld\n\t< 100k %ld\n\t< 1M %ld\n"
	       "\t>= 1M %ld\n", openSizes[LESS_3K],
	       openSizes[LESS_10K], openSizes[LESS_100K],
	       openSizes[LESS_1M], openSizes[NOT_LESS_1M]);
    err_printf("Stat for file sizes at close time:\n"
	   "\t< 3k %ld\n\t< 10k %ld\n\t< 100k %ld\n\t< 1M %ld\n"
	       "\t>= 1M %ld\n", closeSizes[LESS_3K],
	       closeSizes[LESS_10K], closeSizes[LESS_100K],
	       closeSizes[LESS_1M],  closeSizes[NOT_LESS_1M]);
}

void
FSStats::initStats()
{
    statLock.init();
    memset(openSizes, 0, sizeof(openSizes));
    memset(closeSizes, 0, sizeof(closeSizes));
    memset(stats, 0, sizeof(stats));
    nb_max_write_shared = 0;
    second_max_write_shared = 0;
    err_printf("all counters for k42 FS stat have been zeroed\n");
}

/* static */ void
FSStats::updateSizeArray(uval array[], uval size)
{
    if (size < 0xc00) {
	array[LESS_3K]++;
    } else if (size < 0x2800) {
	array[LESS_10K]++;
    } else if (size < 0x19000) {
	array[LESS_100K]++;
    } else if (size < 0x100000) {
	array[LESS_1M]++;
    } else {
	array[NOT_LESS_1M]++;
    }
}

void
FSStats::incStat(StatType type, uval extra_arg /* = 0 */)
{
    switch (type) {
    case LOOKUP_FAILURE:
    case LOOKUP_SUCCESS:
    case CLIENT_CREATED:
    case CLIENT_CREATED_LAZY_INIT:
    case CLIENT_CREATED_NON_SHARED:
    case CLIENT_CREATED_SHARED:
    case CLIENT_CREATED_FIXED_SHARED:
    case CLIENT_BECAME_NON_SHARED:
    case CLIENT_BECAME_SHARED:
    case CLIENT_SWITCH:
    case OPEN:
    case OPEN_RDONLY:
    case DUP:
    case REGISTER_CALLBACK:
    case ACK_USETYPE:
    case LENGTH_OFFSET:
	AtomicAdd(&stats[type], 1);
	break;
    case OPEN_SIZE:
	updateSizeArray(openSizes, extra_arg);
	break;
    case CLOSE_SIZE:
	updateSizeArray(closeSizes, extra_arg);
	break;
    case MAX_WRITE_SIZE_NON_SHARED:
    case MAX_WRITE_SIZE_SHARED:
	statLock.acquire();
	if (extra_arg > stats[type]) {
	    second_max_write_shared = stats[type];
	    stats[type] = extra_arg;
	    nb_max_write_shared = 1;
	} else if (extra_arg == stats[type]) {
	    nb_max_write_shared++;
	}
	statLock.release();
	break;

    default:
	tassertMsg(0, "invalid argumente type %ld\n", (uval) type);
    }
}
#endif // #ifdef GATHERING_STATS
