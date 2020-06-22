/*
 *  Library to generate write files.
 *
 *  Code to read existing Write files.
 *
 *  Externals that are shared between ths module and others are located in
 *  the others so that a program that does not call wri_read() does not get
 *  this code (about 3.5k) in their executable.
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */
#include <stdio.h>  /* for NULL */
#include <string.h> /* for strlen() */
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/*
 *  Function prototypes
 */
static int read_page(PN n, int extent, char *buf, FILE *ifp);
static int read_header(FILE *ifp, struct wri_header *hp);
static int read_text(FILE *ifp, struct wri_header *hp);
static int read_chps(FILE *ifp, struct wri_header *hp);
static int read_paps(FILE *ifp, struct wri_header *hp,
    int want_paps, int want_tabs);
static int read_section(FILE *ifp, struct wri_header *hp);
static int read_fonts(FILE *ifp, struct wri_header *hp);

/* Private data */
static FC fcStart;  /* Index into file of first character to copy */
static FC fcEnd;    /* One past the last character */
static FC initial_text; /* Amount of text there was already in the temp file */

/* Table to convert font codes from file into our font codes */
static int font_map[MAX_FONTS];

/*
 *  User functions
 */
int
wri_open(char *filename)
{
    return(wri_new() || wri_read(filename, WRI_ALL));
}

/*
 *  Read existing Write file onto the end of the document, optionally including
 *  the following parts of the document, as specified by <what>,
 *  the logical "or" of:
 *  WRI_TEXT	The text and paragraph layout
 *  WRI_CHAR_INFO Character info: font, size, boldness ecc.
 *		if you specify WRI_CHAR_INFO without WRI_TEXT, this has the
 *		effect of reading the font table, which provides you with the
 *		correct values for font family codes for fonts other than those
 *		already known to libwrite.
 *  WRI_DOCUMENT The section info: page layout of the document (margins etc)
 *  WRI_TABS	The tab settings used in the document
 *
 *  or WRI_ALL, the logical OR of all the above, to get the document inserted
 *  exactly as it was in the other file. If, in future, we sub-divide the
 *  things you can import (text without character attributes ecc), WRI_ALL
 *  will still mean everything.
 *
 *  After calling wri_read(), the paragraph layout is left set at that of the
 *  last paragraph in the document, and the character info as for the last
 *  character in the document.
 */
int
wri_read(char *filename, int what)
{
    FILE *ifp;	/* file pointer open to Write file */
    struct wri_header header;	/* header from Write file */

    ifp = fopen(filename, "rb");
    if (ifp == NULL) {
	return(1);
    }

    if (read_header(ifp, &header)) return(1);

    /* Check obligatory values. We don't cope with files containg OLE objects */
    if (header.wIdent != WRIH_WIDENT ||
	header.wTool != WRIH_WTOOL) {
	    /* Not a Write file */
	    goto fail;
    }

    if (header.pnMac == 0) {
	/* It's a Word file, not a Write file */
	goto fail;
    }

    /* Remember start and one-past-the-end of the text to copy (fcStart will
     * be incremented if there are initial header/footer paragraphs)
     */
    fcStart = PAGESIZE;
    fcEnd = header.fcMac;

    /*
     * Remember number of bytes of text already output, the amount by
     * which character indices in CHPs and PAPs will be offset.
     */
    initial_text = _wri_cpMac;	/* From text.c */

    /*
     * Append CHPs to the list of CHPs, PAPs to the list of PAPs and
     * append the text onto the end of the temp file.
     * However, do not include text that is marked as being a page header
     * or footer, and beware of the bogus final PAP that refers to two
     * characters beyond the end of the existing text.
     *
     * If any of this fails, restore to the position before we started
     * adding text.  For this reason, we do CHPs and PAPs first, because
     * they are more easily undone than removing text from the end of the
     * temporary text file.
     *
     * CHPs: we need to map the font codes used in the document into the
     * font codes for the document we are defining.
     */

    /* Read the font info */
    if ((what & WRI_CHAR_INFO) && read_fonts(ifp, &header)) {
	/* We don't need to forget the new fonts - they don't do any harm */
	goto fail;
    }

    /*
     * There is a one-to-one correspondence between PAPs and the text, and
     * they tell us which paragraphs are running headers and footers and
     * should hence be ignored.	 For this reason, read_paps() must come first,
     * to set fcStart so that read_text() and read_chps() know how much text
     * to treat.
     */
    _wri_breakpoint_pap();
    _wri_breakpoint_text();	/* breakpoint text even if we don't read it */
    _wri_breakpoint_chp();

    if ((what & WRI_TEXT) || (what & WRI_PARA_INFO) ) {
	/* Read PAPs, and maybe tab info */
	if (read_paps(ifp, &header, 1, what & WRI_TABS)) {
	    goto rollback;
	}

	if (what & WRI_TEXT)
	    if (read_text(ifp, &header)) {
		goto rollback;
	    }

	/* Text with char info: read CHPs. */
	if (what & WRI_CHAR_INFO) {
	    if (read_chps(ifp, &header)) {
		goto rollback;
	    }
	} else {
	    /* Text without char info: extend the current CHP to cover the new
	     * text */
	    _wri_extend_chp(fcEnd - fcStart + initial_text);
	}

	/* If the document ended with a paragraph break, we must create a new
	 * empty paragraph for the following text (otherwise it gets added to
	 * the last paragraph in the document).	 If this results in a bogus
	 * empty paragraph, no problem because this is checked for in the
	 * saving code.
	 * read_text() has side-effected _wri_last_char_read to the value of the
	 * final character in the Write file.  If it was \n or \f, it ended with
	 * a paragraph break.
	 */
	switch (_wri_last_char_read) {
	case '\n':
	case '\f':
	    _wri_new_paragraph();
	    break;
	}
    } else {
	/* They don't want the text (which implies the PAPs), but if they want
	 * tab settings, we must read a PAP to get them */
	if (what & WRI_TABS) {
	    /* Read tab settings without reading PAPs. */
	    if (read_paps(ifp, &header, 0, 1)) goto rollback;
	}
    }

    /* Read section info if required */
    if ((what & WRI_DOCUMENT) && read_section(ifp, &header)) {
	/* If read_section fails, it doesn't modify the section info, but we
	 * do want to undo the rest of the stuff. */
	goto rollback;
    }

    (void) fclose(ifp);
    return(0);

    /* Failure after breakpointing: roll everything back to where it was. */
rollback:
    _wri_rollback_text();
    _wri_rollback_chp();
    _wri_rollback_pap();

fail:
    (void) fclose(ifp);
    return(1);
}

