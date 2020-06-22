/*
 *  Library to generate write files.
 *
 *  Code for treatment of paragraph properties
 *
 *  Includes:
 *	Structure definitions to memorize the PAPs used in the document.
 *	User functions to set various paragraphs properties.
 *	Stuff to handle running head codes (page headers and footers).
 *	Stuff for definition of tab stops.
 *	Code to save PAP information in the final Write file.
 *	Internal functions for manipulation of list of PAPs.
 *
 *  Private data:
 *	list of PAPs defined for the text so far
 *
 *  Strategy:
 *	Treatment of PAPs is different from CHPs because modifications refer
 *	to the current paragraph, and can be effected at any time while we are
 *	creating the current paragraph.
 *	However, most paragraphs will have the same format, so we maintain a
 *	list of where each paragraph begins and ends, and a separate list of
 *	paragraph properties.  Several of the former can refer to one of the
 *	latter, and only when they change the paragraph properties do we
 *	create a new paragraph structure, and then only if the paragraph
 *	properties refer to more than one paragraph.
 *
 *	The first byte of each stored PAP is ignored by Write, and should be 0
 *	in the write file.  For PAPs in the list, it is used as a reference
 *	count: it records the number of elements of the lpap list that point
 *	to this PAP structure.
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */
#include <stdio.h>  /* for NULL */
#include <malloc.h> /* for malloc() */
#include <string.h> /* for memcpy() */
#include <memory.h> /* for memcpy() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Library-internal definitions */
#include "defs.h"

/*
 *  Type definitions
 */

/*
 * Structure definition for linked list of PAPs.
 * All elements of this list are mallocked, except the first element,
 * which is static.
 *
 * To save memory, since many consecutive paragraphs will have the same
 * format, several of the <pap> elements of this list may point to the same
 * PAP structure.  The unused first byte of the PAP structure is used as a
 * reference count, to say how many members of the lpap chain point at it.
 * This reference count is incremented each time we create a new lpap pointing
 * at an old PAP, and is used to know when to pass the memory of the PAP back
 * to free when we destroy the list as we write out the file.
 * When writing the file, these bytes should be set to 0.
 */

/* N.B.!!! We only store the first 17 bytes of the PAP in the linked list so as
 * to save memory - with the tab information its size would be 23 + 4 * 14 = 79
 * bytes. The only useful information in the missing 62 bytes is the tab stops,
 * which have a unique definition across all PAPs, and are copied into the PAPs
 * when saving to the output file.
 * This knowledge should only affect this source file - all others will deal in
 * full struct PAPs.
 */
#define STORED_PAP_SIZE 17  /* Up to the rhcPage..rhcFirst byte. */

/*
 * Element of linked list
 */
struct lpap {
    struct lpap *next;	/* Pointer to next element in list */
    CP cpFirst;		/* Index into text, start of this paragraph */
    CP cpLim;		/* Index into text, one past the last character of
			 * this paragraph */
    struct PAP *papp;	/* pointer to paragraph properties in full */
};

/*
 *  Function prototypes
 */

static int start_rhc(int header_footer);
static void copy_in_tabs(struct PAP *papp);
static struct PAP *this_para(void);
static struct PAP *clone_pap(void);

static void forget_paps(void);
static void memorize_pap(char *cchp);
static char *recall_pap(int cch, char *papp);

static void free_lpaps(struct lpap *from);
static int _wri_set_default_pap(void);
static void _wri_preserve_pap(void);
static int _wri_restore_pap(void);

/*
 *  Private data
 */

/* Default PAP values */
struct PAP _wri_default_pap = {
    0,	/* res1 */
    0,	/* jc */
    0,	/* res2 */
    0,	/* res3 */
    0,	/* res4 */
    0,	/* dxaRight */
    0,	/* dxaLeft */
    0,	/* dxaLeft1 */
    240,    /* dyaLine */
    /* the rest is all 0 */
};

/* First element of list, with the default PAP values */
static struct PAP first_pap = {
    1,	/* res1, also reference count */
    0,	/* jc */
    0,	/* res2 */
    0,	/* res3 */
    0,	/* res4 */
    0,	/* dxaRight */
    0,	/* dxaLeft */
    0,	/* dxaLeft1 */
    240,    /* dyaLine */
    /* the rest is all 0 */
};

