/*
 *  Library to generate write files.
 *
 *  Code for treatment of character properties
 *
 *  Includes:
 *	Structure definitions to memorize the PAPs used in the document.
 *	Code to save CHP information in the final Write file.
 *	Internal functions for manipulation of list of CHPs.
 *
 *  Private data:
 *	list of CHPs defined for the text so far
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */
#include <stdio.h>  /* for NULL */
#include <string.h> /* for memcpy() */
#include <memory.h> /* for memcpy() */
#include <malloc.h> /* for free() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/* Structure definition for linked list of chps. */
/* NB: For every pair of consecutive elements, cpLim of the first is equal
 * to cpFirst of the second.  This is redundant, yes, but avoids having to
 * track back up the list to know whether the current CHP refers to any
 * characters or not.
 * All elements of the list are mallocked, except the first element, which is
 * static.
 */
struct lchp {
    struct lchp *next;	/* Pointer to next element in list */
    CP cpFirst;		/* Index of first character this CHP refers to */
    CP cpLim;		/* Index into text, one past the last character that
			 * this CHP refers to */
    struct CHP chp;	/* Character properties in full */
};

/*
 *  Function prototypes
 */
static struct CHP *new_chp(void);
static void free_lchps(struct lchp *from);

/*
 *  Private data
 */

/* The default CHP, with values that are assumed for fields that are not
 * specified in the Write file.	 Made public for read.c.
 */
const struct CHP _wri_default_chp = {
    0,	/* res1 */

    0,	/* fBold */
    0,	/* fItalic */
    0,	/* ftc: First font, always Arial. */

    24, /* hps: 12 points */

    /* the rest is all 0 */
};

/* First element of list, with the default CHP values assumed by Write on
 * startup.
 */
static struct lchp lchp_first = {
    (struct lchp *) NULL,   /* next */
    (CP) 0,		    /* cpFirst */
    (CP) 0,		    /* cpLim */
    {			    /* chp */
	0,  /* res1 */

	0,  /* fBold */
	0,  /* fItalic */
	0,  /* ftc: First font, always Arial. */

	20, /* hps: 10 points, different from the supposed default. */

	/* the rest is all 0 */
    }
};

static struct lchp *lchp_curr = &lchp_first;	/* Current CHP (the last in the list) */
/* Easy way to reference the current CHP */
#define chp_curr (lchp_curr->chp)

/*
 *  Public functions
 */

/* Reset to default values (just like the menu option). */
int
wri_char_normal()
{
    /* If any of them fail, indicate failure */
    return (
	wri_char_bold(0) ||
	wri_char_italic(0) ||
	wri_char_underline(0) ||
	wri_char_script(WRI_NORMAL)
    );
}

/* Set value of boldness.  Parameter is 0 or 1 */
int
wri_char_bold(int value)
{
    /* Check range */
    if (value != 0 && value != 1) return(1);

    /* Are we already in bold?	If so, do nothing */
    if (chp_curr.fBold == (unsigned)value) return(0);

    /* Otherwise create new CHP and set property */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.fBold = (unsigned)value;
    return(0);
}

/* Set value of italicness.  Parameter is 0 or 1 */
int
wri_char_italic(int value)
{
    /* Check range */
    if (value != 0 && value != 1) return(1);

    /* Are we already in italic?  If so, do nothing */
    if (chp_curr.fItalic == (unsigned)value) return(0);

    /* Otherwise create new CHP and set property */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.fItalic = (unsigned)value;
    return(0);
}

/* Set underlining.  Parameter is 0 or 1 */
int
wri_char_underline(int value)
{
    /* Check range */
    if (value != 0 && value != 1) return(1);

    /* Are we already in underline?  If so, do nothing */
    if (chp_curr.fUline == (unsigned)value) return(0);

    /* Otherwise create new CHP and set property */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.fUline = (unsigned)value;
    return(0);
}

/*
 * Set superscript/subscript
 */