static int
read_header(FILE *ifp, struct wri_header *hp)
{
    return(read_page((PN)0, sizeof(*hp), (char *)hp, ifp));
}

/* Copy the relevant text into the temp file.
 * Its CHPs and PAPs are already present.
 */
static int
read_text(FILE *ifp, struct wri_header *hp)
{
    if (fseek(ifp, fcStart, SEEK_SET) != 0) return(1);
    if (_wri_append_text(ifp, fcEnd - fcStart)) return(1);
    return(0);
}

static int
read_chps(FILE *ifp, struct wri_header *hp)
{
    PN pn;  /* page number of page of CHPs that we are reading */
    CP fcFirst, fcLim;	/* Start and end of text for current CHP,
			 * as offsets in the Write file */

    /* For each page of CHP info... */
    for (pn = pnChar(*hp); pn < hp->pnPara; pn++) {
	struct FKP fkp; /* page of CHP info being decoded... */
	int i;

	/* ...read it into our buffer... */
	if (read_page(pn, PAGESIZE, (char *)&fkp, ifp)) return(1);

	fcFirst = fkp.fcFirst;
	for (i=0; i < (int)fkp.cfod; i++) {
	    struct CHP chp;
	    int bfprop;

	    fcLim = fkp.rgFOD[i].fcLim;
	    bfprop = fkp.rgFOD[i].bfprop;

	    /* Check that the CHP refers to a part of the text that we are
	     * going to copy (ie not to initial running head code paragraphs)
	     */
	    if (fcLim > fcStart) {
		/* Allow for the possibility that a CHP spans the initial
		 * rhc paragraphs and the first textual paragraph.  In fact,
		 * this is unnecessary because we don't use fcFirst.
		 * if (fcFirst < fcStart) {
		 *     fcFirst = fcStart;
		 * }
		 */
		memcpy(&chp, &_wri_default_chp, sizeof(chp));
		if (bfprop == -1) {
		    /* Default CHP */
		} else {
		    struct FPROP *fpropp;

		    fpropp = (struct FPROP *) &(fkp.rgFPROP[bfprop]);
		    /* Copy in the part that differs from the default CHP */
		    memcpy(&chp, &(fpropp->chp), fpropp->cch);
		}
		/* Map font code */
		chp.ftc = font_map[chp.ftc];

		/* Set ignored byte to the same as the default so that the
		 * comparison with the default CHP works. */
		chp.res1 = _wri_default_chp.res1;
		chp.res2 = _wri_default_chp.res2;

		_wri_append_chp(&chp, initial_text + fcLim - fcStart);
	    }

	    /* The next one starts where this one leaves off */
	    fcFirst = fcLim;
	}
    }

    /* Check that the coverage of the CHPs is right */
    if (fcLim != fcEnd) {     
#ifdef _WINDOWS
	char szmess[80];
	wsprintf(szmess, "read_chps: fcLim=%ld should be fcEnd=%ld.", fcLim, fcEnd);
	MessageBox( 0, szmess, "WriteKit in error", MB_OK|MB_ICONSTOP );
#else
	fprintf(stderr, "read_chps: fcLim=%ld should be fcEnd=%ld.\n", fcLim, fcEnd);
#endif	    
	return(1);
    }

    return(0);
}

