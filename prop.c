/*
 *  Library to generate write files.
 *
 *  Code for treatment of properties: used for CHPs and PAPs.
 */

#include <stdio.h>  /* for NULL */
#include <malloc.h> /* for malloc() */
#include <string.h> /* for memcpy() */
#include <memory.h> /* for memcpy() */
#include "write.h"
#include "libwrite.h"
#include "defs.h"

/*
 * Allocate a new property structure, adding it to the end of the list.
 * We are passed the address of the pointer to the last item in the list
 * (because we must modify it to point to the new item), and the size of the
 * structure in question.
 * We copy the contents of the old structure into the new.
 *
 * Returns the address of the new element that we add to the end of the list,
 * or NULL if we are out of memory.
 */
struct lprop *
_wri_new_lprop(struct lprop **lprop_currp, int lprop_size)
{
    struct lprop *lprop_new;
    struct lprop *lprop_curr = *lprop_currp;	/* for easy access */

    lprop_new = (struct lprop *) malloc(lprop_size);
    if (lprop_new == NULL) {
	_wri_error = 1;	/* Fatal error */
	return(NULL);
    }

    /* Copy info from old into new element */
    memcpy(lprop_new, lprop_curr, lprop_size);

    /*
     * The new CHP starts where the previous one finished, and finishes
     * there too as it doesn't refer to any characters yet.
     */
    lprop_new->cpFirst = lprop_curr->cpLim;
    /* lprop_new->cpLim = lprop_curr->cpLim already from memcpy */

    /* Add it to end of the list and advance lchp_curr to point to it. */
    lprop_new->next = NULL;
    lprop_curr->next = lprop_new;
    lprop_curr = lprop_new;

    *lprop_currp = lprop_curr;

    return(lprop_curr);
}

/*
 * How many characters of the two structures need we specify to give all
 * those that are different from the default structure?
 */
int
_wri_find_cch(char *cp1, char *cp2, int max_chars)
{
    int cch;

    /*
     * Seek back from the end of the structures until we find a character
     * that differs.
     * Minimum cch for CHPs and PAPs is 1.
     */
    for (cch=max_chars - 1; cch > 0; cch--) {
	if (cp1[cch] != cp2[cch]) break;
    }

    /* cch is left as the index of the last byte we need to specify.
     * The number of bytes we need to specify is one greater than this.
     */
    cch++;

    return(cch);
}
