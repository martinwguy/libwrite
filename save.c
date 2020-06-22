/*
 *  Library to generate write files.
 *
 *  Output final write file
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */

#include <stdio.h>
#include <stdlib.h> /* for exit() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/* Function prototypes */
static int save_header(struct wri_header *hp, FILE *ofp);

int
wri_save(char *filename)
{
    /* Header is static for free zeroing of unused elements */
    static struct wri_header header;
    FILE *ofp;	/* Output file pointer */

    /* Create output file */
    ofp = fopen(filename, "wb");
    if (ofp == NULL) {
	_wri_error = 1;
	return(1);
    }

    /* In case they have started a header or footer and not finished it,
     * we close it for them.  If we're not in header/footer, it does nothing
     */
    wri_doc_return();

    /* These functions fill in their information in the header, so the
     * order in which they are called must correspond to the order of their
     * size fields in the header structure.
     */
    if (_wri_save_text(&header, ofp)) goto fail;
    if (_wri_save_chp(&header, ofp)) goto fail;
    if (_wri_save_pap(&header, ofp)) goto fail;

    header.pnSep = header.pnFntb;   /* No footnote table */

    if (_wri_save_section(&header, ofp)) goto fail;

    header.pnFfntb = header.pnPgtb; /* No page table */

    if (_wri_save_fonts(&header, ofp)) goto fail;
    if (save_header(&header, ofp)) goto fail;

    /* If the disk fills at the last sector, th only way to detect it is through
     * ferror() because the following happens:
     * the last fwrite() doesn't really write to the disk because of buffering.
     * This happens when we fseek() back to the beginning to write the
     * header; though the writing fails, the fseek in itself succeeds.
     * The trace of the failed write remains in ferror, however.
     */
    if (ferror(ofp)) goto fail;

    if (fclose(ofp) != 0) goto fail2;

    return(0);

fail:
    /* Something failed during writing of the file.
     * Remove the half-baked output file
     */
    (void) fclose(ofp);
fail2:
    (void) remove(filename);

    _wri_error = 1;
    return(1);
}

static int
save_header(struct wri_header *hp, FILE *ofp)
{
    hp->wIdent = WRIH_WIDENT;
    hp->wTool  = WRIH_WTOOL;

    if (_wri_seek_to_page((PN)0, ofp)) return(1);

    if (fwrite((void *)hp, (size_t) sizeof(*hp), (size_t) 1, ofp) != 1) {
	_wri_error = 1;
	return(1);
    }

    return(0);
}

/*
 *  Utility function for file-writers: seek to a particular page
 */
int
_wri_seek_to_page(PN n, FILE *fp)
{
    long offset = (long)n * PAGESIZE;

    if (fseek(fp, offset, SEEK_SET) != 0) {
	_wri_error = 1;
	return(1);
    }

    /* We must check ferror because if the seek implies writing the buffered
     * data, and the write fails, fseek returns success all the same.
     */
    if (ferror(fp)) {
	_wri_error = 1;
	return(1);
    }

    return(0);
}

int
_wri_write_page(char *page, FILE *ofp)
{
    if (fwrite(page, (size_t) PAGESIZE, (size_t) 1, ofp) != (size_t)1) {
	_wri_error = 1;
	return(1);
    }

    /* Defensive programming... */
    if (ferror(ofp)) {
	_wri_error = 1;
	return(1);
    }

    return(0);
}
