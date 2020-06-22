/*
 *  Library to generate write files.
 *
 *  Definitions internal to the library.
 *  Include write.h before this to pick up type definitions.
 */

/*
 *  Generalised structure for lists of CHPS or PAPs.  Must correspond to the
 *  initial members of struct lchp and struct lpap in chp.c and pap.c
 */
struct lprop {
    struct lprop *next; /* Pointer to next element in list */
    CP cpFirst;		/* Index of first character this CHP refers to */
    CP cpLim;		/* Index into text, one past the last character that
			 * the property refers to
			 */
};

/*
 *  Function prototypes for internal interface
 */

/* In init.c */
extern int _wri_error;

/* In text.c */
extern CP _wri_cpMac;
extern char _wri_last_char_read;
extern int _wri_in_rhc;
extern int _wri_had_normal_text;
int _wri_save_text(struct wri_header *hp, FILE *ofp);
int _wri_reinit_text(void);
int _wri_append_text(FILE *ifp, CP n_to_read);
void _wri_breakpoint_text(void);
void _wri_rollback_text(void);

/* In chp.c */
extern const struct CHP _wri_default_chp;
void _wri_extend_chp(CP cpLim);
int _wri_chp_special(int x);
int _wri_save_chp(struct wri_header *hp, FILE *ofp);
int _wri_reinit_chp(void);
int _wri_set_default_chp(void);
void _wri_preserve_chp(void);
int _wri_restore_chp(void);
int _wri_append_chp(struct CHP *chpp, CP cpLim);
void _wri_breakpoint_chp(void);
void _wri_rollback_chp(void);

/* In pap.c */
extern struct PAP _wri_default_pap;
void _wri_set_tabs(struct TBD *rgtbd);
void _wri_extend_pap(CP cpLim);
int _wri_new_paragraph(void);
int _wri_save_pap(struct wri_header *hp, FILE *ofp);
int _wri_reinit_pap(void);
int _wri_append_pap(struct PAP *papp, CP cpLim, int is_first_para);
void _wri_breakpoint_pap(void);
void _wri_rollback_pap(void);

/* In prop.c */
struct lprop *_wri_new_lprop(struct lprop **lprop_currp, int lprop_size);
int _wri_find_cch(char *cp1, char *cp2, int max_chars);

/* In section.c */
extern struct SEP _wri_sep;
void _wri_set_default_sep(void);
void _wri_user_to_sep(void);
void _wri_sep_to_user(void);
int _wri_save_section(struct wri_header *hp, FILE *ofp);
int _wri_reinit_section(void);


/* In font.c */
int _wri_cvt_font_name_to_code(char *font_name, unsigned char ffid);
int _wri_save_fonts(struct wri_header *hp, FILE *ofp);
int _wri_reinit_font(void);

/* In save.c */
int _wri_seek_to_page(PN n, FILE *fp);
int _wri_write_page(char *page, FILE *ofp);

/*
 * Macro to give the minimum of two values, if not already defined
 */
#ifndef min
# define min(a, b) ((a)<(b) ? (a) : (b))
#endif