/* First element of list, with the default PAP values */
static struct lpap lpap_first =	 {
    (struct lpap *) NULL,   /* next */
    (CP) 0,		    /* cpFirst */
    (CP) 0,		    /* cpLim */
    &first_pap,		    /* papp */
};

/* Current PAP (the last in the list) */
static struct lpap *lpap_curr = &lpap_first;

/* Easy way to reference the current PAP (usage: pap_curr.element) */
#define pap_curr (*(lpap_curr->papp))

/* Print header/footer on first page? (Default: no) */
static int pofp[2] = { 0, 0 };

/*
 *  Public functions
 */

/* Reset to default values (just like the menu option). */
int
wri_para_normal()
{
    return(
	wri_para_justify(WRI_LEFT) ||
	wri_para_interline(WRI_SINGLE) ||
	wri_para_indent_left((unsigned)0) ||
	wri_para_indent_right((unsigned)0) ||
	wri_para_indent_first((unsigned)0)
    );
}

/*
 * Set paragraph justification.
 * Parameter is one of WRI_LEFT, WRI_CENTER, WRI_RIGHT, WRI_BOTH
 * from libwrite.h (0, 1, 2 or 3)
 */
int
wri_para_justify(int jc)
{
    /* Check range */
    switch (jc) {
    case WRI_LEFT:
    case WRI_CENTER:
    case WRI_RIGHT:
    case WRI_BOTH:
	break;
    default:
	return(1);
    }

    /* Is value already correct?  If so, do nothing */
    if (pap_curr.jc == (unsigned)jc) {
	 return(0);
    }

    /* Otherwise make sure PAP refers to just this paragraph and set value */
    if (this_para() == NULL) return(1);
    pap_curr.jc = (unsigned)jc;
    return(0);
}

/*
 * Set inter-line spacing.
 *
 * <spacing> is one of WRI_SINGLE, WRI_ONE_1_2, WRI_DOUBLE
 */
int
wri_para_interline(int spacing)
{
    /* Check range */
    if (spacing < 0 || spacing > 32767) return(1);

    /* Is value already correct?  If so, do nothing */
    if (pap_curr.dyaLine == (unsigned)spacing) return(0);

    /* Otherwise make sure PAP refers to just this paragraph and set value */
    if (this_para() == NULL) return(1);
    pap_curr.dyaLine = spacing;
    return(0);
}

/* Set indents in TWIPs (1/20 * 1/72 inch) */
int
wri_para_indent_left(int indent)
{
    /* Check range */
    if (indent < 0 || indent > 32767) return(1);

    /* Is value already correct?  If so, do nothing */
    if (pap_curr.dxaLeft == (unsigned)indent) return(0);

    /* Otherwise make sure PAP refers to just this paragraph and set value */
    if (this_para() == NULL) return(1);
    pap_curr.dxaLeft = indent;
    return(0);
}

int
wri_para_indent_right(int indent)
{
    /* Check range */
    if (indent < 0 || indent > 32767) return(1);

    /* Is value already correct?  If so, do nothing */
    if (pap_curr.dxaRight == (unsigned)indent) return(0);

    /* Otherwise make sure PAP refers to just this paragraph and set value */
    if (this_para() == NULL) return(1);
    pap_curr.dxaRight = indent;
    return(0);
}

int
wri_para_indent_first(int indent)
{
    /* Is value already correct?  If so, do nothing */
    if (pap_curr.dxaLeft1 == indent) return(0);

    /* Otherwise make sure PAP refers to just this paragraph and set value */
    if (this_para() == NULL) return(1);
    pap_curr.dxaLeft1 = indent;
    return(0);
}

/*
 * Stuff to handle page headers and footers...
 *
 * A header/footer is a normal paragraph that has both
 * pap.rhcOdd and pap.rhcEven set to 1. (Write does not make a distinction
 * between odd and even pages, and these two bits always have the same value.
 * pap.rhcPage == 0: page header.  == 1: page footer.
 * Write recognises only headers and footers that appear at the beginning of
 * the document.
 * This means that their text must appear at the start of the text section,
 * although the text may be specified half way through the document.
 * Alternatively, we could force the user to specify page headers and footers
 * before any text.
 *
 * So as to simplify handling of CHPs and PAPs, we also insist that header
 * and footer definition come before everything else, enabling us to simply
 * set default CHP and PAP values instead of having to save and restore.
 */

