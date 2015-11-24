/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <czmq.h>

#include "rdl.h"
#include "resrc_tree.h"
#include "src/common/libutil/xzmalloc.h"

struct resrc_tree_list {
    zlist_t *list;
};


struct resrc_tree {
    resrc_tree_t *parent;
    resrc_t *resrc;
    resrc_tree_list_t *children;
};

/***********************************************************************
 * Resource tree
 ***********************************************************************/

resrc_t *resrc_tree_resrc (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree->resrc;
    return NULL;
}

size_t resrc_tree_num_children (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree_list_size (resrc_tree->children);
    return 0;
}

resrc_tree_list_t *resrc_tree_children (resrc_tree_t *resrc_tree)
{
    if (resrc_tree)
        return resrc_tree->children;
    return NULL;
}

int resrc_tree_add_child (resrc_tree_t *parent, resrc_tree_t *child)
{
    int rc = -1;
    if (parent) {
        child->parent = parent;
        rc = resrc_tree_list_append (parent->children, child);
    }
    return rc;
}

int resrc_tree_add_child_special (resrc_tree_t *parent, resrc_tree_t *child, zhash_t *hash_table, int64_t owner)
{
    printf("inside the function\n");fflush(0);
    int rc = 0;
    if (parent) {
        printf("came inside parent\n");fflush(0);
        resrc_t *resrc = resrc_tree_resrc (child);
        char uuid[40];
        resrc_uuid (resrc, uuid);
        printf("got uuid\n");fflush(0);
        resrc_t *resrc_exist = zhash_lookup (hash_table, uuid);
        printf ("checked for resource exist\n"); fflush(0); 
        if (resrc_exist) {
            printf ("came inside resource exist\n"); fflush(0);
            if (resrc_tree_num_children (child)) {
                resrc_tree_t *chtree = resrc_tree_list_first (child->children);
                while (chtree) {
                    rc = resrc_tree_add_child_special (resrc_phys_tree (resrc_exist), chtree, hash_table, owner);
                    if (rc < 0) 
                        goto ret;
                    chtree = resrc_tree_list_next (child->children);
                }
            }
            rc = 0;
        } else {
            child->parent = parent;
            rc = resrc_tree_list_append (parent->children, child);   
            resrc_tree_set_owner (child, owner);
            printf ("set owner ok\n"); fflush(0);
            resrc_hash_by_uuid (child, hash_table);
            printf ("hash table add ok\n"); fflush(0);
        }
    }
ret:
    return rc;
}

resrc_tree_t *resrc_tree_new (resrc_tree_t *parent, resrc_t *resrc)
{
    resrc_tree_t *resrc_tree = xzmalloc (sizeof (resrc_tree_t));
    if (resrc_tree) {
        resrc_tree->parent = parent;
        resrc_tree->resrc = resrc;
        resrc_tree->children = resrc_tree_list_new ();
        if (parent)
            (void) resrc_tree_add_child (parent, resrc_tree);
    }

    return resrc_tree;
}

resrc_tree_t *resrc_tree_copy (resrc_tree_t *resrc_tree)
{
    resrc_tree_t *new_resrc_tree = xzmalloc (sizeof (resrc_tree_t));

    if (new_resrc_tree) {
        new_resrc_tree->parent = resrc_tree->parent;
        new_resrc_tree->resrc = resrc_tree->resrc;
        new_resrc_tree->children = resrc_tree_list_new ();
        new_resrc_tree->children->list = zlist_dup (resrc_tree->children->list);
    }

    return new_resrc_tree;
}

void resrc_tree_free (resrc_tree_t *resrc_tree, bool destroy_resrc)
{
    if (resrc_tree) {
        resrc_tree_list_free (resrc_tree->children);
        resrc_tree->children = NULL;
        if (destroy_resrc) {
            resrc_resource_destroy (resrc_tree->resrc);
        }
        free (resrc_tree);
    }
}

