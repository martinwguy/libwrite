/*
 *  Library to generate write files.
 *
 *  Code to manage the font face name table.
 *
 *  Public functions:
 *	Change to named font (may imply addition to table),
 *	returning font code.
 *  Private data:
 *	List of fonts used so far.
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */

#include <stdio.h>  /* for NULL */
#include <string.h> /* for strlen() ecc */
#include <memory.h> /* for memcpy() */
#include <malloc.h> /* for malloc() and free() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/* Structure definition for array of font face names. */
struct font {
    unsigned char ffid;	    /* font family identifier */
    char *font_name;	    /* Ascii name of font */
};

/* Our font face name table */
static struct font ffntb[MAX_FONTS] = {
    /* First element of list, the default font */
    {
	32,			/* ffid (from a file generate by Write) */
	"Arial"			/* font_name */
    }
};

/* How many slots in the font face name table are occupied? */
static int NFontsUsed = 1;

/* List of known font name/font family pairs. */
static struct font font_ffid[] = {
    { 32, "Arial" },
    { 48, "Courier" },
    { 16, "Dutch SWA" },
    { 32, "Helv" },
    { 48, "Modern" },
    { 16, "Roman" },
    { 48, "Roman 5cpi" },
    { 48, "Roman 6cpi" },
    { 48, "Roman 10cpi" },
    { 48, "Roman 12cpi" },
    { 48, "Roman 15cpi" },
    { 48, "Roman 17cpi" },
    { 48, "Roman 20cpi" },
    { 16, "Roman PS" },
    { 64, "Script" },
    { 80, "Symbol" },
    { 48, "Terminal" },
    { 16, "Tms Rmn" },
};

/*
 * Convert font name to font code.
 * If the font family ID is known, specify it, otherwise give 0 (unknown).
 * Returns font code (0..MAX_FONTS-1) or -1 on failure.
 */

int
_wri_cvt_font_name_to_code(char *font_name, unsigned char ffid)
{
    int i;

    /* See if it's already in the font face name table */
    for (i=0; i<NFontsUsed; i++) {
	/* Font names are case-independent */
	if (strcasecmp(ffntb[i].font_name, font_name) == 0) {
	    /* Found it! */
	    /* See whether we have better information now on its ffid */
	    if (ffntb[i].ffid == 0 && ffid != 0) {
		ffntb[i].ffid = ffid;
	    }
	    /* Return its code */
	    return(i);
	}
    }

    /* Not found - create new item and return code */

    /* But first check that three's space... */
    if (NFontsUsed >= MAX_FONTS) {
	/* Already using all font slots - return failure */
	return(-1);
    }

    /* Save a copy of the font name */
    ffntb[NFontsUsed].font_name = malloc(strlen(font_name)+1);
    if (ffntb[NFontsUsed].font_name == NULL) {
	/* Out of memory */
	_wri_error = 1;
	return(-1);
    }
    strcpy(ffntb[NFontsUsed].font_name, font_name);

    ffntb[NFontsUsed].ffid = ffid;

    if (ffid == 0) {
	/*
	 * Try to figure out font family codes.	 Probably, we should search
	 * font definition files somewhere, but that is too much like hard work.
	 */
	for (i=0; i < sizeof(font_ffid)/sizeof(font_ffid[0]); i++) {
	    if (strcasecmp(font_name, font_ffid[i].font_name) == 0) {
		/* Found it! Copy font code */
		ffntb[NFontsUsed].ffid = font_ffid[i].ffid;
		break;
	    }
	}
    }

    return(NFontsUsed++);
}

/*
 * Save font table.
 * Strategy: for each font, try to fit it into the current page.  If it won't
 * go, write out the full page and start building a new one.
 */
int
_wri_save_fonts(struct wri_header *hp, FILE *ofp)
{
    char page[PAGESIZE];    /* page of font info under construction */
    PN next_page;   /* In which page of the Write file will the next
			 * page of font information be written? */
    char *cp;		/* Where to write next char in page[] */
    struct font *ffntbp;/* pointer to current font always == &ffntb[i] */
    int i;

    /* Initialise for 1st page */
    next_page = hp->pnFfntb;	/* As yet no pages of font info output */
    if (_wri_seek_to_page(next_page, ofp)) return(1);

    /* Write in cffn at the start */
    *(int *)page = (int)NFontsUsed;
    cp = &page[2];	/* FFNs start straight after */

    for (i=0, ffntbp = ffntb; i<NFontsUsed; i++, ffntbp++) {
	int fnam_len;	/* length of font name, including the nul */

	fnam_len = strlen(ffntbp->font_name) + 1;

	/* If it'll fit in this page, put it in.  Otherwise write the page out
	 * and start a fresh one.  We must leave 2 bytes spare at the end for
	 * the 0xFFFF "more font info on the next page" or the 0 "end of FFNs
	 * word.  Size of FFN comprises an int, a char and the string.
	 */
	if ((&page[PAGESIZE] - cp) - 2 < 3 + fnam_len) {
	    /* Available space is too small.  Create page & write to disk. */

	    /* Indicate that there is more in the next page... */
	    *(int *)cp = 0xFFFF;

	    /* Write to output file */
	    if (_wri_write_page(page, ofp)) return(1);

	    next_page++;

	    /* And prepare for new page */
	    cp = page;
	}

	/* cbFfn */
	*(int *)cp = 1 + fnam_len;
	cp += sizeof(int);

	/* ffid */
	*cp++ = ffntbp->ffid;

	/* font name */
	memcpy(cp, ffntbp->font_name, (size_t)fnam_len);
	cp += fnam_len;
    }

    /* Write final page */
    *(int *)cp = 0;
    if (_wri_seek_to_page(next_page, ofp) || _wri_write_page(page, ofp)) return(1);
    next_page++;

    hp->pnMac = next_page;

    return(0);
}

int
_wri_reinit_font()
{
    int i;

    /* Free all font names except for the first one, which is static */
    for (i=1; i<NFontsUsed; i++) free(ffntb[i].font_name);

    NFontsUsed = 1; /* Just the default font */

    return(0);
}