/*
 * Start page header/footer paragraph(s).
 * Must come at the start of the document else write includes them as normal
 * text.
 */
int
wri_doc_header()
{
    return(start_rhc(0));
}

int
wri_doc_footer()
{
    return(start_rhc(1));
}

/* Start running header/footer.	 0=header, 1=footer */
static int
start_rhc(int header_footer)
{
    if (_wri_had_normal_text) {
	/* They've already output normal text into the document - so it's
	 * too late to define a header or footer.  Result: their attempted
	 * header/footer will appear in the file as normal text.
	 */
	return(1);
    }

    /* Are we already doing a header or footer? */
    if (pap_curr.rhcOdd) {
	if (pap_curr.rhcPage == (unsigned)header_footer) {
	    /* We're already in the right sort of running head code */
	    return(0);
	} else {
	    /* They've gone straight from wri_doc_header() to
	     * wri_doc_footer() (or vice versa) without having called
	     * wri_doc_return() in between.  Do the right thing for them.
	     */
	    if (wri_doc_return()) return(1);
	}
    }

    /* Start new paragraph only if there is already text in the current one.
     * (So as not to leave an empty paragraph at the start of the document)
     */
    if (lpap_curr->cpFirst != lpap_curr->cpLim) {
	if (_wri_new_paragraph()) return(1);
    }

    /* Save current text CHP and PAP */
    _wri_preserve_chp();
    _wri_preserve_pap();

    /* Set default CHP and PAP values, which are like the defaults but with
     * point size = 10 */
    if (_wri_set_default_chp()) return(1);
    wri_char_font_size(10);
    if (_wri_set_default_pap()) return(1);

    /* Indicate that it's a running header/footer paragraph */
    pap_curr.rhcOdd = pap_curr.rhcEven = 1; /* Running header/footer para */
    pap_curr.rhcPage = header_footer;	    /* of the appropriate type */

    /* Let wri_text() know that we're inside a running head code */
    _wri_in_rhc = 1;

    return(0);
}

/*
 *  Return to normal text after call to start header or footer
 */
int
wri_doc_return()
{
    if (pap_curr.rhcOdd == 0) {
	/* We're not in a running header/footer!  Do nothing. */
	return(0);
    }

    /* In the write file, the header/footer always ends with a
     * \r\n that is not printed.  This also forces a new paragraph.
     */
    wri_text("\n");

    /* Restore CHP and PAP for text */
    if (_wri_restore_chp() || _wri_restore_pap()) return(1);

    /* We're no longer inside a rhc */
    _wri_in_rhc = 0;

    return(0);
}

/*
 *  Insert page number in header/footer.
 */
int
wri_doc_insert_page_number()
{
    return(wri_text("\001"));
}

/* Print running header/footer on first page? */
int
wri_doc_pofp(int print)
{
    if (pap_curr.rhcOdd == 0) {
	/* We're not in a running header/footer */
	return(1);
    }

    /* Memorise whether to print on first page, written into all PAPs of this
     * type when we write out the final file.
     */
    pofp[pap_curr.rhcPage] = print;

    return(0);
}

/*
 *  Stuff to handle tabs...
 */

/* Array of tabstops to copy into each and every PAP */
static struct TBD tbd[itbdmax];

/* Number of elements of tbd that are used */
static int nTabs = 0;