int
wri_char_script(int value)
{
    /* Check validity of parameter */
    switch (value) {
    case WRI_NORMAL:
    case WRI_SUBSCRIPT:
    case WRI_SUPERSCRIPT:
	break;
    default:
	return(1);  /* Fail */
    }

    /* Is the value already right? */
    if (chp_curr.hpsPos == (unsigned)value) return(0);

    /* When we go from Normal to Scripted mode, Write does a reduce() of
     * the point size, which is restored when you exit that mode.
     * If you are already at 6pt and do subscript - normal, the reduce()
     * goes no smaller, and the subsequent enlarge() leaves you in 8pt.
     * This is what Write does.
     */
    if (chp_curr.hpsPos == WRI_NORMAL && value != WRI_NORMAL) {
	/* Going into scripted mode */
	if (wri_char_reduce()) return(1);
    } else if (chp_curr.hpsPos != WRI_NORMAL && value == WRI_NORMAL) {
	/* Going out of scripted mode */
	if (wri_char_enlarge()) return(1);
    }

    /* Otherwise create new CHP and set size */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.hpsPos = (unsigned char)value;

    return(0);
}

/* Select font by name */
int
wri_char_font_name(char *font_name)
{
    int ftc = _wri_cvt_font_name_to_code(font_name, 0);

    if (ftc == -1) return(1);	/* Fail */

    /* Is the font code already right?
    if (chp_curr.ftc == (FTC)ftc) return(0);

    /* Otherwise create new CHP and set size */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.ftc = (FTC)ftc;

    return(0);
}

/* Set character size in points.
 * NB. Value stored in structure is in half-points
 */
int
wri_char_font_size(int value)
{
    unsigned char hps;

    /* Check range */
    if (value < 4 || value > 127) return(1);

    /* Convert to half-points */
    hps = (unsigned char)(value * 2);

    /* Is the size already right?
    if (chp_curr.hps == hps) return(0);

    /* Otherwise create new CHP and set size */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.hps = hps;

    return(0);
}

/*
 * Reduce/enlarge point size.  We do what Write does (empirically):
 */
/* Table of point sizes to which Write will enlarge or reduce */
static unsigned hps_values[] = {
    /* 6,  8, 10, 12, 14, 18, 24, 30, 36, 48 points */
      12, 16, 20, 24, 28, 36, 48, 60, 72, 96 /* half-points */
};

/* Number of entries in hps_values[] */
#define N_HPS_VALUES (sizeof(hps_values)/sizeof(hps_values[0]))

/*
 * Reduce point size
 */
int
wri_char_reduce()
{
    int i;

    /* Write's algorithm for selecting point size is as follows: */

    /* If we are already at or beyond the minimum, there is no change */
    if (chp_curr.hps <= hps_values[0]) return(0);

    /* Otherwise change to the next smallest size in the table.
     * More precisely, to the largest value in the table that
     * is less than the current size */
    for (i=N_HPS_VALUES-1; i>=0; i--) {
	if (hps_values[i] < chp_curr.hps) break;
    }

    /* Create new CHP and set size */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.hps = hps_values[i];

    return(0);
}

/*
 * Enlarge point size
 */
int
wri_char_enlarge()
{
    int i;

    /* Write's algorithm for selecting point size is as follows: */

    /* If we are already at or beyond the maximum, there is no change */
    if (chp_curr.hps >= hps_values[N_HPS_VALUES-1]) return(0);

    /* Otherwise change to the next largest size in the table.
     * More precisely, to the smallest value in the table that
     * is larger than the current size */
    for (i=0; i < N_HPS_VALUES-1; i++) {
	if (hps_values[i] > chp_curr.hps) break;
    }

    /* Create new CHP and set size */
    if (new_chp() == NULL) return(1); /* Fail */
    chp_curr.hps = hps_values[i];

    return(0);
}

/*
 * Set the "fSpecial" field of the CHP, used exclusively for the page number.
 * There's no point in checking whether it already has the right value 'cos it
 * never will.
 */
int
_wri_chp_special(int x)
{
    if (new_chp() == NULL) return(1);

    chp_curr.fSpecial = x;
    return(0);
}

/*
 *  Write out character info.  Sets hp->pnPara so that save_pap() knows
 *  how much CHP info there was.
 */
