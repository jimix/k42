
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PacketFilterRoot.C,v 1.3 2002/01/10 18:05:14 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for network packet root filter
 *****************************************************************************/

#include "kernIncs.H"
#include <alloc/alloc.H>
#include <bilge/PacketFilterRoot.H>
#include <bilge/PacketFilterProtocols.H>
#include <io/PacketFilterCommon.H>

#if DECODE_PACKET > 0
static int decode_tcp_hdr(struct tcphdr *tcphd);
static int decode_ip_hdr(struct iphdr *iphd);
#endif /* #if DECODE_PACKET > 0 */

#define FILTER_WILDCARD    0xFFFFFFFF

#define FILTER_ETHER   0x00000000
#define FILTER_IP      0x10000000
#define FILTER_TCP     0x20000000
#define FILTER_UDP     0x30000000

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#define __BIG_ENDIAN__   // defined BIG_ENDIAN by default if neither is defined
#endif /* #if !defined(__BIG_ENDIAN__) && ... */
#if defined(__BIG_ENDIAN__)
#define HTONL(x) (x)
#define HTONS(x) (x)
#define NTOHL(x) (x)
#define NTOHS(x) (x)
#elif defined(__LITTLE_ENDIAN__)
#define HTONL(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))
#define HTONS(x) ((((x) & 0xff) << 8) | ((x) & 0xff00))
#define NTOHL(x) ((((x) & 0xff) << 24) | (((x) & 0xff00) << 8) | (((x) & 0xff0000) >> 8) | (((x) & 0xff000000) >> 24))
#define NTOHS(x) ((((x) & 0xff) << 8) | ((x) & 0xff00))
#else /* #if defined(__BIG_ENDIAN__) */
#error "Need to define either __BIG_ENDIAN__ or __LITTLE_ENDIAN__"
#endif /* #if defined(__BIG_ENDIAN__) */


#define filterHashBucketAlloc() \
  (filter_hash_bucket *)allocPinnedGlobal(sizeof(struct filter_hash_bucket))
#define filterHashBucketFree(bucket) \
  freePinnedGlobal(bucket, sizeof(struct filter_hash_bucket))
#define filterNodeAlloc() \
  (filter_node *)allocPinnedGlobal(sizeof(struct filter_node))
#define filterNodeFree(node) \
  freePinnedGlobal(node, sizeof(struct filter_node))

#define dprintf cprintf


/* static */ SysStatus
PacketFilterRoot::Create(PacketFilterRootRef &pfrRef)
{
    PacketFilterRoot *pfr = new PacketFilterRoot;
    SysStatus rc;

    rc = pfr->init();
    tassert(_SUCCESS(rc), err_printf("PacketFilterRoot::Create failed\n"));

    pfrRef = (PacketFilterRootRef)CObjRootSingleRep::Create(pfr);

    return 0;
}

/* virtual */ SysStatus
PacketFilterRoot::init()
{

    filterLock.init();

    filterRoot = filterCreateRoot(FILTER_ETHER | FILTER_ETHER_TYPE);

    return 0;
}


/* virtual */ SysStatus
PacketFilterRoot::destroy()
{
    /* FIXME: remove all active filters */
    tassert(filterRoot->active_count == 0,
            err_printf("PacketFilterRoot::destroy: active filters found\n"));

    filterNodeFree(filterRoot);

    return 0;
}

/* Creates a root node and initializes it */
/* private */ struct filter_node *
PacketFilterRoot::filterCreateRoot(uval field_type)
{
    struct filter_node *node;
    uval i;

    node = filterNodeAlloc();

    node->active_count = 0;
    node->field_type = field_type;
    node->bucket = NULL;
    node->prev_filter = NULL;
    node->wildcard_object.next_node = NULL;
    node->wildcard_object.field_value = 0;
    node->wildcard_object.this_node = node;

    for (i=0; i<FILTER_ACTIVE_OBJS; i++) {
        node->active_objects[i].next_node = NULL;
        node->active_objects[i].this_node = node;
        node->active_objects[i].field_value = 0;
    }

    return node;
}