int
wri_doc_tab_set(int position, int decimal)
{
    int i, j;

    /* Only positive values are possible */
    if (position <= 0) return(1);

    /* Valid values to store are 0 or 3.  Map all true values to WRI_DECIMAL */
    if (decimal) decimal = WRI_DECIMAL;

    /* See if already set */
    for (i=0; i<nTabs; i++) {
	if (tbd[i].dxa == (unsigned int) position) {
	    /* Redefine same tab stop */
	    tbd[i].jcTab = decimal;
	    return(0);
	}

	if (tbd[i].dxa > (unsigned int) position) {
	    /* Insert new tabstop before the one to its right */
	    break;
	}
    }

    /* We must insert the new tabstop at position i, moving i..Ntabs-1 up by
     * one.  If it is the last tabstop, i==Ntabs, so nothing is shuffled. */

    if (nTabs >= itbdmax) {
	/* We already have the maximum number of tabs defined */
	return(1);
    }

    /* Shuffle up */
    for (j=nTabs-1; j>=i; j--) {
	tbd[j+1].dxa = tbd[j].dxa;
	tbd[j+1].jcTab = tbd[j].jcTab;
    }

    /* Set new element */
    tbd[i].dxa = position;
    tbd[i].jcTab = decimal;

    nTabs++;

    return(0);
}

int
wri_doc_tab_clear(int position)
{
    int i;

    if (nTabs == 0) return(1);	/* No tabstops to clear */

    /* Only positive values are possible */
    if (position <= 0) return(1);

    /* Find tabstop */
    for (i=0; i<nTabs; i++) {
	if (tbd[i].dxa == (unsigned int) position) {
	    /* Found it. Cancel tbd[i], moving tbd[i+1]..tbd[nTabs-1] down */
	    int j;

	    for (j=i+1; j<nTabs; j++) {
		tbd[j-1].dxa = tbd[j].dxa;
		tbd[j-1].jcTab = tbd[j].jcTab;
	    }
	    nTabs--;

	    /* Clear last entry */
	    tbd[nTabs].dxa = 0;
	    tbd[nTabs].jcTab = 0;

	    return(0);
	}

	if (tbd[i].dxa > (unsigned int) position) {
	    /* Gone past it.  it doesn't exist. */
	    return(1);
	}
    }

    /* Not found. */
    return(1);
}

int
wri_doc_tab_cancel()
{
    nTabs = 0;
    memset((char *)tbd, 0, sizeof(tbd));
    return(0);
}

/* Copy tab information into a PAP.  Copy the whole block including 0s,
 * not just the tabs that are set, so that it works if they do:
 * set tabs, save, clear tabs, save.
 * N.B.	 Must not be called on an element of the lpap list, as only the
 * first few elements are stored there, and they do not include tab info.
 */
static void
copy_in_tabs(struct PAP *papp)
{
    memcpy((char *) (papp->rgtbd), (char *) tbd, sizeof(tbd));
}

/*
 *  Set all tabs according to an existing array of <itbdmax> tab settings.
 *  Used in read.c to set all tabs according to a PAP from an external document.
 */
void
_wri_set_tabs(struct TBD *rgtbd)
{
    /* Copy all tab information. */
    memcpy(tbd, rgtbd, sizeof(struct TBD) * itbdmax);
}

/*
 * Save PAPs.
 * Strategy: Loop through PAPs.	 If PAP info will fit in current page,
 * put it in.
 * Otherwise write out current PAP page and start a new page of PAPs.
 *
 * To save space in the file, we can store just one copy of identical PAP
 * descriptions and point several FODs at it.  This makes the calucation of
 * whether a PAP will fit in a page more complex:
 * - If it is a new PAP, different from the others in the page,
 *   put it in if it fits, else write the page and put FOD and PAP in a new page
 * - If it is the same as an existing PAP, we just specify a new FOD with the
 *   same bfprop as the old one.  If, however, there is not room for one FOD,
 *   start a new page and write in both FOD and PAP.
 */