int
_wri_save_chp(struct wri_header *hp, FILE *ofp)
{
    static struct FKP fkp;	/* Page under construction */
    struct lchp *lchpp; /* pointer to current lchp */
    char *start_of_props;   /* pointer to start of FPROPs in the page */
    unsigned int space_left;/* How many bytes between last FOD and first FPROP? */

    if (_wri_seek_to_page(pnChar(*hp), ofp)) return(1);	 /* Fail */

    hp->pnPara = pnChar(*hp);	/* No paragraph info yet */

    /* Initialise page */
    fkp.fcFirst = PAGESIZE; /* First CHP always starts at character 0 */
    fkp.cfod = 0;	    /* No FODs in this page yet */
    start_of_props = &(fkp.cfod);
    space_left = start_of_props - &(fkp.rgFPROP[0]);

    /* Treat all CHPs... */
    for (lchpp = &lchp_first; lchpp != NULL; lchpp = lchpp->next) {
	int cch;	/* how many bytes of the PAP do we need to specify? */
	struct FOD *fodp;   /* pointer to current FOD for convenience */
	unsigned total_size;	    /* Space needed to specify this PAP */

	/* Don't bother saving CHPS that don't refer to anything */
	if (lchpp->cpFirst == lchpp->cpLim) continue;

	/* Work out how much of the CHP we must specify. */
	cch = _wri_find_cch((char *)&(lchpp->chp), (char *)&_wri_default_chp, sizeof(struct CHP));

	/* If only the first byte differs, this is the default CHP */
	if (cch <= 1) {
	    /* default PAP: just the FOD */
	    total_size = sizeof(struct FOD);
	} else {
	    /* FOD with cch bytes and the cch byte prefixed */
	    total_size = sizeof(struct FOD) + cch + 1;
	}

	/* If it doesn't fit, write out the current page and start a new one. */
	if (total_size > space_left) {
	    if (_wri_write_page((char *)&fkp, ofp)) return(1);

	    hp->pnPara++;   /* One more page of CHP info */

	    /* Re-initialise page */
	    fkp.fcFirst = lchpp->cpFirst + PAGESIZE;
	    fkp.cfod = 0;
	    start_of_props = &(fkp.cfod);
	    space_left = start_of_props - &(fkp.rgFPROP[0]);
	}

	/* write in the CHP if not the default CHP */
	if (cch > 1) {
	    memcpy((start_of_props-=cch), (char *)&(lchpp->chp), (size_t)cch);

	    /* prefix the properties with cch */
	    *--start_of_props = (char)cch;
	}

	/* Now start_of_props points to the FPROP we have just created */

	/* Get convenient pointer to FOD */
	fodp = &(fkp.rgFOD[fkp.cfod]);

	if (cch > 1) {
	    /* bfprop is the offset of FPROP from start of FOD array */
	    fodp->bfprop = start_of_props - fkp.rgFPROP;
	} else {
	    /* Default CHP */
	    fodp->bfprop = 0xFFFF;
	}

	/* set fcLim, converting from index-into-text to index-into-file */
	fodp->fcLim = lchpp->cpLim + PAGESIZE;

	/* One more FOD in the page... */
	fkp.cfod++;

	/* ...and less space */
	space_left -= total_size;
    }

    /* Write final page, if it contains anything */
    if (fkp.cfod != 0) {
	_wri_write_page((char *)&fkp, ofp);
	hp->pnPara++;	/* One more page of CHP info */
    }

    return(0);
}

/* Internal interface to this module, called from the rest of this library */

/*
 * Set the extent of the current CHP, called when user adds text.
 */
void
_wri_extend_chp(CP cpLim)
{
    lchp_curr->cpLim = cpLim;
}

/*
 * Create a new CHP, tagged on to the end of the list, with contents copied
 * from the current settings.
 *
 * If the current CHP doesn't refer to any characters, there is no need to
 * create a new one (otherwise two consecutive changes to the character
 * info would create an unused CHP in the list).
 *
 * Returns the address of the CHP within the new list element.
 */
static struct CHP *
new_chp()
{
    /* If CHP refers to no chars, no need to create a new one */
    if (lchp_curr->cpLim == lchp_curr->cpFirst) return(&(lchp_curr->chp));

    if (_wri_new_lprop((struct lprop **)&lchp_curr, sizeof(struct lchp)) == NULL) {
	return(NULL);
    }

    return(&(lchp_curr->chp));
}

