/*
 *  Library to generate write files.
 *
 *  Reinitialise everything and free allocated memory.
 *
 *  Copyright 1992 Martin Guy, Via Marzabotto 3, 47036 Riccione - FO, Italy.
 */

#include <stdio.h>
#include "libwrite.h"	/* Public definitions */
#include "write.h"	/* Data structures for Write documents */
#include "defs.h"	/* Definitions internal to the library */

static int reinit_all(void);

/*
 *  Global variable indicates whether we have encountered a fatal error
 *  such as running out of memory, failed disk write ecc.
 */
int _wri_error = 0;

int wri_err(void)
{
    return(_wri_error);
}

/*
 *  Set up for new document, freeing any allocated memory and so on
 */
int
wri_new()
{
    _wri_error = 0;

    return(reinit_all());
}

/* For exit, we need to free memory - reinitialize to do this.
 * The temporary file for text is deleted when _wri_reinit_text closes
 * its file pointer.
 */
int
wri_exit()
{
    return(reinit_all());
}

static int
reinit_all()
{
    return(
	_wri_reinit_text() ||
	_wri_reinit_chp() ||
	_wri_reinit_pap() ||
	_wri_reinit_section() ||
	_wri_reinit_font()
    );
}
