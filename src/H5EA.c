/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*-------------------------------------------------------------------------
 *
 * Created:		H5EA.c
 *			Jun 17 2008
 *			Quincey Koziol <koziol@hdfgroup.org>
 *
 * Purpose:		Implements an "extensible array" for storing elements
 *                      in an array whose high bounds can extend and shrink.
 *
 *                      Please see the documentation in:
 *                      doc/html/TechNotes/ExtensibleArray.html for a full
 *                      description of how they work, etc.
 *
 *-------------------------------------------------------------------------
 */

/**********************/
/* Module Declaration */
/**********************/

#include "H5EAmodule.h"         /* This source code file is part of the H5EA module */


/***********************/
/* Other Packages Used */
/***********************/


/***********/
/* Headers */
/***********/
#include "H5private.h"		/* Generic Functions			*/
#include "H5Eprivate.h"		/* Error handling		  	*/
#include "H5EApkg.h"		/* Extensible Arrays			*/
#include "H5FLprivate.h"	/* Free Lists                           */
#include "H5VMprivate.h"        /* Vector functions			*/


/****************/
/* Local Macros */
/****************/


/******************/
/* Local Typedefs */
/******************/

/* Typedef for generically unprotecting an object */
typedef herr_t (*H5EA__unprotect_func_t)(void *thing, hid_t dxpl_id,
    unsigned cache_flags);


/********************/
/* Package Typedefs */
/********************/


/********************/
/* Local Prototypes */
/********************/


/*********************/
/* Package Variables */
/*********************/

/* Package initialization variable */
hbool_t H5_PKG_INIT_VAR = FALSE;

/* Extensible array client ID to class mapping */

/* Remember to add client ID to H5EA_cls_id_t in H5EAprivate.h when adding a new
 * client class..
 */
const H5EA_class_t *const H5EA_client_class_g[] = {
    H5EA_CLS_TEST,		/* ? - H5EA_CLS_TEST_ID			*/
};


/*****************************/
/* Library Private Variables */
/*****************************/


/*******************/
/* Local Variables */
/*******************/

/* Declare a free list to manage the H5EA_t struct */
H5FL_DEFINE_STATIC(H5EA_t);



/*-------------------------------------------------------------------------
 * Function:	H5EA_create
 *
 * Purpose:	Creates a new empty extensible array in the file.
 *
 * Return:	Pointer to earray wrapper on success
 *              NULL on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Jun 17 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
H5EA_t *, NULL, NULL,
H5EA_create(H5F_t *f, hid_t dxpl_id, const H5EA_create_t *cparam, void *ctx_udata))

    /* Local variables */
    H5EA_t *ea = NULL;          /* Pointer to new extensible array */
    H5EA_hdr_t *hdr = NULL;     /* The extensible array header information */
    haddr_t ea_addr;            /* Array header address */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(f);
    HDassert(cparam);

    /* H5EA interface sanity check */
    HDcompile_assert(H5EA_NUM_CLS_ID == NELMTS(H5EA_client_class_g));

    /* Create extensible array header */
    if(HADDR_UNDEF == (ea_addr = H5EA__hdr_create(f, dxpl_id, cparam, ctx_udata)))
	H5E_THROW(H5E_CANTINIT, "can't create extensible array header")

    /* Allocate extensible array wrapper */
    if(NULL == (ea = H5FL_MALLOC(H5EA_t)))
	H5E_THROW(H5E_CANTALLOC, "memory allocation failed for extensible array info")

    /* Lock the array header into memory */
    if(NULL == (hdr = H5EA__hdr_protect(f, dxpl_id, ea_addr, ctx_udata, H5AC__NO_FLAGS_SET)))
	H5E_THROW(H5E_CANTPROTECT, "unable to load extensible array header")

    /* Point extensible array wrapper at header and bump it's ref count */
    ea->hdr = hdr;
    if(H5EA__hdr_incr(ea->hdr) < 0)
	H5E_THROW(H5E_CANTINC, "can't increment reference count on shared array header")

    /* Increment # of files using this array header */
    if(H5EA__hdr_fuse_incr(ea->hdr) < 0)
	H5E_THROW(H5E_CANTINC, "can't increment file reference count on shared array header")

    /* Set file pointer for this array open context */
    ea->f = f;

    /* Set the return value */
    ret_value = ea;

