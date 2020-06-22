/*
 *  Library to generate write files.
 *
 *  Code for treatment of section info
 *
 *  Public functions:
 *	Functions to change section info
 *  Private data:
 *	structure of the section info.
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */
#include <stdio.h>  /* for NULL */
#include <string.h> /* for memcmp() */
#include <memory.h> /* for memcmp() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/*
 *  Default section info.
 */
static const struct SEP default_sep = {
    sizeof(struct SEP)-1,   /* cch */
    0,	    /* res1 */
    15840,  /* yaMac */
    12240,  /* xaMac */
    0xFFFF, /* pgnFirst */
    1440,   /* yaTop */
    12960,  /* dyaText */
    1800,   /* xaLeft */
    8640,   /* dxaText */
    256,    /* res2 (empirically) */
    1080,   /* yaHeader */
    14760,  /* yaFooter */
};

/*
 *  Current section info, modifiable by the user
 */
struct SEP _wri_sep = {
    sizeof(struct SEP)-1,   /* cch */
    0,	    /* res1 */
    15840,  /* yaMac */
    12240,  /* xaMac */
    0xFFFF, /* pgnFirst */
    1440,   /* yaTop */
    12960,  /* dyaText */	/* Calculated from dyaBottom when we save */
    1800,   /* xaLeft */
    8640,   /* dxaText */	/* Calculated from dxaRight when we save */
    256,    /* res2 (empirically) */
    1080,   /* yaHeader */
    14760,  /* yaFooter */	/* Calculated from dyaFooter when we save */
};

/*
 * The user defines top, left, right and bottom margins, and the page size.
 * We must memorise top and left margins (no problem) and text width and height.
 * To do this, and also let the user change the physical page size, we save
 * their settings for right and bottom margins, and calculate d[xy]aText
 * just before saving. The same goes for the distance of the footer from the
 * bottom of the page. See _wri_sep_to_user() and _wri_user_to_sep().
 *   ___________________
 *  |			|\	\
 *  |			| >yaTop |
 *  |			|/	 |
 *  |	Go placidly	|\	 |
 *  |	amid the noi-	| .	 |
 *  |	se & haste &	| |	 |
 *  |	remember what	| |	 |
 *  |	peace there	| |	 |
 *  |	may be in si-	| |	 |
 *  |	lence.		|  >dyaText
 *  |	It is still a	| |	  > yaMac
 *  |	beautiful	| '	 |
 *  |	world.		|/	 |
 *  |			|\	 |
 *  |			| >dxaRight
 *  |___________________|/	/
 *  \__/	     \__/
 * xaLeft	     dxaRight
 *	\___________/
 *	   dxaText
 *  \___________________/
 *	    xaMac
 */

static int dxaRight = 12240 - 1800 - 8640;  /* xaMac - xaLeft - dxaText */
static int dyaBottom = 15840 - 1440 - 12960;/* yaMac - yaTop - dyaText */

/* Footer is specified as distance from bottom, stored as distance from top */
static int dyaFooter = 1080;	/* 0.75in */

int
wri_doc_number_from(int pgnFirst)
{
    if (pgnFirst >= 1 && pgnFirst <= 127) {
	_wri_sep.pgnFirst = pgnFirst;
	return(0);
    }
    return(1);
}

/*
 * In Write, the user specifies the four page margins, and fishes the page
 * size out of a printer configuration file.
 * In a Write file, instead, they are memorised as top and left margins,
 * and width and height of printed area.  We provide functions that work
 * with the latter.
 */

int
wri_doc_margin_left(int margin)
{
    if (margin >= 0 && margin <= 32767) {
	_wri_sep.xaLeft = margin;
	return(0);
    }
    return(1);
}

int
wri_doc_margin_top(int margin)
{
    if (margin >= 0 && margin <= 32767) {
	_wri_sep.yaTop = margin;
	return(0);
    }
    return(1);
}

int
wri_doc_margin_right(int margin)
{
    dxaRight = margin;
    return(1);
}

int
wri_doc_margin_bottom(int margin)
{
    dyaBottom = margin;
    return(1);
}