/*
 *  Free all memory allocated to CHPs and leave pointers as they were at the
 *  start of time, ready to create a new Write document.
 */
int
_wri_reinit_chp()
{
    /* First element is static, not mallocked, so start from second */
    free_lchps(lchp_first.next);

    /* reset initial CHP */
    lchp_first.next = NULL;
    lchp_first.cpFirst = lchp_first.cpLim = (CP) 0;
    memcpy((char *)&(lchp_first.chp), (char *) &_wri_default_chp, sizeof(struct CHP));
    /* re-initialise list pointers */
    lchp_curr = &lchp_first;

    return(0);
}

static void
free_lchps(struct lchp *from)
{
    struct lchp *lchpp;

    for (lchpp = from; lchpp != NULL; /* re-init in loop body */) {
	struct lchp *next;

	/* Need to keep hold of next pointer because free() may corrupt it */
	next = lchpp->next;
	free((char *) lchpp);
	lchpp = next;
    }
}

/*
 *  Set all character properties to default values
 */
int
_wri_set_default_chp()
{
    /* If already the default, don't bother. */
    if (memcmp(&chp_curr, &_wri_default_chp, sizeof(struct CHP)) == 0) return(0);

    /* Otherwise create new CHP and copy in default values */
    if (new_chp() == NULL) return(1);
    memcpy((char *)&chp_curr, (char *)&_wri_default_chp, sizeof(struct CHP));

    return(0);
}

/*
 *  Save a copy current paragraph properties (used while defining running heads)
 */
static struct CHP saved_chp;

void
_wri_preserve_chp()
{
    memcpy((char *)&saved_chp, (char *)&chp_curr, sizeof(struct CHP));
}

int
_wri_restore_chp()
{
    /* If nothing has changed, don't bother. */
    if (memcmp(&chp_curr, &saved_chp, sizeof(struct CHP)) == 0) return(0);

    /* Otherwise create new CHP and copy in saved values */
    if (new_chp() == NULL) return(1);
    memcpy((char *)&chp_curr, (char *)&saved_chp, sizeof(struct CHP));

    return(0);
}

/*
 * Raw interface used when reading in existing write files.  Add the CHP to the
 * end of the list.  The text will follow.
 */
int
_wri_append_chp(struct CHP *chpp, CP cpLim)
{
    if (memcmp(chpp, &chp_curr, sizeof(struct CHP)) == 0) {
	/* It's the same as the current CHP - extend current */
    } else {
	if (new_chp() == NULL) return(1);
	memcpy(&chp_curr, chpp, sizeof(chp_curr));
    }
    _wri_extend_chp(cpLim);
    return(0);
}

/*
 * Remember current end of CHP chain to restore to this point if the reading
 * of a write document fails.
 * lchp_break points to the NULL pointer at the end of the list.
 */
static struct lchp **lchp_break;

/*
 * If the first CHP of the file was the same as the existing CHP,
 * the breakpoint CHP will have had its extent increased.
 * cpLim_break remembers the original extent to be able to restore it.
 */
static CP cpLim_break;

void
_wri_breakpoint_chp()
{
    lchp_break = &(lchp_curr->next);
    if (*lchp_break != NULL) {
#ifdef _WINDOWS
	MessageBox( 0, "_wri_breakpoint_chp: Internal error: end pointer not NULL", "WriteKit in error", MB_OK|MB_ICONSTOP );
#else
	fputs("_wri_breakpoint_chp: Internal error: end pointer not NULL\n", stderr);	       
#endif	    
    }

    cpLim_break = lchp_curr->cpLim;
}

/*
 * Restore list of CHPs to the point where we set the breakpoint,
 * by freeing the CHPs that we successively allocated, and replacing the
 * NULL pointer.
 */
void
_wri_rollback_chp()
{
    /* Free from the lchp after the breakpoint */
    free_lchps(*lchp_break);

    *lchp_break = NULL;

    /* Restore (maybe) incremented extent */
    lchp_curr->cpLim = cpLim_break;
}
