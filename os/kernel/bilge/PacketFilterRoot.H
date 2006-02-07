/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilterRoot.H,v 1.5 2002/10/10 13:08:31 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for network packet root filter
 *****************************************************************************/

#ifndef __PACKET_FILTER_ROOT_H_
#define __PACKET_FILTER_ROOT_H_

#include <cobj/CObjRoot.H>
#include <io/PacketFilterCommon.H>

class PacketFilterRoot;
typedef PacketFilterRoot** PacketFilterRootRef;

#define FILTER_ACTIVE_OBJS 8
#define FILTER_HASH_OBJS 8

struct filter_node_object;
struct filter_hash_bucket;
struct filter_node;

struct filter_node_object {
    uval32                     field_value;
    uval                       callback_arg;
    void                     (*callback)(uval arg, void *packet);
    uval8                      index;
    struct filter_node        *next_node;
    struct filter_node        *this_node;
};

struct filter_hash_bucket {
    uval32                     start_range;
    uval32                     end_range;
    uval8                      hash_count;
    struct filter_node_object  hash_objects[FILTER_HASH_OBJS];
    struct filter_hash_bucket *next;
};

struct filter_node {
    uval32                     field_type;
    uval8                      active_count;
    struct filter_node_object  wildcard_object;
    struct filter_node_object  active_objects[FILTER_ACTIVE_OBJS];
    struct filter_node_object *prev_filter;
    struct filter_hash_bucket *bucket;
};


class PacketFilterRoot : public Obj {

private:

    struct filter_node *filterRoot;

    /* Private functions called with lock held */

    SysStatus filterValidate(UserFilter *filter);

    SysStatus filterCheckDuplicates(UserFilter *filter);

    struct filter_node_object *filterFindNodeObject(struct filter_node *root,
                                                    uval32 field_value,
                                                    uval exact_match);

    struct filter_node_object *filterInsertNodeObject(struct filter_node *root,
                                                      uval32 field_value,
                                                      uval callback_arg,
                                               void (*callback)(uval, void *));

    struct filter_node *filterCreateRoot(uval field_type);

    void filterRemoveRoot(struct filter_node *root);

    void filterUnlinkNodeObj(struct filter_node *parent,
                             struct filter_node_object *old_obj);

    struct filter_node_object *filterLinkNodeObject(
                                          struct filter_node_object *old_obj,
                                          uval field_type, uval32 field_value,
                                          uval callback_arg,
                                          void (*callback)(uval, void *));

    SysStatus filterVerifyNodeObj(struct filter_node_object *node_obj,
                                  uval32 field_value);

protected:

    DEFINE_PINNEDGLOBAL_NEW(PacketFilterRoot);

    BLock filterLock;  // FIXME: currently one global filter lock...

public:

    PacketFilterRoot() { /* empty body */ }

    static SysStatus Create(PacketFilterRootRef &pfrRef);

    virtual SysStatus init();

    virtual SysStatus destroy();

    virtual SysStatus filterAddNode(DeviceFilter *filter);

    virtual SysStatus filterRemoveNode(DeviceFilter *filter);

    virtual SysStatus filterFind(char *packet, uval *endpt_arg);

};

#endif /* #ifndef __PACKET_FILTER_ROOT_H_ */