/* private */ void
PacketFilterRoot::filterRemoveRoot(struct filter_node *root)
{
    struct filter_hash_bucket *bucket, *tmp_bucket;

    /* Go through any hash buckets freeing them as well */
    /* FIXME: this should be done incrementally through unlink node */
    bucket = root->bucket;

    while (bucket != NULL) {
        tmp_bucket = bucket->next;
        filterHashBucketFree(bucket);
        bucket = tmp_bucket;
    }

    /* Free root */
    filterNodeFree(root);
}


/* private */ void
PacketFilterRoot::filterUnlinkNodeObj(struct filter_node *parent,
                                      struct filter_node_object *old_obj)
{
    unsigned char index = old_obj->index;
    struct filter_hash_bucket *bucket, *prev_bucket;

    /* First figure out where this filter was */
    /* Try active objects first */
    if (parent->active_count > index &&
        &parent->active_objects[index] == old_obj) {
        /* Fixup active objects */
        parent->active_objects[index] =
            parent->active_objects[parent->active_count-1];
        parent->active_objects[index].index = index;
        parent->active_count--;
        return;
    }

    /* Try wildcard next */
    if (&parent->wildcard_object == old_obj) {
        parent->wildcard_object.field_value = ~FILTER_WILDCARD;
        return;
    }

    /* Try hash buckets */
    bucket = parent->bucket;
    prev_bucket = NULL;

    while (bucket != NULL) {
        /* Check if this hash bucket contains the object */
        if (bucket->hash_count > index &&
            &bucket->hash_objects[index] == old_obj) {
            /* Fixup bucket */
            bucket->hash_objects[index] =
                bucket->hash_objects[bucket->hash_count-1];
            bucket->hash_objects[index].index = index;
            bucket->hash_count--;
            /* FIXME: there is a lot more that can be done here */
        }
        prev_bucket = bucket;
        bucket = bucket->next;
    }

    return;
}


/* private */ struct filter_node_object *
PacketFilterRoot::filterLinkNodeObject(struct filter_node_object *old_obj,
                                       uval                       field_type,
                                       uval32                     field_value,
                                       uval                       callback_arg,
                                       void (*callback)(uval, void *))
{
    struct filter_node *root;
    struct filter_node_object *new_obj;

    /* If the node object passed in was a leaf then create a new root
     * node and link it to the old node object.  Then using the root insert
     * the filter information into a node object linked of that root
     */
    if (old_obj->next_node == NULL) {
        root = filterCreateRoot(field_type);
        old_obj->next_node = root;
        root->prev_filter = old_obj;
        new_obj = NULL;
    } else {
        /* If the node object passed in is linked to a filter node find node
         * object corresponding to the new filter. NULL is returned if not
         * found
         */
        root = old_obj->next_node;
        root->prev_filter = old_obj;
        new_obj = filterFindNodeObject(root, field_value, 1);
    }

    if (new_obj == NULL) {
        new_obj = filterInsertNodeObject(root, field_value,
                                         callback_arg, callback);
    }

    return new_obj;
}


/* private */ SysStatus
PacketFilterRoot::filterVerifyNodeObj(struct filter_node_object *node_obj,
                                      uval32 field_value)
{
    if (node_obj == NULL) {
        return 0; /* No matches, nothing conflicting */
    }
    if (node_obj->field_value == FILTER_WILDCARD) {
        if (field_value == FILTER_WILDCARD && node_obj->next_node == NULL) {
            return 1; /* Already have a wildcard, this is another */
        } else if (field_value != FILTER_WILDCARD) {
            return 0; /* Only match was wildcard, allow more specific */
        }
    }
    return -1;
}