CATCH

    if(hdr && H5EA__hdr_unprotect(hdr, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
	H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array header")
    if(!ret_value)
        if(ea && H5EA_close(ea, dxpl_id) < 0)
            H5E_THROW(H5E_CLOSEERROR, "unable to close extensible array")

END_FUNC(PRIV)  /* end H5EA_create() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_open
 *
 * Purpose:	Opens an existing extensible array in the file.
 *
 * Return:	Pointer to array wrapper on success
 *              NULL on failure
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Aug 28 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
H5EA_t *, NULL, NULL,
H5EA_open(H5F_t *f, hid_t dxpl_id, haddr_t ea_addr, void *ctx_udata))

    /* Local variables */
    H5EA_t *ea = NULL;          /* Pointer to new extensible array wrapper */
    H5EA_hdr_t *hdr = NULL;     /* The extensible array header information */

    /*
     * Check arguments.
     */
    HDassert(f);
    HDassert(H5F_addr_defined(ea_addr));

    /* Load the array header into memory */
#ifdef QAK
HDfprintf(stderr, "%s: ea_addr = %a\n", FUNC, ea_addr);
#endif /* QAK */
    if(NULL == (hdr = H5EA__hdr_protect(f, dxpl_id, ea_addr, ctx_udata, H5AC__READ_ONLY_FLAG)))
        H5E_THROW(H5E_CANTPROTECT, "unable to load extensible array header, address = %llu", (unsigned long long)ea_addr)

    /* Check for pending array deletion */
    if(hdr->pending_delete)
        H5E_THROW(H5E_CANTOPENOBJ, "can't open extensible array pending deletion")

    /* Create fractal heap info */
    if(NULL == (ea = H5FL_MALLOC(H5EA_t)))
        H5E_THROW(H5E_CANTALLOC, "memory allocation failed for extensible array info")

    /* Point extensible array wrapper at header */
    ea->hdr = hdr;
    if(H5EA__hdr_incr(ea->hdr) < 0)
        H5E_THROW(H5E_CANTINC, "can't increment reference count on shared array header")

    /* Increment # of files using this array header */
    if(H5EA__hdr_fuse_incr(ea->hdr) < 0)
        H5E_THROW(H5E_CANTINC, "can't increment file reference count on shared array header")

    /* Set file pointer for this array open context */
    ea->f = f;

    /* Set the return value */
    ret_value = ea;

CATCH

    if(hdr && H5EA__hdr_unprotect(hdr, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array header")
    if(!ret_value)
        if(ea && H5EA_close(ea, dxpl_id) < 0)
            H5E_THROW(H5E_CLOSEERROR, "unable to close extensible array")

END_FUNC(PRIV)  /* end H5EA_open() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_get_nelmts
 *
 * Purpose:	Query the current number of elements in array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Aug 21 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, NOERR,
herr_t, SUCCEED, -,
H5EA_get_nelmts(const H5EA_t *ea, hsize_t *nelmts))

    /* Local variables */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(nelmts);

    /* Retrieve the max. index set */
    *nelmts = ea->hdr->stats.stored.max_idx_set;

END_FUNC(PRIV)  /* end H5EA_get_nelmts() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_get_addr
 *
 * Purpose:	Query the address of the array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Aug 21 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, NOERR,
herr_t, SUCCEED, -,
H5EA_get_addr(const H5EA_t *ea, haddr_t *addr))

    /* Local variables */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(ea->hdr);
    HDassert(addr);

    /* Retrieve the address of the extensible array's header */
    *addr = ea->hdr->addr;

END_FUNC(PRIV)  /* end H5EA_get_addr() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_lookup_elmt
 *
 * Purpose:	Retrieve the metadata object and the element buffer for a
 *              given element in the array.
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Sep  9 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(STATIC, ERR,
herr_t, SUCCEED, FAIL,
H5EA__lookup_elmt(const H5EA_t *ea, hid_t dxpl_id, hsize_t idx, unsigned thing_acc,
    void **thing, uint8_t **thing_elmt_buf, hsize_t *thing_elmt_idx,
    H5EA__unprotect_func_t *thing_unprot_func))

    /* Local variables */
    H5EA_hdr_t *hdr = ea->hdr;          /* Header for EA */
    H5EA_iblock_t *iblock = NULL;       /* Pointer to index block for EA */
    H5EA_sblock_t *sblock = NULL;       /* Pointer to super block for EA */
    H5EA_dblock_t *dblock = NULL;       /* Pointer to data block for EA */
    H5EA_dblk_page_t *dblk_page = NULL; /* Pointer to data block page for EA */
    unsigned iblock_cache_flags = H5AC__NO_FLAGS_SET;   /* Flags to unprotecting index block */
    unsigned sblock_cache_flags = H5AC__NO_FLAGS_SET;   /* Flags to unprotecting super block */
    hbool_t stats_changed = FALSE;      /* Whether array statistics changed */
    hbool_t hdr_dirty = FALSE;          /* Whether the array header changed */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
HDfprintf(stderr, "%s: Index %Hu\n", FUNC, idx);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(hdr);
    HDassert(thing);
    HDassert(thing_elmt_buf);
    HDassert(thing_unprot_func);

    /* only the H5AC__READ_ONLY_FLAG may be set in thing_acc */
    HDassert((thing_acc & (unsigned)(~H5AC__READ_ONLY_FLAG)) == 0);

    /* Set the shared array header's file context for this operation */
    hdr->f = ea->f;

    /* Reset the pointers to the 'thing' info */
    *thing = NULL;
    *thing_elmt_buf = NULL;
    *thing_elmt_idx = 0;
    *thing_unprot_func = (H5EA__unprotect_func_t)NULL;

    /* Check if we should create the index block */
    if(!H5F_addr_defined(hdr->idx_blk_addr)) {
#ifdef QAK
HDfprintf(stderr, "%s: Index block address not defined!\n", FUNC, idx);
#endif /* QAK */
        /* Check if we are allowed to create the thing */
        if(0 == (thing_acc & H5AC__READ_ONLY_FLAG)) { /* i.e. r/w access */
            /* Create the index block */
            hdr->idx_blk_addr = H5EA__iblock_create(hdr, dxpl_id, &stats_changed);
            if(!H5F_addr_defined(hdr->idx_blk_addr))
                H5E_THROW(H5E_CANTCREATE, "unable to create index block")
            hdr_dirty = TRUE;
        } /* end if */
        else
            H5_LEAVE(SUCCEED)
    } /* end if */
#ifdef QAK
HDfprintf(stderr, "%s: Index block address is: %a\n", FUNC, hdr->idx_blk_addr);
#endif /* QAK */

    /* Protect index block */
    if(NULL == (iblock = H5EA__iblock_protect(hdr, dxpl_id, thing_acc)))
        H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array index block, address = %llu", (unsigned long long)hdr->idx_blk_addr)

    /* Check if element is in index block */
    if(idx < hdr->cparam.idx_blk_elmts) {
        /* Set 'thing' info to refer to the index block */
        *thing = iblock;
        *thing_elmt_buf = (uint8_t *)iblock->elmts;
        *thing_elmt_idx = idx;
        *thing_unprot_func = (H5EA__unprotect_func_t)H5EA__iblock_unprotect;
    } /* end if */
    else {
        unsigned sblk_idx;      /* Which superblock does this index fall in? */
        size_t dblk_idx;        /* Data block index */
        hsize_t elmt_idx;       /* Offset of element in super block */

        /* Get super block index where element is located */
        sblk_idx = H5EA__dblock_sblk_idx(hdr, idx);
#ifdef QAK
HDfprintf(stderr, "%s: sblk_idx = %u, iblock->nsblks = %Zu\n", FUNC, sblk_idx, iblock->nsblks);
#endif /* QAK */

        /* Adjust index to offset in super block */
        elmt_idx = idx - (hdr->cparam.idx_blk_elmts + hdr->sblk_info[sblk_idx].start_idx);
#ifdef QAK
HDfprintf(stderr, "%s: after adjusting for super block elements, elmt_idx = %Hu\n", FUNC, elmt_idx);
#endif /* QAK */

        /* Check for data block containing element address in the index block */
        if(sblk_idx < iblock->nsblks) {
#ifdef QAK
HDfprintf(stderr, "%s: Element in data block pointed to by address in index block\n", FUNC);
#endif /* QAK */
            /* Compute the data block index in index block */
            dblk_idx = (size_t)(hdr->sblk_info[sblk_idx].start_dblk + (elmt_idx / hdr->sblk_info[sblk_idx].dblk_nelmts));
#ifdef QAK
HDfprintf(stderr, "%s: dblk_idx = %u, iblock->ndblk_addrs = %Zu\n", FUNC, dblk_idx, iblock->ndblk_addrs);
#endif /* QAK */
            HDassert(dblk_idx < iblock->ndblk_addrs);

            /* Check if the data block has been allocated on disk yet */
            if(!H5F_addr_defined(iblock->dblk_addrs[dblk_idx])) {
                /* Check if we are allowed to create the thing */
                if(0 == (thing_acc & H5AC__READ_ONLY_FLAG)) { /* i.e. r/w access */
                    haddr_t dblk_addr;        /* Address of data block created */
                    hsize_t dblk_off;         /* Offset of data block in array */

                    /* Create data block */
                    dblk_off = hdr->sblk_info[sblk_idx].start_idx + (dblk_idx * hdr->sblk_info[sblk_idx].dblk_nelmts);
                    dblk_addr = H5EA__dblock_create(hdr, dxpl_id, iblock, &stats_changed, dblk_off, hdr->sblk_info[sblk_idx].dblk_nelmts);
                    if(!H5F_addr_defined(dblk_addr))
                        H5E_THROW(H5E_CANTCREATE, "unable to create extensible array data block")

                    /* Set data block address in index block */
                    iblock->dblk_addrs[dblk_idx] = dblk_addr;
                    iblock_cache_flags |= H5AC__DIRTIED_FLAG;
                } /* end if */
                else
                    H5_LEAVE(SUCCEED)
            } /* end if */

            /* Protect data block */
            if(NULL == (dblock = H5EA__dblock_protect(hdr, dxpl_id, iblock, iblock->dblk_addrs[dblk_idx], hdr->sblk_info[sblk_idx].dblk_nelmts, thing_acc)))
                H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array data block, address = %llu", (unsigned long long)iblock->dblk_addrs[dblk_idx])

            /* Adjust index to offset in data block */
            elmt_idx %= hdr->sblk_info[sblk_idx].dblk_nelmts;

            /* Set 'thing' info to refer to the data block */
            *thing = dblock;
            *thing_elmt_buf = (uint8_t *)dblock->elmts;
            *thing_elmt_idx = elmt_idx;
            *thing_unprot_func = (H5EA__unprotect_func_t)H5EA__dblock_unprotect;
        } /* end if */
        else {
            size_t sblk_off;  /* Offset of super block in index block array of super blocks */

            /* Calculate offset of super block in index block's array */
            sblk_off = sblk_idx - iblock->nsblks;

            /* Check if the super block has been allocated on disk yet */
            if(!H5F_addr_defined(iblock->sblk_addrs[sblk_off])) {
                /* Check if we are allowed to create the thing */
                if(0 == (thing_acc & H5AC__READ_ONLY_FLAG)) { /* i.e. r/w access */
                    haddr_t sblk_addr;        /* Address of data block created */

                    /* Create super block */
                    sblk_addr = H5EA__sblock_create(hdr, dxpl_id, iblock, &stats_changed, sblk_idx);
#ifdef QAK
HDfprintf(stderr, "%s: New super block address is: %a\n", FUNC, sblk_addr);
#endif /* QAK */
                    if(!H5F_addr_defined(sblk_addr))
                        H5E_THROW(H5E_CANTCREATE, "unable to create extensible array super block")

                    /* Set super block address in index block */
                    iblock->sblk_addrs[sblk_off] = sblk_addr;
                    iblock_cache_flags |= H5AC__DIRTIED_FLAG;
                } /* end if */
                else
                    H5_LEAVE(SUCCEED)
            } /* end if */

            /* Protect super block */
            if(NULL == (sblock = H5EA__sblock_protect(hdr, dxpl_id, iblock, iblock->sblk_addrs[sblk_off], sblk_idx, thing_acc)))
                H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array super block, address = %llu", (unsigned long long)iblock->sblk_addrs[sblk_off])

            /* Compute the data block index in super block */
            dblk_idx = (size_t)(elmt_idx / sblock->dblk_nelmts);
#ifdef QAK
HDfprintf(stderr, "%s: dblk_idx = %u, sblock->ndblks = %Zu\n", FUNC, dblk_idx, sblock->ndblks);
#endif /* QAK */
            HDassert(dblk_idx < sblock->ndblks);

            /* Check if the data block has been allocated on disk yet */
            if(!H5F_addr_defined(sblock->dblk_addrs[dblk_idx])) {
                /* Check if we are allowed to create the thing */
                if(0 == (thing_acc & H5AC__READ_ONLY_FLAG)) { /* i.e. r/w access */
                    haddr_t dblk_addr;        /* Address of data block created */
                    hsize_t dblk_off;         /* Offset of data block in array */

                    /* Create data block */
                    dblk_off = hdr->sblk_info[sblk_idx].start_idx + (dblk_idx * hdr->sblk_info[sblk_idx].dblk_nelmts);
                    dblk_addr = H5EA__dblock_create(hdr, dxpl_id, sblock, &stats_changed, dblk_off, sblock->dblk_nelmts);
                    if(!H5F_addr_defined(dblk_addr))
                        H5E_THROW(H5E_CANTCREATE, "unable to create extensible array data block")

                    /* Set data block address in index block */
                    sblock->dblk_addrs[dblk_idx] = dblk_addr;
                    sblock_cache_flags |= H5AC__DIRTIED_FLAG;
                } /* end if */
                else
                    H5_LEAVE(SUCCEED)
            } /* end if */

#ifdef QAK
if(sblock->dblk_npages)
    HDfprintf(stderr, "%s: Check 1.0: elmt_idx = %Hu\n", FUNC, elmt_idx);
#endif /* QAK */
            /* Adjust index to offset in data block */
            elmt_idx %= sblock->dblk_nelmts;
#ifdef QAK
if(sblock->dblk_npages)
    HDfprintf(stderr, "%s: Check 2.0: elmt_idx = %Hu\n", FUNC, elmt_idx);
#endif /* QAK */

            /* Check if the data block is paged */
            if(sblock->dblk_npages) {
                haddr_t dblk_page_addr;         /* Address of data block page */
                size_t page_idx;                /* Index of page within data block */
                size_t page_init_idx;           /* Index of 'page init' bit */

                /* Compute page index */
                page_idx = (size_t)elmt_idx / hdr->dblk_page_nelmts;

                /* Compute 'page init' index */
                page_init_idx = (dblk_idx * sblock->dblk_npages) + page_idx;

                /* Adjust index to offset in data block page */
                elmt_idx %= hdr->dblk_page_nelmts;

                /* Compute data block page address */
                dblk_page_addr = sblock->dblk_addrs[dblk_idx] +
                        H5EA_DBLOCK_PREFIX_SIZE(sblock) +
                        (page_idx * sblock->dblk_page_size);
#ifdef QAK
HDfprintf(stderr, "%s: sblock->addr = %a\n", FUNC, sblock->addr);
HDfprintf(stderr, "%s: sblock->dblk_addrs[%Zu] = %a\n", FUNC, dblk_idx, sblock->dblk_addrs[dblk_idx]);
HDfprintf(stderr, "%s: H5EA_DBLOCK_PREFIX_SIZE(sblock) = %u\n", FUNC, (unsigned)H5EA_DBLOCK_PREFIX_SIZE(sblock));
HDfprintf(stderr, "%s: sblock->page_init[%Zu] = %t\n", FUNC, page_init_idx, H5VM_bit_get(sblock->page_init, page_init_idx));
HDfprintf(stderr, "%s: page_idx = %Zu, elmt_idx = %Hu, dblk_page_addr = %a\n", FUNC, page_idx, elmt_idx, dblk_page_addr);
HDfprintf(stderr, "%s: sblock->dblk_page_size = %Zu\n", FUNC, sblock->dblk_page_size);
#endif /* QAK */

                /* Check if page has been initialized yet */
                if(!H5VM_bit_get(sblock->page_init, page_init_idx)) {
                    /* Check if we are allowed to create the thing */
                    if(0 == (thing_acc & H5AC__READ_ONLY_FLAG)) { /* i.e. r/w access */
                        /* Create the data block page */
                        if(H5EA__dblk_page_create(hdr, dxpl_id, sblock, dblk_page_addr) < 0)
                            H5E_THROW(H5E_CANTCREATE, "unable to create data block page")

                        /* Mark data block page as initialized in super block */
                        H5VM_bit_set(sblock->page_init, page_init_idx, TRUE);
                        sblock_cache_flags |= H5AC__DIRTIED_FLAG;
                    } /* end if */
                    else
                        H5_LEAVE(SUCCEED)
                } /* end if */

                /* Protect data block page */
                if(NULL == (dblk_page = H5EA__dblk_page_protect(hdr, dxpl_id, sblock, dblk_page_addr, thing_acc)))
                    H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array data block page, address = %llu", (unsigned long long)dblk_page_addr)

                /* Set 'thing' info to refer to the data block page */
                *thing = dblk_page;
                *thing_elmt_buf = (uint8_t *)dblk_page->elmts;
                *thing_elmt_idx = elmt_idx;
                *thing_unprot_func = (H5EA__unprotect_func_t)H5EA__dblk_page_unprotect;
            } /* end if */
            else {
                /* Protect data block */
                if(NULL == (dblock = H5EA__dblock_protect(hdr, dxpl_id, sblock, sblock->dblk_addrs[dblk_idx], sblock->dblk_nelmts, thing_acc)))
                    H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array data block, address = %llu", (unsigned long long)sblock->dblk_addrs[dblk_idx])

                /* Set 'thing' info to refer to the data block */
                *thing = dblock;
                *thing_elmt_buf = (uint8_t *)dblock->elmts;
                *thing_elmt_idx = elmt_idx;
                *thing_unprot_func = (H5EA__unprotect_func_t)H5EA__dblock_unprotect;
            } /* end else */
        } /* end else */
    } /* end else */

CATCH
    /* Reset 'thing' info on error */
    if(ret_value < 0) {
        *thing = NULL;
        *thing_elmt_buf = NULL;
        *thing_elmt_idx = 0;
        *thing_unprot_func = (H5EA__unprotect_func_t)NULL;
    } /* end if */

    /* Check for updating array statistics */
    if(stats_changed)
        hdr_dirty = TRUE;

    /* Check for header modified */
    if(hdr_dirty)
        if(H5EA__hdr_modified(hdr) < 0)
            H5E_THROW(H5E_CANTMARKDIRTY, "unable to mark extensible array header as modified")

    /* Release resources */
    if(iblock && *thing != iblock && H5EA__iblock_unprotect(iblock, dxpl_id, iblock_cache_flags) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array index block")
    /* (Note: super blocks don't contain elements, so don't have a '*thing != sblock' check) */
    if(sblock && H5EA__sblock_unprotect(sblock, dxpl_id, sblock_cache_flags) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array super block")
    if(dblock && *thing != dblock && H5EA__dblock_unprotect(dblock, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array data block")
    if(dblk_page && *thing != dblk_page && H5EA__dblk_page_unprotect(dblk_page, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array data block page")

END_FUNC(STATIC)  /* end H5EA_lookup_elmt() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_set
 *
 * Purpose:	Set an element of an extensible array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Sep  9 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_set(const H5EA_t *ea, hid_t dxpl_id, hsize_t idx, const void *elmt))

    /* Local variables */
    H5EA_hdr_t *hdr = ea->hdr;          /* Header for EA */
    void *thing = NULL;                 /* Pointer to the array metadata containing the array index we are interested in */
    uint8_t *thing_elmt_buf;            /* Pointer to the element buffer for the array metadata */
    hsize_t thing_elmt_idx;             /* Index of the element in the element buffer for the array metadata */
    H5EA__unprotect_func_t thing_unprot_func;   /* Function pointer for unprotecting the array metadata */
    unsigned thing_cache_flags = H5AC__NO_FLAGS_SET;   /* Flags for unprotecting array metadata */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
HDfprintf(stderr, "%s: Index %Hu\n", FUNC, idx);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(hdr);

    /* Set the shared array header's file context for this operation */
    hdr->f = ea->f;

    /* Look up the array metadata containing the element we want to set */
    if(H5EA__lookup_elmt(ea, dxpl_id, idx, H5AC__NO_FLAGS_SET, &thing, &thing_elmt_buf, &thing_elmt_idx, &thing_unprot_func) < 0)
        H5E_THROW(H5E_CANTPROTECT, "unable to protect array metadata")

    /* Sanity check */
    HDassert(thing);
    HDassert(thing_elmt_buf);
    HDassert(thing_unprot_func);

    /* Set element in thing's element buffer */
    HDmemcpy(thing_elmt_buf + (hdr->cparam.cls->nat_elmt_size * thing_elmt_idx), elmt, hdr->cparam.cls->nat_elmt_size);
    thing_cache_flags |= H5AC__DIRTIED_FLAG;

    /* Update max. element set in array, if appropriate */
#ifdef QAK
HDfprintf(stderr, "%s: idx = %Hu, hdr->stats.max_idx_set = %Hu\n", FUNC, idx, hdr->stats.max_idx_set);
#endif /* QAK */
    if(idx >= hdr->stats.stored.max_idx_set) {
        /* Update the max index for the array */
        hdr->stats.stored.max_idx_set = idx + 1;
        if(H5EA__hdr_modified(hdr) < 0)
            H5E_THROW(H5E_CANTMARKDIRTY, "unable to mark extensible array header as modified")
    } /* end if */

CATCH
    /* Release resources */
    if(thing && (thing_unprot_func)(thing, dxpl_id, thing_cache_flags) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array metadata")

END_FUNC(PRIV)  /* end H5EA_set() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_get
 *
 * Purpose:	Get an element of an extensible array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Sep 11 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_get(const H5EA_t *ea, hid_t dxpl_id, hsize_t idx, void *elmt))

    /* Local variables */
    H5EA_hdr_t *hdr = ea->hdr;          /* Header for EA */
    void *thing = NULL;                 /* Pointer to the array metadata containing the array index we are interested in */
    H5EA__unprotect_func_t thing_unprot_func;   /* Function pointer for unprotecting the array metadata */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
HDfprintf(stderr, "%s: Index %Hu\n", FUNC, idx);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(hdr);

    /* Check for element beyond max. element in array */
    if(idx >= hdr->stats.stored.max_idx_set) {
#ifdef QAK
HDfprintf(stderr, "%s: Element beyond max. index set, hdr->stats.max_idx_set = %Hu, idx = %Hu\n", FUNC, hdr->stats.max_idx_set, idx);
#endif /* QAK */
        /* Call the class's 'fill' callback */
        if((hdr->cparam.cls->fill)(elmt, (size_t)1) < 0)
            H5E_THROW(H5E_CANTSET, "can't set element to class's fill value")
    } /* end if */
    else {
        uint8_t *thing_elmt_buf;        /* Pointer to the element buffer for the array metadata */
        hsize_t thing_elmt_idx;         /* Index of the element in the element buffer for the array metadata */

#ifdef QAK
HDfprintf(stderr, "%s: Index block address is: %a\n", FUNC, hdr->idx_blk_addr);
#endif /* QAK */

        /* Set the shared array header's file context for this operation */
        hdr->f = ea->f;

        /* Look up the array metadata containing the element we want to set */
        if(H5EA__lookup_elmt(ea, dxpl_id, idx, H5AC__READ_ONLY_FLAG, &thing, &thing_elmt_buf, &thing_elmt_idx, &thing_unprot_func) < 0)
            H5E_THROW(H5E_CANTPROTECT, "unable to protect array metadata")

        /* Check if the thing holding the element has been created yet */
        if(NULL == thing) {
            /* Call the class's 'fill' callback */
            if((hdr->cparam.cls->fill)(elmt, (size_t)1) < 0)
                H5E_THROW(H5E_CANTSET, "can't set element to class's fill value")
        } /* end if */
        else
            /* Get element from thing's element buffer */
            HDmemcpy(elmt, thing_elmt_buf + (hdr->cparam.cls->nat_elmt_size * thing_elmt_idx), hdr->cparam.cls->nat_elmt_size);
    } /* end else */

CATCH
    /* Release thing */
    if(thing && (thing_unprot_func)(thing, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array metadata")

END_FUNC(PRIV)  /* end H5EA_get() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_depend
 *
 * Purpose:	Make a child flush dependency between the extensible array's
 *              header and another piece of metadata in the file.
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		May 27 2009
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_depend(H5AC_info_t *parent_entry, H5EA_t *ea))

    /* Local variables */
    H5EA_hdr_t *hdr = ea->hdr;          /* Header for EA */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(hdr);

    /* Set the shared array header's file context for this operation */
    hdr->f = ea->f;

    /* Set up flush dependency between parent entry and extensible array header */
    if(H5EA__create_flush_depend(parent_entry, (H5AC_info_t *)hdr) < 0)
        H5E_THROW(H5E_CANTDEPEND, "unable to create flush dependency on file metadata")

CATCH

END_FUNC(PRIV)  /* end H5EA_depend() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_undepend
 *
 * Purpose:	Remove a child flush dependency between the extensible array's
 *              header and another piece of metadata in the file.
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		May 27 2009
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_undepend(H5AC_info_t *parent_entry, H5EA_t *ea))

    /* Local variables */
    H5EA_hdr_t *hdr = ea->hdr;          /* Header for EA */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);
    HDassert(hdr);

    /* Set the shared array header's file context for this operation */
    hdr->f = ea->f;

    /* Remove flush dependency between parent entry and extensible array header */
    if(H5EA__destroy_flush_depend(parent_entry, (H5AC_info_t *)hdr) < 0)
        H5E_THROW(H5E_CANTUNDEPEND, "unable to destroy flush dependency on file metadata")

CATCH

END_FUNC(PRIV)  /* end H5EA_undepend() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_support
 *
 * Purpose:	Create a child flush dependency on the array metadata that
 *              contains the element for an array index.
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		May 21 2009
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_support(const H5EA_t *ea, hid_t dxpl_id, hsize_t idx, H5AC_info_t *child_entry))

    /* Local variables */
    void *thing = NULL;                 /* Pointer to the array metadata containing the array index we are interested in */
    uint8_t *thing_elmt_buf;            /* Pointer to the element buffer for the array metadata */
    hsize_t thing_elmt_idx;             /* Index of the element in the element buffer for the array metadata */
    H5EA__unprotect_func_t thing_unprot_func;   /* Function pointer for unprotecting the array metadata */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
HDfprintf(stderr, "%s: Index %Hu\n", FUNC, idx);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);

    /* Look up the array metadata containing the element we want to set */
    if(H5EA__lookup_elmt(ea, dxpl_id, idx, H5AC__NO_FLAGS_SET, &thing, &thing_elmt_buf, &thing_elmt_idx, &thing_unprot_func) < 0)
        H5E_THROW(H5E_CANTPROTECT, "unable to protect array metadata")

    /* Sanity check */
    HDassert(thing);
    HDassert(thing_elmt_buf);
    HDassert(thing_unprot_func);

    /* Set up flush dependency between child_entry and metadata array 'thing' */
    if(H5EA__create_flush_depend((H5AC_info_t *)thing, child_entry) < 0)
        H5E_THROW(H5E_CANTDEPEND, "unable to create flush dependency on array metadata")

CATCH
    /* Release resources */
    if(thing && (thing_unprot_func)(thing, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array metadata")

END_FUNC(PRIV)  /* end H5EA_support() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_unsupport
 *
 * Purpose:	Remove a flush dependency on the array metadata that contains
 *              the element for an array index.
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		May 21 2009
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_unsupport(const H5EA_t *ea, hid_t dxpl_id, hsize_t idx, H5AC_info_t *child_entry))

    /* Local variables */
    void *thing = NULL;                 /* Pointer to the array metadata containing the array index we are interested in */
    uint8_t *thing_elmt_buf;            /* Pointer to the element buffer for the array metadata */
    hsize_t thing_elmt_idx;             /* Index of the element in the element buffer for the array metadata */
    H5EA__unprotect_func_t thing_unprot_func;   /* Function pointer for unprotecting the array metadata */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
HDfprintf(stderr, "%s: Index %Hu\n", FUNC, idx);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);

    /* Look up the array metadata containing the element we want to set */
    if(H5EA__lookup_elmt(ea, dxpl_id, idx, H5AC__READ_ONLY_FLAG, &thing, &thing_elmt_buf, &thing_elmt_idx, &thing_unprot_func) < 0)
        H5E_THROW(H5E_CANTPROTECT, "unable to protect array metadata")

    /* Sanity check */
    HDassert(thing);
    HDassert(thing_elmt_buf);
    HDassert(thing_unprot_func);

    /* Remove flush dependency between child_entry and metadata array 'thing' */
    if(H5EA__destroy_flush_depend((H5AC_info_t *)thing, child_entry) < 0)
        H5E_THROW(H5E_CANTUNDEPEND, "unable to destroy flush dependency on array metadata")

CATCH
    /* Release resources */
    if(thing && (thing_unprot_func)(thing, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array metadata")

END_FUNC(PRIV)  /* end H5EA_unsupport() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_close
 *
 * Purpose:	Close an extensible array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Aug 21 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_close(H5EA_t *ea, hid_t dxpl_id))

    /* Local variables */
    hbool_t pending_delete = FALSE;     /* Whether the array is pending deletion */
    haddr_t ea_addr = HADDR_UNDEF;      /* Address of array (for deletion) */

#ifdef QAK
HDfprintf(stderr, "%s: Called\n", FUNC);
#endif /* QAK */

    /*
     * Check arguments.
     */
    HDassert(ea);

    /* Decrement file reference & check if this is the last open extensible array using the shared array header */
    if(0 == H5EA__hdr_fuse_decr(ea->hdr)) {
        /* Set the shared array header's file context for this operation */
        ea->hdr->f = ea->f;

        /* Shut down anything that can't be put in the header's 'flush' callback */

        /* Check for pending array deletion */
        if(ea->hdr->pending_delete) {
            /* Set local info, so array deletion can occur after decrementing the
             *  header's ref count
             */
            pending_delete = TRUE;
            ea_addr = ea->hdr->addr;
        } /* end if */
    } /* end if */

    /* Check for pending array deletion */
    if(pending_delete) {
        H5EA_hdr_t *hdr;            /* Another pointer to extensible array header */

#ifndef NDEBUG
{
    unsigned hdr_status = 0;         /* Header's status in the metadata cache */

    /* Check the header's status in the metadata cache */
    if(H5AC_get_entry_status(ea->f, ea_addr, &hdr_status) < 0)
        H5E_THROW(H5E_CANTGET, "unable to check metadata cache status for extensible array header")

    /* Sanity checks on header */
    HDassert(hdr_status & H5AC_ES__IN_CACHE);
    HDassert(hdr_status & H5AC_ES__IS_PINNED);
    HDassert(!(hdr_status & H5AC_ES__IS_PROTECTED));
}
#endif /* NDEBUG */

        /* Lock the array header into memory */
        /* (OK to pass in NULL for callback context, since we know the header must be in the cache) */
        if(NULL == (hdr = H5EA__hdr_protect(ea->f, dxpl_id, ea_addr, NULL, H5AC__NO_FLAGS_SET)))
            H5E_THROW(H5E_CANTLOAD, "unable to load extensible array header")

        /* Set the shared array header's file context for this operation */
        hdr->f = ea->f;

        /* Decrement the reference count on the array header */
        /* (don't put in H5EA_hdr_fuse_decr() as the array header may be evicted
         *  immediately -QAK)
         */
        if(H5EA__hdr_decr(ea->hdr) < 0)
            H5E_THROW(H5E_CANTDEC, "can't decrement reference count on shared array header")

        /* Delete array, starting with header (unprotects header) */
        if(H5EA__hdr_delete(hdr, dxpl_id) < 0)
            H5E_THROW(H5E_CANTDELETE, "unable to delete extensible array")
    } /* end if */
    else {
        /* Decrement the reference count on the array header */
        /* (don't put in H5EA_hdr_fuse_decr() as the array header may be evicted
         *  immediately -QAK)
         */
        if(H5EA__hdr_decr(ea->hdr) < 0)
            H5E_THROW(H5E_CANTDEC, "can't decrement reference count on shared array header")
    } /* end else */

    /* Release the extensible array wrapper */
    ea = (H5EA_t *)H5FL_FREE(H5EA_t, ea);

CATCH

END_FUNC(PRIV)  /* end H5EA_close() */


/*-------------------------------------------------------------------------
 * Function:	H5EA_delete
 *
 * Purpose:	Delete an extensible array
 *
 * Return:	SUCCEED/FAIL
 *
 * Programmer:	Quincey Koziol
 *		koziol@hdfgroup.org
 *		Aug 28 2008
 *
 *-------------------------------------------------------------------------
 */
BEGIN_FUNC(PRIV, ERR,
herr_t, SUCCEED, FAIL,
H5EA_delete(H5F_t *f, hid_t dxpl_id, haddr_t ea_addr, void *ctx_udata))

    /* Local variables */
    H5EA_hdr_t *hdr = NULL;             /* The fractal heap header information */

    /*
     * Check arguments.
     */
    HDassert(f);
    HDassert(H5F_addr_defined(ea_addr));

    /* Lock the array header into memory */
#ifdef QAK
HDfprintf(stderr, "%s: ea_addr = %a\n", FUNC, ea_addr);
#endif /* QAK */
    if(NULL == (hdr = H5EA__hdr_protect(f, dxpl_id, ea_addr, ctx_udata, H5AC__NO_FLAGS_SET)))
        H5E_THROW(H5E_CANTPROTECT, "unable to protect extensible array header, address = %llu", (unsigned long long)ea_addr)

    /* Check for files using shared array header */
    if(hdr->file_rc)
        hdr->pending_delete = TRUE;
    else {
        /* Set the shared array header's file context for this operation */
        hdr->f = f;

        /* Delete array now, starting with header (unprotects header) */
        if(H5EA__hdr_delete(hdr, dxpl_id) < 0)
            H5E_THROW(H5E_CANTDELETE, "unable to delete extensible array")
        hdr = NULL;
    } /* end if */

CATCH

    /* Unprotect the header, if an error occurred */
    if(hdr && H5EA__hdr_unprotect(hdr, dxpl_id, H5AC__NO_FLAGS_SET) < 0)
        H5E_THROW(H5E_CANTUNPROTECT, "unable to release extensible array header")

END_FUNC(PRIV)  /* end H5EA_delete() */

