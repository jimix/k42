/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: modifyBootParms.C,v 1.4 2005/06/28 19:42:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: program to modify the boot parameters
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/systemAccess.H>
#include <stub/StubKBootParms.H>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stub/StubFRKernelPinned.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

#define BOOT_DATA_SIZE 4095

void *GetSharedMemory(uval &kaddr, uval size)
{
    SysStatus rc;
    ObjectHandle frOH;
    uval uaddr;

    printf("Trying to get %ld bytes of shared memory\n", size);
    rc = StubFRKernelPinned::_Create(frOH, kaddr, size);
    if (_FAILURE(rc)) {
	printf("FRKernelPinned::_Create() failed\n");
	exit(1);
    }

    /* Create a Region to allow the process to reference the memory */
    rc = StubRegionDefault::_CreateFixedLenExt(uaddr, BOOT_DATA_SIZE, 0,
					       frOH, 0,
					       AccessMode::writeUserWriteSup,
					       0, RegionType::UseSame);

    if (_FAILURE(rc)) {
	printf("_CreateFixedLen() failed: 0x%016lx", rc);
	exit(1);
    }
    return (void *)uaddr;
}

char *GetParameters(uval *bootDataSize)
{
    uval kaddr;
    char *bootData;
    SysStatus rc;

    bootData = (char *)GetSharedMemory(kaddr, BOOT_DATA_SIZE);

    rc = StubKBootParms::_GetAllParameters(kaddr, BOOT_DATA_SIZE,
					   bootDataSize);
    if (_FAILURE(rc)) {
	printf("BOOT_DATA_SIZE define is too small to update boot parameters\n"
	       "Increase it and recompile this program %ld\n", rc);
	exit(1);
    }

    printf("Recieved %ld bytes from StubKBootParms::_GetAllParameters\n",
	   *bootDataSize);
    return bootData;
}


void ShowHelp()
{
    printf("Usage: modifyBootParms [-h] [-s] [-u parameter value] [-f file]\n"
	   " -h                       Help \n"
	   " -s                       Show current settings\n"
	   " -p <parameter>           Parameter\n"
	   " -v <value>               Value\n"
	   " -d                       Delete parameter\n"
	   " -u                       Update paremeter value\n"
	   " -f <file>                Replace with file\n");
   exit(1);
}

void ShowSettings(char *parameter)
{
    char *bootData;
    uval bootDataSize;

    bootData = GetParameters(&bootDataSize);

    for (uval32 i=0; i<bootDataSize; i++) {
	if (bootData[i]==0) {
	    printf("\n");
	} else {
	    printf("%c", bootData[i]);
	}
    }
}

sval FindKBootParmsData(char *input_data, uval length, uval *dataLength)
{
    uval32 index, numParts;
    uval32 partLength;

    numParts = *(uval32 *)input_data;
    numParts = ntohl(numParts);
    index = sizeof(uval32);

    printf("Looking for KBootParms section in file (%d parts)\n", numParts);

    while (index < length) {
	partLength = *((uval32 *)(input_data+index));
	printf("Length of part is %d\n", partLength);
	if (strncmp(input_data+index+sizeof(uval32),
		    "KBootParms", strlen("KBootParms")+1) == 0) {
	    *dataLength = partLength;
	    return index;
	}

	/* Iterate to next section */
	index += partLength;
    }

    return -1;
}


void UpdateParameters(char *dataBlock, uval length)
{
    uval kaddr;
    char *data;
    data = reinterpret_cast<char *>(GetSharedMemory(kaddr, length));

    memcpy(data, dataBlock, length);

    printf("About to send update parameter data\n");
    if (_FAILURE(StubKBootParms::_UpdateParameters(kaddr))) {
	printf("Updating parameters failed\n");
	exit(1);
    }
}

void UpdateThroughFile(char *filename)
{
    FILE *file;
    struct stat file_info;
    char *update_data;
    sval kbootparm_offset;
    uval kbootparm_length;

    if (stat(filename, &file_info)!=0) {
	perror("Could not state update file\n");
	exit(1);
    }

    file = fopen(filename, "r");
    if (file == NULL) {
	perror("Could not open update file");
	exit(1);
    }

    if ( (update_data = reinterpret_cast<char *>(malloc(file_info.st_size)))
	 == NULL ) {
	perror("Malloc failed\n");
	exit(1);
    }

    if (fread(update_data, 1, file_info.st_size, file)
	!= (unsigned int) file_info.st_size) {
	perror("Failed to read update file\n");
	exit(1);
    }

    kbootparm_offset = FindKBootParmsData(update_data, file_info.st_size,
					  &kbootparm_length);
    if (kbootparm_offset<0) {
	printf("Could not find KBootParms section in update file\n");
	exit(1);
    }

     UpdateParameters(update_data+kbootparm_offset, kbootparm_length);

}