/* private */ SysStatus
PacketFilterRoot::filterValidate(UserFilter *filter)
{
    /* For now only allow wildcards in TCP_SRC, UDP_SRC, IP_SRC, and ETH_SRC */
    if (!(filter->flags & FILTER_ETHER_TYPE)) {
        dprintf("Ether type must be specified\n");
        return -1;
    } else if (filter->ether_type == FILTER_WILDCARD) {
        /* FIXME */
        dprintf("Ether type cannot be wildcard\n");
        return -1;
    }

    /* Check if this an IP filter */
    if (filter->ether_type == ETHER_TYPE_IP) {
        struct ip_filter *ip_filter;
        ip_filter = &(filter->ether_filter.ip_filter);

        if (!(ip_filter->flags & FILTER_IP_PROT)) {
            dprintf("IP protocol must be specified\n");
            return -1;
        } else if (ip_filter->protocol == FILTER_WILDCARD) {
            /* FIXME */
            dprintf("IP protocol cannot be wildcard\n");
            return -1;
        }

        /* Check if this a TCP filter */
        if (ip_filter->protocol == IP_PROTOCOL_TCP) {
            if (!(ip_filter->tp.tcp_filter.flags & FILTER_TCP_DST)) {
                dprintf("TCP destination must be specified\n");
                return -1;
            } else if (ip_filter->tp.tcp_filter.dest_port == FILTER_WILDCARD) {
                /* FIXME */
                dprintf("TCP destination cannot be wildcard\n");
                return -1;
            }
        } else if (ip_filter->protocol == IP_PROTOCOL_UDP) {
            if (!(ip_filter->tp.udp_filter.flags & FILTER_UDP_DST)) {
                dprintf("UDP destination must be specified\n");
                return -1;
            } else if (ip_filter->tp.udp_filter.dest_port == FILTER_WILDCARD) {
                /* FIXME */
                dprintf("UDP destination cannot be wildcard\n");
                return -1;
            }
        }
    }

    return 0;
}


/* Searches for and returns a node object based upon matching field_value */
/* private */ struct filter_node_object *
PacketFilterRoot::filterFindNodeObject(struct filter_node *root,
                                       uval32 field_value,
                                       uval exact_match)
{
    struct filter_hash_bucket *bucket;
    uval i;

    if ((root == NULL || root->active_count == 0) &&
        (root->wildcard_object.field_value != FILTER_WILDCARD)) {
        return NULL;
    }

    /* First check active objects */
    for (i = 0; i < root->active_count; i++) {
        if (root->active_objects[i].field_value == field_value) {
            //      dprintf("Found active object match\n");
            return &root->active_objects[i];
        }
    }

    /* Check hash buckets */
    bucket = root->bucket;

    while (bucket != NULL) {
        /* Check if this field value fits in the range held by this bucket */
        if (field_value >= bucket->start_range ||
            field_value <= bucket->end_range) {

            /* Go through bucket objects checking for value */
            for (i = 0; i < bucket->hash_count; i++) {
                if (bucket->hash_objects[i].field_value == field_value) {
                    //          dprintf("Found hash bucket match\n");
                    return &bucket->hash_objects[i];
                }
            }
        }

        bucket = bucket->next;
    }

    /* Lastly check wildcard */
    if ((root->wildcard_object.field_value == FILTER_WILDCARD) &&
        ((exact_match && field_value == FILTER_WILDCARD) || (!exact_match)) ) {
        //    dprintf("Found wildcard\n");
        return &root->wildcard_object;
    }

    return NULL; /* Not found */
}

