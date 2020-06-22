/*
 *	Library to generate write files.
 *	Code for treatment of text
 *
 *	Public functions:
 *		Append text with current properties
 *		Add hard page breaks and other special characters
 *	Private data:
 *		list of CHPs defined for the text so far
 *	Query:
 *		What is the precise list of valid characters for output?
 *		When should we generate a new PAP in response to funny characters
 *		in the input?
 *
 *	Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */
#include <stdio.h>	/* for NULL */
#include <stdlib.h>	/* for exit() */
#include <string.h>	/* for strchr() */

#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

/*
 *	Function prototypes
 */
static int do_text(char *text);
static int create_tempfile(void);

/*
 * We memorise the text in a temporary file, because it is potentially very
 * large.  We could write it in the final output file, leaving a space for the
 * header, if we knew the name of the output file...
 */

/* Stdio file pointer for temporary file. NULL means we haven't got one. */
static FILE *text_fp = NULL;

/* Public data */
CP _wri_cpMac = 0;	/* Number of bytes of actual text */
char _wri_last_char_read; /* Last char read when appending from Write file. */
int _wri_in_rhc = 0; /* Are we defining a running head code? (Set in pap.c) */
int _wri_had_normal_text = 0; /* Have we output non-rhc text yet? */

/* User interface */
int
wri_text(char *text)
{
    char *cp;

    /* Do we need to initialise ourselves? */
    if (text_fp == NULL) {
	if(create_tempfile()) return(1);
    }

    /* Special treatment for character \001, the page number, which is only
     * valid inside running head codes, and which needs the fSpecial bit set
     * in its CHP.
     */
    if (_wri_in_rhc) {
	/* Can't define a header after you've already output normal text */
	if (_wri_had_normal_text) return(1);

	/*
	 * Do special handling of (page number) since it requires its own CHP
	 * with the fSpecial bit set.
	 */
	if ((cp = strchr(text, '\001')) != NULL) {
	    /* Output preceding text, then the 001, then the following text. */
	    char *copy = strdup(text);	/* Constant strings may be read-only */

	    *(strchr(copy, '\001')) = '\0';
	    if (do_text(copy)) { free(copy); return 1; }
	    free(copy);

	    if (_wri_chp_special(1) ||
		do_text("\001") ||
		_wri_chp_special(0)) return(1);

	    return(wri_text(cp+1)); /* recursive call to process other \001s */
	}
    }

    return(do_text(text));
}

/*
 *	Separate function really outputs the text, required because of treatment
 *	of \001 in wri_text
 */
static int
do_text(char *text)
{
    char *cp;

    for (cp=text; *cp; cp++) {
	switch (*cp) {
	/* They can specify \n or \r\n or even \n\r and we do the right thing
	 * by ignoring \r and outputting \r\n for every \n in the input. */
	case '\r':
	    /* Ignore \r */
	    continue;
	case '\n':
	    /* Insert \r */
	    if (putc('\r', text_fp) != '\r') return(1);
	    _wri_cpMac++;
	    break;  /* Followed by the \n... */

	case '\f':
	case '\t':
	    /* Accept form feed and tab */
	    break;

	case '\001':
	    /* Accept page number if inside running header or footer */
	    if (!_wri_in_rhc) continue;
	    break;

	default:
	    /* Reject all other control characters */
	    if ((*cp & ~31) == 0) continue;

	    /* Everything else is ok */
	    break;
	}

	/* Put char to file.  Characters over 127 need to be converted to int
	 * by hand otherwise they get converted to a negative value. */
	if (putc(*cp, text_fp) != (*cp & 255)) {
#ifdef _WINDOWS
	    char szmess[80];
	    wsprintf(szmess, "Error putting char %d", *cp);
	    MessageBox( 0, szmess, "WriteKit in error", MB_OK|MB_ICONSTOP );
#else
	    fprintf(stderr, "Error putting char %d\n", *cp);
#endif	    
	    
	    _wri_error = 1;  /* fatal error */
	    return(1);
	}
	_wri_cpMac++;

	/* Start new paragraph? */
	switch (*cp) {
	case '\n':
	case '\f':		/* page break also implies new paragraph */
	    _wri_extend_pap((CP) _wri_cpMac);
	    if(_wri_new_paragraph()) return(1);
	    break;
	}
    }

    /* Inform the current CHP and PAP that they should cover these characters */
    _wri_extend_chp((CP) _wri_cpMac);
    _wri_extend_pap((CP) _wri_cpMac);

    if (!_wri_in_rhc) _wri_had_normal_text = 1;

    return(0);
}