int
_wri_save_pap(struct wri_header *hp, FILE *ofp)
{
    static struct FKP fkp;	/* Page under construction */
    struct lpap *lpapp; /* pointer to current lpap */
    char *start_of_props;   /* pointer to start of FPROPs in the page */
    unsigned int space_left;/* How many bytes between last FOD and first FPROP? */
    struct PAP pap;	/* Complete PAP, with tabstop information */

    /* Initialise complete PAP */
    pap = _wri_default_pap;
    /*
     * Copy in tabstop information
     */
    copy_in_tabs(&pap);

    /* Make sure values of _wri_sep.d[xy]aText reflect reality (since the user
     * specifies margins, but the SEP talks in text areas) */
    _wri_user_to_sep();

    /* pnPara is set by save_chp() */
    hp->pnFntb = hp->pnPara;	/* No paragraph info yet */

    if (_wri_seek_to_page(hp->pnPara, ofp)) return(1);

    /* There are no PAPs yet - clear structures used for merging them */
    forget_paps();

    /* Initialise page */
    fkp.fcFirst = PAGESIZE; /* First paragraph always starts at character 0 */
    fkp.cfod = 0;	    /* No FODs in this page yet */
    start_of_props = &(fkp.cfod);
    space_left = start_of_props - &(fkp.rgFPROP[0]);

    /* Treat all paragraphs... */
    for (lpapp = &lpap_first; lpapp != NULL; lpapp = lpapp->next) {
	int cch;	/* how many bytes of the PAP do we need to specify? */
	struct FOD *fodp;   /* pointer to current FOD for convenience */
	unsigned xaRight;   /* Right page margin (calculated) */
	unsigned total_size;/* Space needed to specify this PAP */
	int bfprop;	    /* bfprop for FOD */

	/* Don't bother saving PAPS that don't refer to anything */
	if (lpapp->cpFirst == lpapp->cpLim) continue;

	/* Since we store only the first significant elements of the PAP in
	 * the linked list, make a copy into a full PAP.
	 */
	memcpy(&pap, lpapp->papp, STORED_PAP_SIZE);
	/* Fix pap.res1, which we use as a reference count in the list,
	 * but should be 0 in the output. */
	pap.res1 = 0;

	/*
	 * In paragraph info for headers and footers, empirically, the indents
	 * are inclusive of the page margins.
	 * We also need to set the print-on-first-page info.
	 */
	if (pap.rhcOdd) {
	    /* Adjust margin info */
	    pap.dxaLeft += _wri_sep.xaLeft;
	    xaRight = _wri_sep.xaMac - _wri_sep.xaLeft - _wri_sep.dxaText;
	    pap.dxaRight += xaRight;

	    /* Set pofp info according to whether it's a header or a footer */
	    pap.rhcFirst = pofp[pap.rhcPage];
	}

	/* Work out how much of the PAP we must specify.
	 * It doesn't matter that we haven't set res1 back to its correct value
	 * of 0 because the minimum cch is 1 anyway and res1 is the first
	 * element of the PAP.
	 */
	cch = _wri_find_cch((char *) &pap, (char *) &_wri_default_pap, sizeof(struct PAP));

	/* If only the first byte differs, this is the default PAP */
	if (cch <= 1) {
	    /* default PAP: just the FOD */
	    total_size = sizeof(struct FOD);
	    bfprop = 0xFFFF;
	} else {
	    char *cchp;
	    /* If there is a matching PAP in the page, we will consume just
	     * one FOD.	 If there is not enough space for one FOD, we will
	     * have to begin a new page anyway, so there is no point looking
	     * for a matching PAP.  If there IS enough space for one FOD, and
	     * we find a matching PAP, we are sure not to have to start a new
	     * page, so there is no risk that we find a matching PAP and
	     * subsequently discover that it won't fit into the current page.
	     */
	    if (space_left >= sizeof(struct FOD) &&
		(cchp = recall_pap(cch, (char *) &pap)) != NULL) {
		/* Found a matching PAP. */
		total_size = sizeof(struct FOD);
		bfprop = cchp - fkp.rgFPROP;
	    } else {
		/* Either this is a new PAP, or there is not room for a FOD in
		 * the page.  In either case, we need to write a FOD and
		 * <cch> bytes of PAP prefixed by one byte for cch.
		 */
		total_size = sizeof(struct FOD) + cch + 1;

		/* 0 is an impossible value for bfprop, and signals that we
		 * must write a new PAP into the page and set bfprop
		 * accordingly.
		 */
		bfprop = 0;
	    }
	}

	/* If it doesn't fit, write out the current page and start a new one. */
	if (total_size > space_left) {
	    if (_wri_write_page((char *) &fkp, ofp)) return(1); 

	    hp->pnFntb++;   /* One more page of paragraph info */

	    /* Re-initialise page */
	    fkp.fcFirst = lpapp->cpFirst + PAGESIZE;
	    fkp.cfod = 0;
	    start_of_props = &(fkp.cfod);
	    space_left = start_of_props - &(fkp.rgFPROP[0]);

	    /* No PAPs in this page yet */
	    forget_paps();
	}

	/* write in the PAP if necessary */
	if (bfprop == 0) {
	    memcpy((start_of_props-=cch), (char *) &pap, (size_t)cch);

	    /* and prefix the properties with cch */
	    *--start_of_props = (char)cch;

	    /* Now start_of_props points to the FPROP we have just created */

	    /* Remember PAP for possible future merge */
	    memorize_pap(start_of_props);

	    /* and bfprop is the offset of FPROP from start of FOD array */

	    bfprop = start_of_props - fkp.rgFPROP;
	}

	/* Set fiddled indents back to correct values */
	if (pap.rhcOdd) {
	    pap.dxaLeft -= _wri_sep.xaLeft;
	    pap.dxaRight -= xaRight;
	}

	/* Get convenient pointer to FOD */
	fodp = &(fkp.rgFOD[fkp.cfod]);

	fodp->bfprop = bfprop;

	/* set fcLim, converting from index-into-text to index-into-file */
	fodp->fcLim = lpapp->cpLim + PAGESIZE;

	/* One more FOD in the page... */
	fkp.cfod++;

	/* ...and less space */
	space_left -= total_size;
    }

    /* Write final page, if it contains anything */
    if (fkp.cfod != 0) {
	_wri_write_page((char *) &fkp, ofp);
	hp->pnFntb++;	/* One more page of paragraph info */
    }

    return(0);
}