void resrc_tree_destroy (resrc_tree_t *resrc_tree, bool destroy_resrc)
{
    if (resrc_tree) {
        if (resrc_tree->parent)
            resrc_tree_list_remove (resrc_tree->parent->children, resrc_tree);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (child) {
                resrc_tree_destroy (child, destroy_resrc);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
        resrc_tree_free (resrc_tree, destroy_resrc);
    }
}

void resrc_tree_print (resrc_tree_t *resrc_tree)
{
    if (resrc_tree) {
        resrc_print_resource (resrc_tree->resrc);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (child) {
                resrc_tree_print (child);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
}

void resrc_tree_list_print (resrc_tree_list_t *resrc_tree_list)
{
    if (resrc_tree_list) {
        resrc_tree_t *child = resrc_tree_list_first (resrc_tree_list);
        while (child) {
            resrc_tree_print (child);
            child = resrc_tree_list_next (resrc_tree_list);
        }
    }
}


int resrc_tree_serialize (JSON o, resrc_tree_t *resrc_tree, int64_t jobid)
{
    JSON co = Jnew ();
    int rc = -1;

    if (o && resrc_tree) {
        rc = resrc_to_json (co, resrc_tree->resrc, jobid);
        json_object_array_add (o, co);
        if (!rc) {
            if (resrc_tree_num_children (resrc_tree)) {
                JSON ja = Jnew_ar ();
                resrc_tree_t *child;

                json_object_object_add (co, "children", ja);
                child = resrc_tree_list_first (resrc_tree->children);
                while (!rc && child) {
                    rc = resrc_tree_serialize (ja, child, jobid);
                    child = resrc_tree_list_next (resrc_tree->children);
                }
            }
        }
    }
    return rc;
}

resrc_tree_t *resrc_tree_deserialize (JSON o, resrc_tree_t *parent)
{
    JSON ca = NULL;     /* array of child json objects */
    JSON co = NULL;     /* child json object */
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = NULL;

    resrc = resrc_new_from_json (o, NULL, true);
    if (resrc) {
        resrc_tree = resrc_tree_new (parent, resrc);

        if ((ca = Jobj_get (o, "children"))) {
            int i, nchildren = 0;

            if (Jget_ar_len (ca, &nchildren)) {
                for (i=0; i < nchildren; i++) {
                    Jget_ar_obj (ca, i, &co);
                    (void) resrc_tree_deserialize (co, resrc_tree);
                }
            }
        }
    }

    return resrc_tree;
}

int resrc_tree_allocate (resrc_tree_t *resrc_tree, int64_t job_id,
                         int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_allocate_resource (resrc_tree->resrc, job_id,
                                      starttime, endtime);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (!rc && child) {
                rc = resrc_tree_allocate (child, job_id, starttime, endtime);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
    return rc;
}

int resrc_tree_allocate_dynamic (resrc_tree_t *resrc_tree, int64_t job_id,
                                 int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_allocate_resource_in_time_dynamic (resrc_tree->resrc, job_id,
                                                      starttime, endtime);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (!rc && child) {
                rc = resrc_tree_allocate_dynamic (child, job_id, starttime, endtime);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
    return rc;
}

int resrc_tree_reserve (resrc_tree_t *resrc_tree, int64_t job_id,
                        int64_t starttime, int64_t endtime)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_reserve_resource (resrc_tree->resrc, job_id,
                                     starttime, endtime);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (!rc && child) {
                rc = resrc_tree_reserve (child, job_id, starttime, endtime);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
    return rc;
}

int resrc_tree_release (resrc_tree_t *resrc_tree, int64_t job_id)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_release_allocation (resrc_tree->resrc, job_id);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (!rc && child) {
                rc = resrc_tree_release (child, job_id);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
    return rc;
}

int resrc_tree_release_all_reservations (resrc_tree_t *resrc_tree)
{
    int rc = -1;
    if (resrc_tree) {
        rc = resrc_release_all_reservations (resrc_tree->resrc);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (!rc && child) {
                rc = resrc_tree_release_all_reservations (child);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
    return rc;
}

void resrc_tree_unstage_resources (resrc_tree_t *resrc_tree)
{
    if (resrc_tree) {
        resrc_stage_resrc (resrc_tree->resrc, 0);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree->children);
            while (child) {
                resrc_tree_unstage_resources (child);
                child = resrc_tree_list_next (resrc_tree->children);
            }
        }
    }
}

/***********************************************************************
 * Resource tree list
 ***********************************************************************/

resrc_tree_list_t *resrc_tree_list_new ()
{
    resrc_tree_list_t *new_list = xzmalloc (sizeof (resrc_tree_list_t));
    new_list->list = zlist_new ();
    return new_list;
}

int resrc_tree_list_append (resrc_tree_list_t *rtl, resrc_tree_t *rt)
{
    if (rtl && rtl->list && rt)
        return zlist_append (rtl->list, (void *) rt);
    return -1;
}

int resrc_tree_list_list_append (resrc_tree_list_t *rtl, resrc_tree_list_t *newrtl)
{
    if (rtl && rtl->list && newrtl && newrtl->list) {
        resrc_tree_t *rt = resrc_tree_list_first (newrtl);
        while (rt) {
            resrc_tree_list_append (rtl, rt);
            rt = resrc_tree_list_next (newrtl);
        }
    }
    return -1;
}

resrc_tree_t *resrc_tree_list_first (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_first (rtl->list);
    return NULL;
}

resrc_tree_t *resrc_tree_list_next (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_next (rtl->list);
    return NULL;
}

size_t resrc_tree_list_size (resrc_tree_list_t *rtl)
{
    if (rtl && rtl->list)
        return zlist_size (rtl->list);
    return 0;
}

void resrc_tree_list_remove (resrc_tree_list_t *rtl, resrc_tree_t *rt)
{
    zlist_remove (rtl->list, rt);
}

void resrc_tree_list_free (resrc_tree_list_t *resrc_tree_list)
{
    if (resrc_tree_list) {
        if (resrc_tree_list->list) {
            zlist_destroy (&(resrc_tree_list->list));
        }
        free (resrc_tree_list);
    }
}

void resrc_tree_list_destroy (resrc_tree_list_t *resrc_tree_list,
                              bool destroy_resrc)
{
    if (resrc_tree_list) {
        if (resrc_tree_list_size (resrc_tree_list)) {
            resrc_tree_t *child = resrc_tree_list_first (resrc_tree_list);
            while (child) {
                resrc_tree_destroy (child, destroy_resrc);
                child = resrc_tree_list_next (resrc_tree_list);
            }
        }
        resrc_tree_list_free (resrc_tree_list);
    }
}

int resrc_tree_list_serialize (JSON o, resrc_tree_list_t *rtl, int64_t jobid)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (o && rtl && rtl->list) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (rt) {
            if ((rc = resrc_tree_serialize (o, rt, jobid)))
                break;
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

resrc_tree_list_t *resrc_tree_list_deserialize (JSON o)
{
    JSON ca = NULL;     /* array of child json objects */
    int i, nchildren = 0;
    resrc_tree_t *rt = NULL;
    resrc_tree_list_t *rtl = resrc_tree_list_new ();

    if (o && rtl) {
        if (Jget_ar_len (o, &nchildren)) {
            for (i=0; i < nchildren; i++) {
                Jget_ar_obj (o, i, &ca);
                rt = resrc_tree_deserialize (ca, NULL);
                if (!rt || resrc_tree_list_append (rtl, rt))
                    break;
            }
        }
    }

    return rtl;
}

int resrc_tree_list_allocate (resrc_tree_list_t *rtl, int64_t job_id,
                              int64_t starttime, int64_t endtime)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_allocate (rt, job_id, starttime, endtime);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_allocate_dynamic (resrc_tree_list_t *rtl, int64_t job_id,
                                      int64_t starttime, int64_t endtime)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_allocate_dynamic (rt, job_id, starttime, endtime);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}


int resrc_tree_list_reserve (resrc_tree_list_t *rtl, int64_t job_id,
                             int64_t starttime, int64_t endtime)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_reserve (rt, job_id, starttime, endtime);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_release (resrc_tree_list_t *rtl, int64_t job_id)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_release (rt, job_id);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

int resrc_tree_list_release_all_reservations (resrc_tree_list_t *rtl)
{
    resrc_tree_t *rt;
    int rc = -1;

    if (rtl) {
        rc = 0;
        rt = resrc_tree_list_first (rtl);
        while (!rc && rt) {
            rc = resrc_tree_release_all_reservations (rt);
            rt = resrc_tree_list_next (rtl);
        }
    }

    return rc;
}

void resrc_tree_list_unstage_resources (resrc_tree_list_t *rtl)
{
    resrc_tree_t *rt;

    if (rtl) {
        rt = resrc_tree_list_first (rtl);
        while (rt) {
            resrc_tree_unstage_resources (rt);
            rt = resrc_tree_list_next (rtl);
        }
    }
}

resrc_t *resrc_find_by_id_tree (resrc_tree_t *resrc_tree, int64_t id)
{
    resrc_t *resrc;

    if (resrc_tree) {
        resrc = resrc_tree_resrc (resrc_tree);
        if (resrc_id (resrc) == id)
            return resrc;
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child_tree = resrc_tree_list_first (resrc_tree->children);
            while (child_tree) {
                resrc = resrc_find_by_id_tree (child_tree, id);
                if (resrc) 
                    return resrc;
                child_tree = resrc_tree_list_next (resrc_tree->children);
            }    
        }
    }

    return NULL;
}

resrc_t *resrc_tree_by_id_tree_list (resrc_tree_list_t *resrc_tree_list, int64_t id)
{
    resrc_t *resrc = NULL;
    resrc_tree_t *resrc_tree = resrc_tree_list_first (resrc_tree_list);
    while (resrc_tree) {
        resrc = resrc_find_by_id_tree (resrc_tree, id);
        if (resrc)
            return resrc;
        resrc_tree = resrc_tree_list_next (resrc_tree_list);
    }

    return NULL;

}

int resrc_hash_by_uuid (resrc_tree_t *resrc_tree, zhash_t *hash_table)
{
    int rc = 0;
    resrc_t *resrc = NULL;
    if (resrc_tree) {
        resrc = resrc_tree_resrc (resrc_tree);
        char uuid[40];
        resrc_uuid (resrc, uuid);
        //uuid_unparse (resrc->uuid, uuid);
        zhash_insert (hash_table, uuid, (void *)resrc);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child_tree = resrc_tree_list_first (resrc_tree->children);
            while (child_tree) {
                resrc_hash_by_uuid (child_tree, hash_table);
                child_tree = resrc_tree_list_next (resrc_tree->children);
            }
        } 

    }

    return rc;
}

int resrc_hash_by_uuid_list (resrc_tree_list_t *resrc_tree_list, zhash_t *hash_table)
{
    int rc = 0;
    
    resrc_tree_t *resrc_tree = resrc_tree_list_first (resrc_tree_list);
    while (resrc_tree) {
        resrc_hash_by_uuid (resrc_tree, hash_table);
        resrc_tree = resrc_tree_list_next (resrc_tree_list);
    }
    return rc;
}

int resrc_tree_set_owner (resrc_tree_t *resrc_tree, int64_t owner)
{
    //printf ("so entered in function\n");
    int rc = 0;
    resrc_t *resrc = NULL;
    if (resrc_tree) {
        //printf ("so entered inside if\n");
        resrc = resrc_tree_resrc (resrc_tree);
        //printf ("so obtained resrc\n");
        resrc_set_owner (resrc, owner);
        //printf ("so set owner for this resource\n");
        if (resrc_tree_num_children (resrc_tree)) {
            //printf ("so resrc has children\n");
            resrc_tree_t *child_tree = resrc_tree_list_first (resrc_tree->children);
            //printf ("so obtained first tree\n");
            while (child_tree) {
                //printf ("so calling recursive call\n");
                resrc_tree_set_owner (child_tree, owner);
                child_tree = resrc_tree_list_next (resrc_tree->children);
                //printf ("so obtained next list\n");
            }
        }
        //printf("so everything ok\n");
    }
    return rc;
}


int resrc_tree_destroy_returned_resources (resrc_tree_t *resrc_tree, zhash_t *hash_table)
{
    int rc = 0;
    resrc_t *resrc = NULL;
    if (resrc_tree) {
        resrc = resrc_tree_resrc (resrc_tree);
        if (resrc_tree_num_children (resrc_tree)) {
            resrc_tree_t *child_tree = resrc_tree_list_first (resrc_tree->children);
            while (child_tree) {
                resrc_tree_destroy_returned_resources (child_tree, hash_table);
                child_tree = resrc_tree_list_next (resrc_tree->children);
            }
            if ((!(resrc_tree_num_children (resrc_tree))) && (resrc_check_resource_destroy_ready (resrc))) {
                // delete it
                printf ("destroyreturnedresource: deleting a resource that just lost all its children\n");
                char uuid[40];
                resrc_uuid (resrc, uuid);
                zhash_delete (hash_table, uuid);
                resrc_tree_destroy (resrc_tree, true);
            }
        } else {
            if (resrc_check_resource_destroy_ready (resrc)) {
                printf ("destroyreturnedresource: deleting resource ready for deletion\n");
                char uuid[40];
                resrc_uuid (resrc, uuid);
                zhash_delete (hash_table, uuid);
                resrc_tree_destroy (resrc_tree, true);
            } 
        }
    }
    return rc;
}

resrc_tree_list_t* resrc_split_resources (resrc_tree_list_t *found_tree_list, resrc_tree_list_t *job_tree_list, JSON ret_array, int64_t jobid)
{
    int rc = -1;
    int found = 0;

    resrc_tree_list_t *to_serialize_tree_list = resrc_tree_list_new ();

    resrc_tree_t *resrc_tree = resrc_tree_list_first (found_tree_list);
    printf ("gogsladfjalskdfj\n"); fflush (0);
    while (resrc_tree) {
        resrc_t *new_resrc = resrc_tree_resrc (resrc_tree);
        resrc_tree_t *job_tree = resrc_tree_list_first (job_tree_list);
        printf ("okie doklaksdjfl asdlkjfas asd flkasjdf\n"); fflush (0);
        while (job_tree) {
            resrc_t *job_resrc = resrc_tree_resrc (job_tree);
#if 1
            if (new_resrc == job_resrc) {
                // The new node and old node are same
                found = 1;
                int add = resrc_compare (resrc_tree, job_tree, ret_array);
#if 1
                if (add) {
                    resrc_tree_list_append (to_serialize_tree_list, resrc_tree);
                }
#endif
                break;
            }
#endif
nextjobtree:
            job_tree = resrc_tree_list_next (job_tree_list);
        }
    
        if (to_serialize_tree_list == NULL) {
            printf ("YESSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS\n");
        }
        fflush(0);
#if 1
        if (!found) {
            // The new node was not found in any of the job tree
            // This needs to be serialized
            printf ("Going to appendddddddddddddd\n"); fflush(0);
            resrc_tree_list_append (to_serialize_tree_list, resrc_tree);

        }
#endif
next:
        found = 0;
        resrc_tree = resrc_tree_list_next (found_tree_list);
    }

    rc = 0;
    return to_serialize_tree_list;
}

// dealing with cores here
int resrc_compare (resrc_tree_t *new_tree, resrc_tree_t *job_tree, JSON ret_array)
{
    int rc = 0;
    int found = 0;

    resrc_tree_t *child = resrc_tree_list_first (new_tree->children);
    while (child) {
        resrc_t *child_resrc = resrc_tree_resrc (child);
        resrc_tree_t *job_child = resrc_tree_list_first (job_tree->children);
        while (job_child) {
            resrc_t *job_child_resrc = resrc_tree_resrc (job_child);
            if (child_resrc == job_child_resrc) {
                found = 1;
                char uuid[40] = {0};
                resrc_uuid (child_resrc, uuid);
                Jadd_ar_str (ret_array, uuid);
                goto nextone;
            }
            job_child = resrc_tree_list_next (job_tree->children);
        }

        if (!found) {
            rc = 1;
        }
nextone:
        found = 0;
        child = resrc_tree_list_next (new_tree->children);
    }



    return rc;
}


/*
 * vi: ts=4 sw=4 expandtab
 */