/*
 * Read non-rhc PAPs into our document.	 Assumes that the PAPs are present
 * in the same order as the text in the document, and that they cover the
 * text exactly.
 * If want_paps is non-zero, read all the PAPs ontp the end of our document.
 * If want_tabs is non-zero, set tabs according to the settings in the first
 * PAP of the document (in a write file, every PAP contains an identical copy
 * of the tab settings, as in Word format, even though it wastes space.
 * At least one of want_tabs and want_paps will always be 1 (otherwise why are
 * they calling us?!)
 */
static int
read_paps(FILE *ifp, struct wri_header *hp, int want_paps, int want_tabs)
{
    PN pn;  /* page number of page of PAPs that we are reading */
    /*
     * While we are reading header and footer paragraphs, ignoring_rhc
     * remains == 1, and when we find the first paragraph of the text, we set
     * fcStart to the byte number in the file of the start of that
     * paragraph. We subsequently ignore the text and CHPs that refer to the
     * rhc paragraphs.
     */
    int ignoring_rhc = 1;
    FC fcLimLast;	/* File index in input file of the character after
			 * last significant PAP, for checking purposes */
    int is_first_para = 1;  /* Is this the first paragraph we have read? */

    /* If they don't want anything; that's easy! (This cannot happen) */
    if ((!want_paps) & (!want_tabs)) return(0);

    /* For each page of PAP info... */
    for (pn = hp->pnPara; pn < hp->pnFntb; pn++) {
	struct FKP fkp; /* page of PAP info being decoded... */
	CP fcFirst, fcLim;  /* Start and end of text for current PAP,
			     * as offsets in the Write file */
	int i;

	/* ...read it into our buffer... */
	if (read_page(pn, PAGESIZE, (char *)&fkp, ifp)) return(1);

	fcFirst = fkp.fcFirst;
	fcLimLast = fcFirst;
	for (i=0; i<(int)fkp.cfod; i++) {
	    struct PAP pap;	/* Current PAP */
	    struct FPROP *fpropp;
	    int bfprop;

	    fcLim = fkp.rgFOD[i].fcLim;
	    bfprop = fkp.rgFOD[i].bfprop;

	    memcpy(&pap, &_wri_default_pap, sizeof(struct PAP));
	    if (bfprop == -1) {
		/* Default PAP */
	    } else {
		fpropp = (struct FPROP *) &(fkp.rgFPROP[bfprop]);
		/* Copy in the part that differs from the default PAP */
		memcpy(&pap, &(fpropp->pap), fpropp->cch);
	    }

	    if (pap.rhcOdd == 0) {
		/* If this is the first PAP referring to real text, text
		 * starts here. */
		if (ignoring_rhc) {
		    ignoring_rhc = 0;
		    fcStart = fcFirst;
		}
	    }

	    /* Import tabs from the first PAP if required.  Tab settings are
	     * included in all PAPs, including those for header/footers.
	     */
	    if (want_tabs) {
		_wri_set_tabs(pap.rgtbd);
		/* Are the tab settings all they wanted? */
		if (!want_paps) return(0);
		want_tabs = 0;	/* Do it just once */
	    }

	    /* All PAPs except initial rhcs get included - even if they are
	     * bogus RHCs that occur after normal text has been encoutered.
	     */
	    if (!ignoring_rhc) {
		/* Ignore the bogus extra paragraph that extends beyond the end
		 * of the text */
		if (fcFirst >= fcEnd) {
		    /* Bogus PAP */
		} else {
		    CP cpLim;
		    FC real_fcLim;

		    /* The bogus extra paragraph can also be included in
		     * the last PAP.  Trim the extent if this is the case.
		     */
		    real_fcLim = min(fcLim, fcEnd);

		    /* Calculate position of paragraph text in the output file.
		     * fcLim is file offset of the end of the paragraph in the
		     * input file,
		     * fcStart is the file offset of the start of the
		     * text that we will copy into the output file,
		     * so (fcLim - fcStart) is the offset of the end of
		     * the paragraph from the start of the new text.
		     * initial_text is the amount of text there was already in
		     * the output file.
		     */
		    cpLim = initial_text + real_fcLim - fcStart;

		    /* Set ignored bytes to the same as the default so that the
		     * comparison with the default CHP works. */
		    pap.res1 = _wri_default_pap.res1;
		    pap.res2 = _wri_default_pap.res2;
		    pap.res3 = _wri_default_pap.res3;
		    pap.res4 = _wri_default_pap.res4;
		    pap.res5 = _wri_default_pap.res5;

		    if (_wri_append_pap(&pap, cpLim, is_first_para)) return(1);
		    is_first_para = 0;

		    /* Remember the end of the last PAP for the final check */
		    fcLimLast = real_fcLim;
		}
	    }

	    /* The next PAP starts where this one leaves off */
	    fcFirst = fcLim;
	}
    }

    /* Check that the coverage of the PAPs is right */
    if (fcLimLast != fcEnd) {
#ifdef _WINDOWS
	char szmess[80];
	wsprintf(szmess, "read_paps: fcLimLast=%ld should be fcEnd=%ld.", fcLimLast, fcEnd);
	MessageBox( 0, szmess, "WriteKit in error", MB_OK|MB_ICONSTOP );
#else
	fprintf(stderr, "read_paps: fcLimLast=%ld should be fcEnd=%ld.\n", fcLimLast, fcEnd);
#endif	    
	return(1);
    }

    return(0);
}