/* Private functions used by wri_save_paps() to remember what PAPs are already
 * specified in this page, so as to be able to merge new PAPs with identical
 * old ones.
 */

/* How many PAPs can there be in a page?
 * The page has FC fcFirst at the start, and char cch at the end
 * Each PAP is always at least 3 bytes (cch + ingored byte + 1st significant
 * byte), and for each PAP there is always at least one FOD
 */
#define MAX_PAPS_PER_PAGE ((PAGESIZE - sizeof(FC) - 1) / (3 + sizeof(struct FOD)))

static int n_old_paps = 0;  /* Number of different PAPs in current page */
static char *old_pap[MAX_PAPS_PER_PAGE];/* pointers to cch byte of the various PAPs. */

static void
forget_paps()
{
    n_old_paps = 0;
}

/* Memorize address of cch byte for an existing PAP in the page */
static void
memorize_pap(char *cchp)
{
    /* The following condition should always be true, but... */
    if (n_old_paps < MAX_PAPS_PER_PAGE) old_pap[n_old_paps++] = cchp;
}

/* If there is an existing PAP specifier with the same cch byte and contents,
 * return its address.	Otherwise return NULL.
 */
static char *
recall_pap(int cch, char *papp)
{
    int i;

    for (i=0; i < n_old_paps; i++) {
	char *cchp = old_pap[i];

	/* Check first cch, then contents of PAP. */
	if ((char)cch == *cchp && memcmp(papp, cchp+1, cch) == 0) {
	    /* Found a match! */
	    return(cchp);
	}
    }
    return(NULL);
}

/* Internal interface to this module, called from the rest of this library */

/*
 * Set the extent of the current PAP, called when user adds text.
 */
void
_wri_extend_pap(CP cpLim)
{
    lpap_curr->cpLim = cpLim;
}

/*
 * Start of new paragraph.
 * Add an element to the list of paragraph extents, referring to the same PAP.
 */
int
_wri_new_paragraph()
{
    if (_wri_new_lprop((struct lprop **) &lpap_curr, sizeof(struct lpap)) == NULL) {
	return(1);  /* Fail */
    }

    /* Increment reference count (8 bits) */
    if (lpap_curr->papp->res1++ == 0) {
	/* If it overflows, create a new pap.  clone_pap() decrements the
	 * reference count in the old element, so it goes back to 255. */
	if (clone_pap() == NULL) return(1);
    }

    return(0);	/* success */
}

