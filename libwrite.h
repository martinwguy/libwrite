/*
 *  libwrite.h: Definizioni per utenti DOS della libreria per generazione di
 *  archivi nel formato di Microsoft Write.
 */

/*
 *  External data
 */
extern int wri_err();

/*
 *  User function prototypes
 */
/* In init.c */
extern int wri_new(void);
extern int wri_exit(void);

/* In text.c */
extern int wri_text(char *text);

/* In chp.c */
extern int wri_char_normal(void);
extern int wri_char_bold(int value);
extern int wri_char_italic(int value);
extern int wri_char_underline(int value);
extern int wri_char_script(int value);
extern int wri_char_font_name(char *font_name);
extern int wri_char_font_size(int value);
extern int wri_char_reduce(void);
extern int wri_char_enlarge(void);

/* In pap.c */
extern int wri_para_normal(void);
extern int wri_para_justify(int jc);
extern int wri_para_interline(int spacing);
extern int wri_para_indent_left(int indent);
extern int wri_para_indent_right(int indent);
extern int wri_para_indent_first(int indent);
extern int wri_doc_header(void);
extern int wri_doc_footer(void);
extern int wri_doc_return(void);
extern int wri_doc_insert_page_number(void);
extern int wri_doc_pofp(int print);
extern int wri_doc_tab_set(int position, int decimal);
extern int wri_doc_tab_clear(int position);
extern int wri_doc_tab_cancel(void);

/* In section.c */
extern int wri_doc_number_from(int pgnFirst);
extern int wri_doc_margin_left(int margin);
extern int wri_doc_margin_top(int margin);
extern int wri_doc_margin_right(int margin);
extern int wri_doc_margin_bottom(int margin);
extern int wri_doc_page_width(int width);
extern int wri_doc_page_height(int height);
extern int wri_doc_distance_from_top(int distance);
extern int wri_doc_distance_from_bottom(int distance);

/* In read.c */
extern int wri_open(char *filename);
extern int wri_read(char *filename, int what);

/* In save.c */
extern int wri_save(char *filename);

/*
 * Definitions for the parameter to _wri_char_script().
 * Write recognises 0, 1-127 and 128-255; these values are those used by
 * Microsoft Word.
 */
#define WRI_NORMAL	0   /* Neither superscript nor subscript */
#define WRI_SUPERSCRIPT 12  /* Superscript */
#define WRI_SUBSCRIPT	244 /* Subscript (8 bit integer = -12) */

/* Definitions for paragraph alignment, the parameter to _wri_para_justify() */
#define WRI_LEFT	0   /* left alignment */
#define WRI_CENTER	1   /* centered */
#define WRI_RIGHT	2   /* right alignment */
#define WRI_BOTH	3   /* justified */

/* Definitions for interline spacing */
#define WRI_SINGLE	((unsigned)240) /* Single (12 pt) */
#define WRI_ONE_1_2	((unsigned)360) /* 1 1/2 (18 pt) */
#define WRI_DOUBLE	((unsigned)480) /* Double (24 point) */

/* Definitions for type of tabstop: WRI_NORMAL and... */
#define WRI_DECIMAL 3	/* Value in PAP.jcTab for decimal tab stop */

/* Definitions for the parameter <what> to _wri_read() */
#define WRI_TEXT 1
#define WRI_CHAR_INFO 2
#define WRI_DOCUMENT 4
#define WRI_TABS 8
#define WRI_ALL 15
#define WRI_PARA_INFO 16

/* Macros to be able to express distances in inches or centimeters
 * for functions that require distances in TWIPs.  1 TWIP = 1/20 * 1/72 inch
 */
#define WRI_IN(i)   ((int)((i) * 1440))
#define WRI_CM(c)   ((int)((c) * 567))	/* 1440 / 2.54, avoiding use of floating point */