void UpdateSingleParameter(char *parameter, char *value, bool deleteParm)
{
    char *existingData;
    uval bootDataSize;
    char *newData = NULL;
    uval newDataLength = 0;
    char *oldpar, *oldval;
    uval index;
    uval newDataIndex;
    bool replaceParameter = false;
    char *insertVal = NULL;
    bool skip;

    existingData = GetParameters(&bootDataSize);

    newDataLength = strlen("KBootParms") + sizeof(uval32) + 1;
    newData = reinterpret_cast<char *>(malloc(newDataLength));

    if (newData == NULL) {
	printf("Failed to malloc a few bytes\n");
    }

    strcpy(newData+sizeof(uval32), "KBootParms");
    newDataIndex = newDataLength;
    index = 0;
    while (index < bootDataSize) {
	oldpar = existingData + index;
	oldval = existingData + index + strlen(oldpar) + 1;
	printf("Found %s with value %s\n", oldpar, oldval);

	skip = false;
	if (strcmp(oldpar, parameter) == 0) {
	    // Found existing parameter
	    if (deleteParm) {
		skip = true;
	    } else {
		insertVal = value;
		replaceParameter = true;
	    }
	} else {
	    insertVal = oldval;
	}

	if (!skip) {

	    newDataLength += strlen(oldpar) + strlen(insertVal) + 2;
	    newData = reinterpret_cast<char *>(realloc(newData, newDataLength));
	    if (newData == NULL) {
		printf("Failed to realloc %ld bytes\n", newDataLength);
		exit(1);
	    }

	    newDataIndex +=
		sprintf(newData+newDataIndex, "%s=%s", oldpar, insertVal) + 1;
	}
	index += strlen(oldpar) + strlen(oldval) + 2;
    }

    if (!replaceParameter && !deleteParm) {
	// We didn't replace the parameter as it doesn't already exist
	// so we make an addition
	newDataLength += strlen(parameter) + strlen(value) + 2;
	newData = reinterpret_cast<char *>(realloc(newData, newDataLength));
	if (newData == NULL) {
	    printf("Failed to realloc %ld bytes\n", newDataLength);
	    exit(1);
	}
	sprintf(newData+newDataIndex, "%s=%s", parameter, value);
    }
    *((uval32 *)newData) = newDataLength;

    UpdateParameters(newData, newDataLength);

}

int main(int argc, char *argv[])
{
    NativeProcess();

    int option;
    int show_settings = 0;
    char *parameter = NULL;
    char *value = NULL;
    char *update_with_file = NULL;
    int delete_parameter = 0;
    int update_parameter = 0;
    int update_file = 0;

    while (1) {

	option = getopt(argc, argv, "hsp:v:udf:");
	if (option == -1) {
	    break;
	}

	switch (option) {

	case 'h':
	    ShowHelp();
	    break;

	case 's':
	    show_settings = 1;
	    break;

	case 'p':
	    asprintf(&parameter, "%s", optarg);
	    break;

	case 'v':
	    asprintf(&value, "%s", optarg);
	    break;

	case 'd':
	    delete_parameter = 1;
	    break;

	case 'u':
	    update_parameter = 1;
	    break;

	case 'f':
	    update_file = 1;
	    asprintf(&update_with_file, "%s", optarg);
	    break;

	default:
	    ShowHelp();

	}
    }

    // Sanity check options passed
    if (delete_parameter+update_parameter+update_file+show_settings != 1
	|| (delete_parameter && parameter==NULL)
	|| (update_parameter && (parameter==NULL || value==NULL))
	|| (update_file && update_with_file==NULL) ) {

	ShowHelp();
    }

    if (show_settings)	{
	ShowSettings(parameter);
    }
    else if (update_file) {
	UpdateThroughFile(update_with_file);
    }
    else if (update_parameter) {
	UpdateSingleParameter(parameter, value, false);
    }
    else if (delete_parameter) {
	UpdateSingleParameter(parameter, value, true);
    }

    return 0;
}