/*
 * Return a pointer to the PAP for the current paragraph such that it can be
 * modified.  It is here that the laziness about creating new PAP structures
 * gets resolved; if the PAP info for the current paragraph is referred to by
 * more than one paragraph extent, create a new PAP and copy the contents.
 */
static struct PAP *
this_para()
{
    /* If there is only one reference to the PAP,
     * there's no need to create a new one.
     */
    if (pap_curr.res1 == 1) {
	return(&pap_curr);
    }

    /* Otherwise create a new paragraph property block that can be modified */
    return(clone_pap());
}

/*
 *  Clone the last PAP in the list, returning a pointer to the new pap.
 *  Adjust list pointers and reference counts appropriately
 */
static struct PAP *
clone_pap()
{
    struct PAP *pap_old;    /* pointer to new PAP */
    struct PAP *pap_new;    /* pointer to new PAP */

    pap_old = &pap_curr;
    pap_new = (struct PAP *) malloc(STORED_PAP_SIZE);
    if (pap_new == NULL) {
	_wri_error = 1;
	return(NULL);	/* fail */
    }

    /* Copy info from old into new element */
    memcpy(pap_new, pap_old, STORED_PAP_SIZE);

    /* Point lpap_curr at the new, modifyable PAP */
    lpap_curr->papp = pap_new;

    /* Adjust reference counts */
    pap_old->res1--;
    pap_new->res1 = 1;

    return(pap_new);
}

/*
 *  Free all memory allocated to PAPs and leave pointers as they were at the
 *  start of time, ready to create a new Write document.
 */
int
_wri_reinit_pap()
{
    /* First element is static, not mallocked, so start from second */
    free_lpaps(lpap_first.next);

    /* reset initial PAP */
    memcpy((char *) &(first_pap), (char *) &_wri_default_pap, sizeof(struct PAP));
    first_pap.res1 = 1; /* Set reference count */

    /* and initial lpap */
    lpap_first.next = NULL;
    lpap_first.cpFirst = lpap_first.cpLim = (CP) 0;
    /* lpap_first.papp never moves from &first_pap */

    /* re-initialise list pointers */
    lpap_curr = &lpap_first;

    /* Reset pofp default values */
    pofp[0] = pofp[1] = 0;

    /* Clear all tabs */
    wri_doc_tab_cancel();

    return(0);
}

static void
free_lpaps(struct lpap *from)
{
    struct lpap *lpapp;

    for (lpapp = from; lpapp != NULL; /* re-init in loop body */) {
	struct lpap *next;

	/* Decrement reference count to paragraph info, and if no one points
	 * to it any longer, free that too.  First structure is static, so be
	 * careful not to free that.
	 */
	if (--(lpapp->papp->res1) == 0 && lpapp->papp != &first_pap) {
	    free((char *) lpapp->papp);
	}

	/* We need to keep hold of next pointer because free() may corrupt it */
	next = lpapp->next;
	free((char *) lpapp);
	lpapp = next;
    }
}

/*
 *  Set all paragraph properties to default values
 */
static int
_wri_set_default_pap()
{
    /* Make sure the PAP structure is not referred to by other lpaps. */
    if (this_para() == NULL) return(1);

    /* Set default PAP values, leave reference count */
    memcpy(((char *) &pap_curr) + 1 , ((char *) &_wri_default_pap) + 1, STORED_PAP_SIZE - 1);

    return(0);
}

/*
 *  Save a copy current paragraph properties
 */
static char saved_pap[STORED_PAP_SIZE];

static void
_wri_preserve_pap()
{
    memcpy(saved_pap, (char *) &pap_curr, STORED_PAP_SIZE);
}

static int
_wri_restore_pap()
{
    /* Make sure the PAP structure is not referred to by other lpaps. */
    if (this_para() == NULL) return(1);

    /* Set saved PAP values, leave reference count */
    memcpy(((char *) &pap_curr) + 1, saved_pap + 1, STORED_PAP_SIZE - 1);

    return(0);
}