/* Copy section info from an existing write file */
static int
read_section(FILE *ifp, struct wri_header *hp)
{
    struct SEP sep;	/* SEP as found in write file */

    if (hp->pnSep + 1 == hp->pnSetb && hp->pnSetb + 1 == hp->pnPgtb) {
	/* There are section properties */
	if (read_page(hp->pnSep, sizeof(struct SEP), (char *)&sep, ifp)) {
		/* Read failed - do not modify SEP */
		return(1);
	}
	/* Copy the amount of the SEP that is defined - and be careful not to
	 * overflow the amount of SEP that we define (empirically, Word
	 * saves tons of section info, though we don't know what it means).
	 * The byte _wri_sep.cch covers the whole sep, as always.
	 * The macro min(a,b) is defined in defs.h.
	 */
	_wri_set_default_sep();
	memcpy(&_wri_sep.res1, &sep.res1, min(sep.cch, sizeof(struct SEP)-1));
	/* Recalculate the variables that the user deals with */
	_wri_sep_to_user();
    } else {
	/* No section properties in file - set default values */
	_wri_set_default_sep();
    }

    return(0);
}

/*
 * Read fonts defined in file and create table to convert font codes from
 * those used in the file to those we will use in the output file.
 */
static int
read_fonts(FILE *ifp, struct wri_header *hp)
{
    char page[PAGESIZE];
    PN pn;  /* page number of page of fonts that we are reading */
    int cffn;	/* number of FFNs in current page */
    char *ffn;	/* current ffn */
    unsigned int cbFfn;
    int ftc;	/* font code */

    if (hp->pnFfntb == hp->pnMac) {
	/* No font info */
	return(0);
    }

    pn = hp->pnFfntb;
    if (read_page(pn++, PAGESIZE, page, ifp)) return(1);
    cffn = *(int *)page;
    ffn = &page[2];

    ftc = 0;
    do {
	char *font_name;
	unsigned char ffid;

	cbFfn = *(int *)ffn;
	ffn += sizeof(int);

	switch (cbFfn) {
	case 0:
	    /* end */
	    break;
	case 0xFFFF:
	    /* More fonts on new page... */
	    if (read_page(pn++, PAGESIZE, page, ifp)) return(1);
	    ffn = page;
	    break;
	default:
	    ffid = *ffn++;
	    font_name = ffn;
	    ffn += strlen(font_name)+1;

	    font_map[ftc] = _wri_cvt_font_name_to_code(font_name, ffid);
	    if (font_map[ftc] == -1) {
		/* _wri_cvt... failed */
		return(1);
	    }

	    ftc++;
	    break;
	}
    } while (cbFfn != 0);

    return(0);
}

/*
 *  Read <extent> bytes of the <n>th page (starting from 0) from the file into
 *  the buffer, detecting errors.
 *  <extent> will usually be == PAGESIZE.
 */
static int
read_page(PN n, int extent, char *buf, FILE *ifp)
{
    if (fseek(ifp, (long) n * PAGESIZE, SEEK_SET) != 0) {
	return(1);
    }

    if (fread((void *)buf, (size_t)1, (size_t)extent, ifp) != (size_t)extent) {
	return(1);
    }
    return(0);
}