/* Inserts a node object into a parent node */
/* private */ struct filter_node_object *
PacketFilterRoot::filterInsertNodeObject(struct filter_node *root,
                                         uval32 field_value,
                                         uval callback_arg,
                                         void (*callback)(uval, void *))
{
    struct filter_node_object *new_node=NULL;
    struct filter_hash_bucket *bucket;
    struct filter_hash_bucket *prev_bucket;
    uval8 index=0;

    /* Check if this is a wildcard */
    if (field_value == FILTER_WILDCARD) {
        new_node = &root->wildcard_object;
        new_node->index = 0;
        new_node->field_value = field_value;
        new_node->callback_arg = callback_arg;
        new_node->callback = callback;
        new_node->next_node = NULL;
        return new_node;
    }

    /* Check if room in active objects */
    if (root->active_count < FILTER_ACTIVE_OBJS) {
        new_node = &root->active_objects[root->active_count];
        new_node->index = root->active_count;
        new_node->field_value = field_value;
        new_node->callback_arg = callback_arg;
        new_node->callback = callback;
        new_node->next_node = NULL;

        root->active_count++;

        return new_node;
    }

    /* Check hash buckets */
    bucket = root->bucket;
    prev_bucket = NULL;

    /* Go through existing buckets and find insert point */
    while (bucket != NULL) {

        if (bucket->hash_count < FILTER_HASH_OBJS) {
            /* Insert here */

            /* Set start range */
            if (bucket->start_range > field_value) {
                bucket->start_range = field_value;
            }
            /* Set end range */
            if (bucket->end_range < field_value) {
                bucket->end_range = field_value;
            }

            new_node = &bucket->hash_objects[bucket->hash_count];
            index = bucket->hash_count;
            bucket->hash_count++;

            break;
        }

        prev_bucket = bucket;
        bucket = bucket->next;
    }

    /* All buckets are full or none exist */
    if (bucket == NULL) {

        /* Set up new bucket */
        bucket = filterHashBucketAlloc();
        bucket->start_range = field_value;
        bucket->end_range = field_value;
        bucket->hash_count = 1;
        bucket->next = NULL;

        new_node = &bucket->hash_objects[0];
        index = 0;
        if (prev_bucket == NULL) {   /* No previous buckets, link to root */
            root->bucket = bucket;
        } else {                     /* Link to previous bucket */
            prev_bucket->next = bucket;
        }

    }

    /* Set up new node */
    new_node->index = index;
    new_node->field_value = field_value;
    new_node->callback_arg = callback_arg;
    new_node->callback = callback;
    new_node->next_node = NULL;

    return new_node;
}


/* Walks through the filter tree to search for duplicates
 * Returns 0 if no duplicates found
 * Returns 1 if duplicates or conflicting filter's found
 */
/* private */ SysStatus
PacketFilterRoot::filterCheckDuplicates(UserFilter *filter)
{
    uval32 field_value;
    SysStatus rc;
    struct filter_node_object *node_obj;
    struct filter_node *root;

    root = filterRoot;

    /* Check ethernet type */
    field_value = filter->ether_type;
    node_obj = filterFindNodeObject(root, field_value, 0);
    if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
        return rc;
    }

    /* IP type filter */
    if (filter->ether_type == ETHER_TYPE_IP) {
        struct ip_filter *ip_filter;
        ip_filter = &filter->ether_filter.ip_filter;
        field_value = ip_filter->protocol;

        root = node_obj->next_node;
        node_obj = filterFindNodeObject(root, field_value, 0);
        if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
            return rc;
        }

        /* Determine next node in filter (TCP or UDP) */
        if (ip_filter->protocol == IP_PROTOCOL_TCP) { /* IP TCP filters */
            struct tcp_filter *tcp_filter;
            tcp_filter = &ip_filter->tp.tcp_filter;

            /* Check TCP destination port */
            field_value = tcp_filter->dest_port;
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, field_value, 0);
            if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
                return rc;
            }

            /* Then check for source port */
            if (tcp_filter->flags & FILTER_TCP_SRC) {
                field_value = tcp_filter->source_port;
            } else {
                field_value = FILTER_WILDCARD;
            }
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, field_value, 0);
            if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
                return rc;
            }


        } else if (ip_filter->protocol == IP_PROTOCOL_UDP) {
            struct udp_filter *udp_filter;
            udp_filter = &ip_filter->tp.udp_filter;

            /* Check UDP destination port */
            field_value = udp_filter->dest_port;
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, field_value, 0);
            if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
                return rc;
            }

            /* Then check for source port */
            if (udp_filter->flags & FILTER_UDP_SRC) {
                field_value = udp_filter->source_port;
            } else {
                field_value = FILTER_WILDCARD;
            }
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, field_value, 0);
            if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
                return rc;
            }

        }

        /* Ok, done with transport protocol, now do IP src address */
        if (ip_filter->flags & FILTER_IP_SRC) {
            field_value = ip_filter->source_addr;
        } else {
            field_value = FILTER_WILDCARD;
        }
        root = node_obj->next_node;
        node_obj = filterFindNodeObject(root, field_value, 0);
        if ((rc = filterVerifyNodeObj(node_obj, field_value)) >= 0) {
            return rc;
        }
    }

    return 1;
}