/*
 * Raw interface used when reading PAPs from a write file.  Add the PAP
 * and set its extent... the text will be read in appropriately in due course.
 * is_first_para tells us if this is the first paragraph from the file.
 *
 * If the previous text did not end with a paragraph break, the first paragraph
 * of the file is appended to the end of that paragraph, and we take the
 * paragraph properties from the file in preference to those already in force.
 * If the previous text *did* end with a paragraph break, the last lpap in the
 * chain will have an extent of 0.
 * In either case, the text in the first paragraph gets appended to the end of
 * the last existing lpap, and the paragraph properties for that paragraph are
 * those from the file.
 */
int
_wri_append_pap(struct PAP *papp, CP cpLim, int is_first_para)
{
    /* Create a new entry in the lpap list unless this is the first paragraph */
    if (!is_first_para && _wri_new_paragraph()) return(1);

    /* If the new pap differs from the old, allocate a new PAP structure.
     * The reference count does not enter into the comparison.
     */
    if (memcmp(((char *)papp) + 1, ((char *)&pap_curr) + 1, STORED_PAP_SIZE - 1) != 0)	{
	/* They differ.	 Make sure we have a copy of the PAP with just one
	 * reference, and set the new properties (but leave the ref count).
	 */
	if (this_para() == NULL) return(1);
	memcpy(((char *)&pap_curr) + 1, ((char *)papp) + 1, STORED_PAP_SIZE - 1);
    }
    /* This PAP covers the new characters. */
    _wri_extend_pap(cpLim);

    return(0);
}

/*
 * Memorise the current situation to be able to recover it if reading of
 * a write file fails subsequently.  This means recovering both the list of
 * lpaps, the PAPs that they refer to, and the reference count in the last
 * PAP.	 We use the generic free_lpaps which takes care of decrementing the
 * reference count in each PAP when the lpap that refers to it is freed, and
 * frees the PAP when it is no longer referenced.
 */

/* lpap_break points to the NULL in the final lpap. */
static struct lpap **lpap_break;

void
_wri_breakpoint_pap()
{
    lpap_break = &(lpap_curr->next);
    if (*lpap_break != NULL) {
#ifdef _WINDOWS
	MessageBox( 0, "_wri_breakpoint_pap: Internal error: *lpap_break != NULL.", "WriteKit in error", MB_OK|MB_ICONSTOP );
#else
	fputs("_wri_breakpoint_pap: Internal error: *lpap_break != NULL.\n", stderr);	       
#endif	    
    }
}

void
_wri_rollback_pap()
{
    free_lpaps(*lpap_break);
    *lpap_break = NULL;
}

#if 0
/*
 * Debugging function: check that all reference counts are correct by
 * running through the list and recalculating the reference counts into
 * res3, another free byte in struct PAP.
 */
void
_wri_verify_ref_counts()
{
    int failed = 0;
    struct lpap *lpapp;

    /* Set all res3s to 0 (should be anyway, but... */
    for (lpapp = &lpap_first; lpapp != NULL; lpapp = lpapp->next) {
	lpapp->papp->res3 = 0;
    }

    /* calculate reference counts */
    for (lpapp = &lpap_first; lpapp != NULL; lpapp = lpapp->next) {
	if (++lpapp->papp->res3 == 0) {
	    fputs("Reference count overflow\n", stderr);
	    failed = 1;
	}
    }

    /* check reference counts */
    for (lpapp = &lpap_first; lpapp != NULL; lpapp = lpapp->next) {
	if (lpapp->papp->res3 != lpapp->papp->res1) {
	    #ifdef _WINDOWS
	    char szmess[80];									
	    wsprintf(stderr, "Reference count mismatch: res1=%d, should be %d\n", lpapp->papp->res1, lpapp->papp->res3);
	    MessageBox( 0, szmess, "WriteKit in error", MB_OK|MB_ICONSTOP );
	    #elif
	    fprintf(stderr, "Reference count mismatch: res1=%d, should be %d\n", lpapp->papp->res1, lpapp->papp->res3);
	    #endif	

	    failed = 1;
	}
    }

    /* Set all res3s back to 0 */
    for (lpapp = &lpap_first; lpapp != NULL; lpapp = lpapp->next) {
	lpapp->papp->res3 = 0;
    }

    if (!failed) fputs("Reference counts ok.\n", stderr);
}
#endif
