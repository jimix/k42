/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Configure.C,v 1.10 2005/01/31 05:05:44 mergen Exp $
 *****************************************************************************/
#define __KERNEL__
#include "lkKernIncs.H"
#include <lk/LinuxEnv.H>
#include <lk/InitCalls.H>
#include <alloc/PageAllocatorDefault.H>
extern "C" {
#define private __C__private
#define typename __C__typename
#define new __C__new
#define class __C__class
#define virtual __C__virtual
#include <linux/init.h>
#undef private
#undef typename
#undef new
#undef class
#undef virtual
}
#include <stub/StubKBootParms.H>
#include <mem/PageAllocatorKernPinned.H>

#include "mem/RegionDefault.H"
#include "mem/FCM.H"
#include "mem/FCMStartup.H"
#include "mem/FRPlaceHolder.H"
#include <bilge/CObjGlobalsKern.H>

#include <asm/cputable.h>


extern "C" void sock_init(void);
extern "C" struct obs_kernel_param* setup_ipauto;
extern "C" int ip_auto_config_setup(char* addrs);

extern "C" void vfs_caches_init(unsigned long numpages);
extern "C" void driver_init(void);
extern "C" void buffer_init(void);
extern void ConfigureLinuxHWDevArch();
extern void ConfigureLinuxHWBlockArch();


static uval ran_ConfigureLinuxHWBlock = 0;
void
ConfigureLinuxHWBlock(VPNum vp)
{
    if (vp) return;

    if (ran_ConfigureLinuxHWBlock) return;

    ran_ConfigureLinuxHWBlock = 1;

    ConfigureLinuxHWBlockArch();
    if (!KernelInfo::OnSim()) {
	INITCALL(ata_init);
	INITCALL(ide_init);
	INITCALL(idedisk_init);
	INITCALL(ide_cdrom_init);
	INITCALL(cdrom_init);

	INITCALL(init_scsi);
	INITCALL(init_sd);
    }
}

void
ConfigureLinuxHWDev(VPNum vp)
{
    if (vp) return;

    {
	LinuxEnv le(SysCall);
	vfs_caches_init(0x1<<8);
	driver_init();
	buffer_init();
    }

    ConfigureLinuxHWDevArch();

    INITCALL(deadline_slab_setup);
    INITCALL(device_init);
    INITCALL(elevator_global_init);
    INITCALL(init_bio);
    INITCALL(vio_bus_init);

    if (!KernelInfo::OnSim() || KernelInfo::OnHV()) {
	INITCALL(pcibus_class_init);
	INITCALL(pci_driver_init);
	INITCALL(pci_init);

    }
}

void
ConfigureLinuxNet(VPNum vp)
{
    if (vp) return;

    static int net_init = 0;
    if (net_init) return;

    char buf[512];
    char* ptr = &buf[0];
    *ptr=0;
    StubKBootParms::_GetParameterValue("K42_IP_ADDRESS", ptr, 512);

    while (*ptr) ++ptr;
    *ptr = ':'; ++ptr; *ptr = 0;

    // server ip is blank
    *ptr = ':'; ++ptr; *ptr = 0;

    StubKBootParms::_GetParameterValue("K42_IP_ROUTER", ptr, 512);
    while (*ptr) ++ptr;
    *ptr = ':'; ++ptr; *ptr = 0;

    StubKBootParms::_GetParameterValue("K42_IP_NETMASK", ptr, 512);
    while (*ptr) ++ptr;
    *ptr = ':'; ++ptr; *ptr = 0;

    StubKBootParms::_GetParameterValue("K42_IP_HOSTNAME", ptr, 512);
    while (*ptr) ++ptr;
    *ptr = ':'; ++ptr; *ptr = 0;

    StubKBootParms::_GetParameterValue("K42_IP_INTERFACE", ptr, 512);
    while (*ptr) ++ptr;
    *ptr = 0;

    net_init = 1;
    LinuxEnv le(SysCall);
    sock_init();
    INITCALL(netlink_proto_init);
    INITCALL(net_dev_init);
    INITCALL(net_olddevs_init);
    INITCALL(inet_init);

    INITCALL(ip_auto_config);


    ip_auto_config_setup(buf);

}