/******************** Public interface functions follow ********************/


/* Removes a filter starting at leaf node and working upwards */
/* virtual */ SysStatus
PacketFilterRoot::filterRemoveNode(DeviceFilter *filter)
{
    struct filter_node_object *leaf, *prev_leaf;
    struct filter_node *parent;
    uval branch_point=0;

    filterLock.acquire();

    leaf = (struct filter_node_object *)(filter->dev_private);

    while (leaf != NULL) {
        parent = leaf->this_node;
        prev_leaf = parent->prev_filter;

        dprintf("Remove: parent=%p, node=%p, prev_node=%p\n",
                parent, leaf, prev_leaf);

        /* Check parent to see if the leaf was it's only child,
         * if so remove parent
         */
        if (!branch_point &&
            (parent->active_count == 1 ||
             (parent->active_count == 0 &&
              parent->wildcard_object.field_value == FILTER_WILDCARD))) {

            /* Remove parent, unless this is the filter root */
            if (parent != filterRoot) {
                dprintf("Remove: Removing parent: %p\n", parent);
                filterRemoveRoot(parent);
                prev_leaf->next_node = NULL;
            } else {
                parent->active_count = 0;
            }

        } else if (leaf->next_node == NULL) {
            /* If leaf has no children then remove it from the filter */
            dprintf("Remove: Unlinking leaf: %p\n", leaf);
            filterUnlinkNodeObj(parent, leaf);
            branch_point=1;
        }

        leaf = prev_leaf;
    }

    filter->dev_private = NULL;

    filterLock.release();

    return 0;
}


