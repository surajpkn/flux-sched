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

/* trdl.c - test RDL C API */

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "src/common/libutil/log.h"
#include "src/common/libutil/jsonutil.h"
#include "rdl.h"

static void perr (void *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
}

void print_resource (struct resource *r, int pad)
{
    struct resource *c;

    fprintf (stdout, "%*s/%s=%d/%d\n", pad, "", rdl_resource_name (r),
            (int) rdl_resource_available (r),
            (int) rdl_resource_size (r));

    rdl_resource_iterator_reset (r);
    while ((c = rdl_resource_next_child (r))) {
        print_resource (c, pad+1);
        rdl_resource_destroy (c);
    }
}


int main (int argc, char *argv[])
{
    struct rdllib *l;
    struct rdl *rdl1, *rdl2;
    struct rdl_accumulator *a;
    struct resource *r, *c;
    int64_t val;

    const char *filename = argv[1];

    log_init (basename (argv[0]));
    rdllib_set_default_errf (NULL, &perr);

    if (!(l = rdllib_open ()))
        err_exit ("rdllib_open");

    if (filename == NULL || *filename == '\0')
        filename = getenv ("TESTRDL_INPUT_FILE");

    if (!(rdl1 = rdl_loadfile (l, filename)))
        err_exit ("loadfile: %s", filename);

    if (!(rdl2 = rdl_copy (rdl1)))
        err_exit ("copy");

    r = rdl_resource_get (rdl1, "default");
    if (rdl_resource_set_int (r, "test-tag", 5959) < 0)
        exit (1);
    rdl_resource_get_int (r, "test-tag", &val);
    if (val != 5959)
        exit (1);
     rdl_resource_delete_tag (r, "test-tag");

    c = rdl_resource_next_child (r);

    a = rdl_accumulator_create (rdl1);
    if (rdl_accumulator_add (a, c) < 0)
        exit (1);

    rdl2 = rdl_accumulator_copy (a);
    rdl_accumulator_destroy (a);

    print_resource (rdl_resource_get (rdl2, "default"), 0);

    /*
     *  Test find
     */
    json_object *args = util_json_object_new_object ();
    util_json_object_add_string (args, "type", "node");
    util_json_object_add_int (args, "id", 300);
    rdl2 = rdl_find (rdl1, args);
    json_object_put (args);
    r = rdl_resource_get (rdl2, "default");
    if (r == NULL)
        exit (1);


    c = rdl_resource_next_child (r);
    printf ("found %s\n", rdl_resource_name (c));

    rdl_resource_destroy (r);
    rdl_destroy (rdl2);

    r = rdl_resource_get (rdl1, "default:/hype/hype300/socket0/memory");
    if (r == NULL)
        exit (1);

    print_resource (r, 0);
    rdl_resource_alloc (r, 1024);
    printf ("After alloc:\n");
    print_resource (r, 0);
    rdl_resource_free (r, 1024);
    printf ("After free:\n");
    print_resource (r, 0);

    rdllib_close (l);

    log_fini ();

    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