static int
create_tempfile()
{		       
    text_fp = tmpfile();  

    if (text_fp == NULL) {
	return(1);
    }

    /* The temp file is created in binary read/write mode */

    return(0);
}

int
_wri_save_text(struct wri_header *hp, FILE *ofp)
{
    char page[PAGESIZE];/* Buffer for transfer of data */
    FC nbytes_to_go;	    /* How many bytes remain to be copied from the temp file
			 * to the output file? */

    hp->fcMac = _wri_cpMac + PAGESIZE;

    if (_wri_cpMac == 0) {
	/* No text, hence no temp file either */
	return(0);
    }

    if (_wri_seek_to_page((PN)1, ofp)) return(1);

    rewind(text_fp);

    /* rewind can imply writing of last block, but never returns failure.
     * Only ferror() can tell us if this last write failed.
     */
    if (ferror(text_fp)) {
	_wri_error = 1;
	return(1);
    }

    /* Can't simply copy the whole file because if the reading of a write file
     * fails, the temporary file may have extra bogus text left at the end.
     * Use _wri_cpMac instead.
     */
    nbytes_to_go = _wri_cpMac;
    do {
	size_t block;	/* How many bytes to read in each operation */
	size_t nbytes_read;	/* How many were actually read from the file */

	block = (size_t) min(nbytes_to_go, (FC) PAGESIZE);
	nbytes_read = fread(page, (size_t)1, block, text_fp);
	if (nbytes_read != block) {
	    _wri_error = 1;
	    return(1);
	}
	if (fwrite(page, (size_t)1, (size_t)nbytes_read, ofp) != (size_t)nbytes_read) {
	    _wri_error = 1;
	    return(1);
	}
	nbytes_to_go -= nbytes_read;
    } while (nbytes_to_go > 0);

    return(0);
}

int
_wri_reinit_text()
{			   
    if (text_fp != NULL) fclose(text_fp);
    
    text_fp = NULL;

    _wri_cpMac = 0;
    _wri_had_normal_text = 0;
    _wri_in_rhc = 0;

    return(0);
}

/*
 * Raw interface: read n_to_read chars from an already-open and positioned
 * file pointer.
 * Side-effects _wri_last_char_read to the value of the last char in the file.
 */

int
_wri_append_text(FILE *ifp, CP n_to_read)
{
    CP n_read;	    /* Number of bytes transferred so far */
    int c;

    /* Do we need to initialise ourselves? */
    if (text_fp == NULL) {
	if(create_tempfile()) return(1);
    }

    for (n_read = 0; n_read < n_to_read; n_read++) {
	c = getc(ifp);
	if (c == EOF || putc(c, text_fp) != c) return(1);

    }

    /* Remember the last significant character for read.c's benefit */
    _wri_last_char_read = (char) c;

    /* Increment _wri_cpMac to cover the new bytes read in */
    _wri_cpMac += n_to_read;

    /* Can't define a running head code now that we've had text. */
    _wri_had_normal_text = 1;

    return(0);
}

/*
 * Memorise the current quantity of text so as to be able to cancel it
 * if the reading of the write file subsequently fails.
 * We roll back simply by repositioning the file pointer, so beware of the
 * possibility that the temporary file may be longer than the number of
 * significant characters in it.
 */

static CP cp_break;

void
_wri_breakpoint_text()
{
    cp_break = _wri_cpMac;
}

void
_wri_rollback_text()
{
    (void) fseek(text_fp, cp_break, SEEK_SET);
    _wri_cpMac = cp_break;
}