/* virtual */ SysStatus
PacketFilterRoot::filterAddNode(DeviceFilter *filter)
{
    uval32 field_value;
    struct filter_node *root;
    struct filter_node_object *node_obj;

    filterLock.acquire();

    /* Lower level validation */
    if (filterValidate(filter->filter) < 0) {
        dprintf("Error: filter validation failed\n");
        filterLock.release();
        return -1;
    }

    /* Check for duplicate filters */
    if (filterCheckDuplicates(filter->filter) != 0) {
        dprintf("Error: filter duplicate check failed\n");
        filterLock.release();
        return -1;
    }

    root = filterRoot;

    /* Go through filter and construct tree */
    /* Ethernet type filter */
    field_value = HTONS(filter->filter->ether_type);
    node_obj = filterFindNodeObject(root, field_value, 1);
    if (node_obj == NULL) {
        node_obj = filterInsertNodeObject(root, field_value,
                                          filter->callback_arg,
                                          filter->callback);
    }

    /* IP type filter */
    if (filter->filter->ether_type == ETHER_TYPE_IP) {
        struct ip_filter *ip_filter;
        ip_filter = &filter->filter->ether_filter.ip_filter;

        /* First check IP protocol */
        /* Link in node object */
        node_obj = filterLinkNodeObject(node_obj,
                                        FILTER_IP | FILTER_IP_PROT,
                                        ip_filter->protocol,
                                        filter->callback_arg,
                                        filter->callback);

        /* Determine next node in filter (TCP or UDP) */
        if (ip_filter->protocol == IP_PROTOCOL_TCP) { /* IP TCP filters */
            struct tcp_filter *tcp_filter;
            tcp_filter = &ip_filter->tp.tcp_filter;

            /* Next is destination port */
            node_obj = filterLinkNodeObject(node_obj,
                                            FILTER_TCP | FILTER_TCP_DST,
                                            HTONS(tcp_filter->dest_port),
                                            filter->callback_arg,
                                            filter->callback);

            /* Then check for source port */
            if (tcp_filter->flags & FILTER_TCP_SRC) {
                field_value = HTONS(tcp_filter->source_port);
            } else {
                field_value = FILTER_WILDCARD;
            }

            node_obj = filterLinkNodeObject(node_obj,
                                            FILTER_TCP | FILTER_TCP_SRC,
                                            field_value,
                                            filter->callback_arg,
                                            filter->callback);

        } else if (ip_filter->protocol == IP_PROTOCOL_UDP) {
            struct udp_filter *udp_filter;
            udp_filter = &ip_filter->tp.udp_filter;

            /* Next is destination port */
            node_obj = filterLinkNodeObject(node_obj,
                                            FILTER_UDP | FILTER_UDP_DST,
                                            HTONS(udp_filter->dest_port),
                                            filter->callback_arg,
                                            filter->callback);

            /* Then check for source port */
            if (udp_filter->flags & FILTER_UDP_SRC) {
                field_value = HTONS(udp_filter->source_port);
            } else {
                field_value = FILTER_WILDCARD;
            }

            node_obj = filterLinkNodeObject(node_obj,
                                            FILTER_UDP | FILTER_UDP_SRC,
                                            field_value,
                                            filter->callback_arg,
                                            filter->callback);
        }

        /* Ok, done with transport protocol, now do IP src address */
        if (ip_filter->flags & FILTER_IP_SRC) {
            field_value = HTONL(ip_filter->source_addr);
        } else {
            field_value = FILTER_WILDCARD;
        }

        node_obj = filterLinkNodeObject(node_obj,
                                        FILTER_IP | FILTER_IP_SRC,
                                        field_value,
                                        filter->callback_arg,
                                        filter->callback);
    }

    /* FIXME: add filter for ethernet source addr */

    /* Lastly keep track of endpoint for easy removal */
    filter->dev_private = (void *)node_obj;

    filterLock.release();

    return 0;
}


/* Takes a pointer to the beginning of a packet and returns 0 or -1
 * on failure.
 */
/* virtual */ SysStatus
PacketFilterRoot::filterFind(char *packet, uval *endpt_arg)
{
    struct filter_node        *root;
    struct filter_node_object *node_obj;
    char *packet_pos;

    struct etherhdr *eth_hdr;
    struct iphdr  *ip_hdr;
    struct tcphdr *tcp_hdr;
    struct udphdr *udp_hdr;

    packet_pos = packet;

    if (filterRoot == NULL || packet == NULL) {
        return -1;
    }

    filterLock.acquire();

    root = filterRoot;

    /* Check ethernet type */
    eth_hdr = (struct etherhdr *)packet_pos;
    packet_pos += sizeof(struct etherhdr);
    // dprintf("Checking ether prototype: %#x\n", ntohs(eth_hdr->ether_type));
    node_obj = filterFindNodeObject(root, eth_hdr->ether_type, 0);
    if (node_obj == NULL) {
        goto error;
    }

    /* Continue if this is an IP packet */
    if (eth_hdr->ether_type == HTONS(ETHER_TYPE_IP)) {
        ip_hdr = (struct iphdr *)packet_pos;
        packet_pos += (ip_hdr->ip_hl * 4);

        /* Make sure we are dealing with IPv4 */
        if (ip_hdr->ip_v != 4) {
            goto error;
        }

        /* Check IP protocol */
        //printf("Checking IP Protocol: %#x\n", ip_hdr->ip_p);
        root = node_obj->next_node;
        node_obj = filterFindNodeObject(root, ip_hdr->ip_p, 0);
        if (node_obj == NULL) {
            goto error;
        }

        if (ip_hdr->ip_p == IP_PROTOCOL_TCP) {
            tcp_hdr = (struct tcphdr *)packet_pos;

            /* Check TCP destination port */
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, tcp_hdr->tcp_dest, 0);
            if (node_obj == NULL) {
                goto error;
            }

            /* Check TCP source port */
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, tcp_hdr->tcp_source, 0);
            if (node_obj == NULL) {
                goto error;
            }

        } else if (ip_hdr->ip_p == IP_PROTOCOL_UDP) {
            udp_hdr = (struct udphdr *)packet_pos;

            /* Check UDP destination port */
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, udp_hdr->udp_dest, 0);
            if (node_obj == NULL) {
                goto error;
            }

            /* Check UDP source port */
            root = node_obj->next_node;
            node_obj = filterFindNodeObject(root, udp_hdr->udp_source, 0);
            if (node_obj == NULL) {
                goto error;
            }
        }

        /* Check IP source addr */
        root = node_obj->next_node;
        node_obj = filterFindNodeObject(root, ip_hdr->ip_source, 0);
        if (node_obj == NULL) {
            goto error;
        }

    }

    /* If we got this far then there this packet matched a filter
     * return the endpoint id
     */

    dprintf("Found match\n");