int
wri_doc_page_width(int width)
{
    if (width > 0 && width <= 32767) {
	_wri_sep.xaMac = width;
	return(0);
    }
    return(1);
}

int
wri_doc_page_height(int height)
{
    if (height > 0 && height <= 32767) {
	_wri_sep.yaMac = height;
	return(0);
    }
    return(1);
}

/* Set distance of running header from top of page.
 * Maximum value permitted by Write is 22 inches.
 */

int
wri_doc_distance_from_top(int distance)
{
    if (distance >= 0 && distance <= 31680) {
	_wri_sep.yaHeader = distance;
	return(0);
    }
    return(1);
}

/* Set distance of running footer from bottom of page.
 * Specified as distance from bottom of page, stored as distance from top.
 */
int
wri_doc_distance_from_bottom(int distance)
{
    if (distance >= 0 && distance <= 31680) {
	dyaFooter = distance;
	return(0);
    }
    return(1);
}

/* Functions to convert from the variables the user deals with to those
 * stored in the SEP, and vice-versa
 */
void
_wri_user_to_sep()
{
    _wri_sep.dxaText = _wri_sep.xaMac - _wri_sep.xaLeft - dxaRight;
    _wri_sep.dyaText = _wri_sep.yaMac - _wri_sep.yaTop - dyaBottom;

    /* Footer is stored as distance from top, not distance from bottom */
    _wri_sep.yaFooter = _wri_sep.yaMac - dyaFooter;
}

/* Called from read.c */
void
_wri_sep_to_user()
{
    dxaRight = _wri_sep.xaMac - _wri_sep.xaLeft - _wri_sep.dxaText;
    dyaBottom = _wri_sep.yaMac - _wri_sep.yaTop - _wri_sep.dyaText;
    dyaFooter = _wri_sep.yaMac - _wri_sep.yaFooter;
}

int
_wri_save_section(struct wri_header *hp, FILE *ofp)
{
    char page[PAGESIZE];
    struct SETB setb;

    /*
     * Fix calculation of distances according to final page size
     */
    _wri_user_to_sep();

    if (memcmp(&_wri_sep, &default_sep, (size_t) sizeof(_wri_sep)) == 0) {
	/* Default SEP needs not be specified */
	hp->pnPgtb = hp->pnSetb = hp->pnSep;
	return(0);
    }

    /* Memorise whole SEP.  No point in messing about with size reduction */
    hp->pnSetb = hp->pnSep + 1;
    hp->pnPgtb = hp->pnSetb + 1;

    /*
     *	Write SEP
     */

    /* Clear unused stuff to 0 */
    (void*) memset(page, 0, PAGESIZE);

    /* Copy in SEP */
    (void*) memcpy(page, &_wri_sep, sizeof(_wri_sep));
    /* And write to file */
    if (_wri_seek_to_page(hp->pnSep, ofp) || _wri_write_page(page, ofp)) {
	/* Failed - so specify no section info */
	hp->pnPgtb = hp->pnSetb = hp->pnSep;
	return(1);
     }

    /*
     *	Write SETB
     */

    /* Clear unused stuff to 0 */
    (void*) memset(page, 0, sizeof(_wri_sep));

    /* Create SETB */
    setb.csed = 2;
    setb.rgSED[0].cp = hp->fcMac - PAGESIZE;
    setb.rgSED[0].fcSep = (FC)hp->pnSep * PAGESIZE;
    setb.rgSED[1].fcSep = (unsigned long)-1;

    /* Copy in SETB */
    (void*) memcpy(page, &setb, sizeof(setb));
    if (_wri_seek_to_page(hp->pnSetb, ofp) || _wri_write_page(page, ofp)) {
	/* Failed - so specify no section info */
	hp->pnPgtb = hp->pnSetb = hp->pnSep;
	return(1);
    }

    return(0);
}

/*
 * Set back to default values for a new document
 */
int
_wri_reinit_section()
{
    _wri_set_default_sep();
    return(0);
}

void
_wri_set_default_sep()
{
    memcpy(&_wri_sep, &default_sep, sizeof(struct SEP));
    _wri_sep_to_user();
}