#if DECODE_PACKET > 0
    decode_ip_hdr(ip_hdr);
    decode_tcp_hdr(tcp_hdr);
#endif /* #if DECODE_PACKET > 0 */

    if (node_obj->callback != NULL) {
        node_obj->callback(node_obj->callback_arg, packet);
    }

    // FIXME...
    *endpt_arg = node_obj->callback_arg;

    filterLock.release();

    return 0;

 error:

    filterLock.release();
    return -1;
}


/* Prints a node */
static void
filter_print_node(uval field_type,
                  struct filter_node_object *node_obj)

{
    dprintf("\n");

    switch (field_type & 0xF0000000) {
    case FILTER_ETHER:
        dprintf("Ether ");
        switch (field_type & 0x0FFFFFFF) {
        case FILTER_ETHER_TYPE:
            dprintf("Type=%#x ", node_obj->field_value);
            break;
        }
        break;
    case FILTER_IP:
        dprintf("IP ");
        switch (field_type & 0x0FFFFFFF) {
        case FILTER_IP_VER:
            dprintf("Version=%d ", node_obj->field_value);
            break;
        case FILTER_IP_PROT:
            dprintf("Protocol=%#x ", node_obj->field_value);
            break;
        case FILTER_IP_SRC:
            dprintf("Source=%#x ", node_obj->field_value);
            break;
        case FILTER_IP_DST:
            dprintf("Dest=%#x ", node_obj->field_value);
            break;
        }
        break;
    case FILTER_TCP:
        switch (field_type & 0x0FFFFFFF) {
        case FILTER_TCP_SRC:
            dprintf("Source=%d ", node_obj->field_value);
            break;
        case FILTER_TCP_DST:
            dprintf("Dest=%d ", node_obj->field_value);
            break;
        }
        break;
    case FILTER_UDP:
        switch (field_type & 0x0FFFFFFF) {
        case FILTER_UDP_SRC:
            dprintf("Source=%d ", node_obj->field_value);
            break;
        case FILTER_UDP_DST:
            dprintf("Dest=%d ", node_obj->field_value);
            break;
        }
        break;
    }

    return;
}

/* Prints a dump of the filter tree doing a depth first search */
static void
filter_dump_tree(struct filter_node *root,
                 uval level)
{
    struct filter_node_object *node_obj;
    struct filter_hash_bucket *bucket;
    uval i;

    if (root == NULL) {
        return;
    }

    /* Do a depth first printout of tree */
    /* Check active object list */
    for (i = 0; i < root->active_count; i++) {
        node_obj = &root->active_objects[i];
        filter_dump_tree(node_obj->next_node, level+1);
        filter_print_node(root->field_type, node_obj);
        dprintf("(Level=%ld Active=%ld/%d) (Rt=%p, Nd=%p, Nxt=%p, Prv=%p)\n",
                level, i, root->active_count, root, node_obj,
                node_obj->next_node,
                root->prev_filter);
    }

    bucket = root->bucket;
    /* Check buckets */
    while (bucket != NULL) {
        for (i = 0; i < bucket->hash_count; i++) {
            node_obj = &bucket->hash_objects[i];
            filter_dump_tree(node_obj->next_node, level+1);
            filter_print_node(root->field_type, node_obj);
            dprintf("(Level=%ld Bucket=%ld) (Rt=%p, Nd=%p, Nxt=%p, Prv=%p)\n",
                    level, i, root, node_obj, node_obj->next_node,
                    root->prev_filter);
        }
        bucket = bucket->next;
    }

    /* Check wildcards */
    if (root->wildcard_object.field_value == FILTER_WILDCARD) {
        node_obj = &root->wildcard_object;
        filter_dump_tree(node_obj->next_node, level+1);
        filter_print_node(root->field_type, node_obj);
        dprintf("(Level=%ld Wildcard) (Rt=%p, Nd=%p, Nxt=%p, Prv=%p)\n",
                level, root, node_obj, node_obj->next_node, root->prev_filter);
    }
}

#if DECODE_PACKET > 0

#define IP_ADDR_FORMAT(x) (((x) >> 24) & 0xff), (((x) >> 16) & 0xff), \
                          (((x) >> 8) & 0xff), ((x) & 0xff)


static
int decode_tcp_hdr(struct tcphdr *tcphd)
{
    cprintf("TCP Header\n");
    cprintf("SRC Port:\t%d\n", NTOHS(tcphd->tcp_source));
    cprintf("DST Port:\t%d\n", NTOHS(tcphd->tcp_dest));
    cprintf("seq #:   \t%lu\n", (long)NTOHL(tcphd->tcp_seq));
    cprintf("ack #:   \t%lu\n", (long)NTOHL(tcphd->tcp_ack_seq));
    cprintf("hdr len: \t%d\n", tcphd->tcp_doff * 4);
    cprintf("rst:     \t%d\n", tcphd->tcp_rst);
    cprintf("syn:     \t%d\n", tcphd->tcp_syn);
    cprintf("fin:     \t%d\n", tcphd->tcp_fin);
    cprintf("window:  \t%d\n", NTOHS(tcphd->tcp_window));
    cprintf("cksum:   \t%d\n", NTOHS(tcphd->tcp_sum));
    cprintf("urg_ptr: \t%d\n", NTOHS(tcphd->tcp_urg_ptr));
    cprintf("P: %d, A: %d, U: %d, R1: %d, R2: %d\n",
            tcphd->tcp_psh, tcphd->tcp_ack, tcphd->tcp_urg, tcphd->tcp_res1,
            tcphd->tcp_res2);

    return 0;
}


static
int decode_ip_hdr(struct iphdr *iphd)
{
    cprintf("IP Header\n");
    cprintf("SRC IP:     \t%ld.%ld.%ld.%ld\n",
            IP_ADDR_FORMAT((long)NTOHL(iphd->ip_source)));
    cprintf("DST IP:     \t%ld.%ld.%ld.%ld\n",
            IP_ADDR_FORMAT((long)NTOHL(iphd->ip_dest)));
    cprintf("version:    \t%d\n", iphd->ip_v);
    cprintf("protocol:   \t%d\n", iphd->ip_p);
    cprintf("frag offset:\t%d\n", NTOHS(iphd->ip_off)&0x1fff);
    cprintf("more frags: \t%d\n", (NTOHS(iphd->ip_off)&0x2000) >> 13);
    cprintf("ip hdr len: \t%d\n", iphd->ip_hl*4);
    cprintf("total len:  \t%d\n", NTOHS(iphd->ip_len));
    cprintf("id:         \t%d\n", NTOHS(iphd->ip_id));
    cprintf("\n");

    return 0;
}


#endif /* #if DECODE_PACKET > 0 */





