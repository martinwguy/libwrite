/*
 *  Library to generate write files.
 *
 *  Definizioni per elementi di un archivio write.
 *
 *  Quest'archivio viene incluso dai moduli della libreria, ma non
 *  dal utente della libraria.
 *
 *  Unit of measure: 1 twip == 1/20th of a point = 1/(20 * 72) inch
 */

typedef unsigned BF;	    /* bitfield */
typedef unsigned long CP;   /* index into text of document */
typedef unsigned long FC;   /* index into Write file */
typedef unsigned short PN;	    /* Page number */
#define itbdmax 14	    /* Maximum number of tabs */

typedef unsigned char FTC;  /* Font code (lives in a 6-bit field) */

/* Misura delle pagine in un archivio WRITE */
#define PAGESIZE 128

/* These structures reflect file structures, so must have no gaps */
#pragma pack(1)

/* Struttura del header di un archivio WRITE. */
struct wri_header
{
    unsigned short wIdent;    /* Must be WRIH_WIDENT or WRIH_WIDENT_OLE */
    unsigned short dty;	    /* Must be 0 */
    unsigned short wTool;	    /* Must be WRIH_WTOOL */
    unsigned short res1;	    /* Must be 0 */
    unsigned short res2;	    /* Must be 0 */
    unsigned short res3;	    /* Must be 0 */
    unsigned short res4;	    /* Must be 0 */
    unsigned long fcMac;
    unsigned short pnPara;
    unsigned short pnFntb;
    unsigned short pnSep;
    unsigned short pnSetb;
    unsigned short pnPgtb;
    unsigned short pnFfntb;
    unsigned short szSsht[48-15];
    unsigned short pnMac;	    /* If 0, file is from MS Word */
};

/* Obligatory values for header */

#define WRIH_WIDENT 0137061	/* wIdent (for a file without OLE objects) */
#define WRIH_WIDENT_OLE 0137062 /* wIdent (for a file with OLE objects) */
#define WRIH_WTOOL  0125400	/* wTool */

/* Page number of start of character information */
#define pnChar(h) (unsigned)(((h).fcMac + PAGESIZE - 1) / PAGESIZE)

/* Character property structure */
struct CHP {
    BF res1	:8; /* Reserved; ignored by write */

    BF fBold	:1; /* Bold? */
    BF fItalic	:1; /* Italic? */
    BF ftc	:6; /* Font code */

    BF hps	:8; /* Point size in half points */

    BF fUline	:1; /* Underline? */
    BF fStrike	:1; /* Strikethrough? (ignored by write) */
    BF fDline	:1; /* Double underline? (ignored by write) */
    BF fNew	:1; /* Inserted under revision marking? (ignored by write) */
    BF csm	:2; /* Case modifier (ignored by write) */
    BF fSpecial :1; /* Use alternate char definitions?	Set for "(page)" only */
    BF fHidden	:1; /* Hidden text? (not used by write) */

    BF ftcXtra	:3; /* Font code, high-order bits (concatenated with ftc) */
    BF fOutline :1; /* Not used by Write */
    BF fShadow	:1; /* Not used by Write */
    BF res2	:3; /* Reserved; ignored by write */

    BF hpsPos	:8; /* Superscript/subscript */
};

/* definitions for CHP.hpspos */
#define hpsNormal	0   /* Neither superscript nor subscript */
#define hpsSuperscript	12  /* Superscript */
#define hpsSubscript	244 /* Subscript (8 bit integer = -12) */

/*
 *  Tab descriptor
 */
struct TBD {
    unsigned short dxa;	/* Indent from left margin of tab stop, in twips */

    BF	jcTab	    :3; /* Tab type: 0=normal, 3=decimal */
    BF	tlc	    :3; /* reserved; ignored by write */
    BF	res1	    :2; /* reserved; must be 0 */

    BF	chAlign	    :8; /* Reserved; ignored by write */
};

/*
 *  Paragraph properties
 */
struct PAP {
    BF res1	    :8; /* Reserved; must be 0 */

    BF jc	    :2; /* Paragraph alignment (see below) */
    BF res2	    :6; /* Reserved; must be 0 */

    BF res3	    :8; /* Reserved; must be 0 */
    BF res4	    :8; /* Reserved; must be 0 */

    unsigned short dxaRight;	/* right indent */
    unsigned short dxaLeft;	/* left indent */
    short	   dxaLeft1;	/* first line indent (relative to dxaLeft) */

    unsigned short dyaLine;	/* line spacing in twips. standard is 240,
				 * but -80 means auto (in Word) */
    unsigned short dyaBefore;	/* space before (not used by write: == 0) */
    unsigned short dyaAfter;	/* space after (not used by write: == 0) */

    BF rhcPage	    :1; /* 0=header, 1=footer */
    BF rhcOdd	    :1; /* 0:normal paragraph, 1: header or footer paragraph */
    BF rhcEven	    :1; /* 0:normal paragraph, 1: header or footer paragraph */
    BF rhcFirst	    :1; /* Print on first page? */
    BF fGraphics    :1; /* Paragraph type: 0=text, 1=picture */
    BF res5	    :3; /* reserved: must be 0 */

    BF res6a:8; /* reserved: must be 0.	 Can't be included in res6b because
		 * bitfields are packed into unsigned ints, which leaves a gap
		 * of an extra char */

    char res6b[4];  /* reserved: must be 0 */

    struct TBD rgtbd[itbdmax];	/* Tab descriptors */

    char endTab;	/* Zero byte terminates list of tabs if all used */
};

/* Definitions for PAP.jc (paragraph alignment) */
#define jcLeft	    0	/* left alignment */
#define jcCenter    1	/* centered */
#define jcRight	    2	/* right alignment */
#define jcBoth	    3	/* justified */

/*
 *  Structure of a FOD within a FKP
 */
struct FOD {
    FC	fcLim;		/* Byte number after last byte covered by this FOD */
    short bfprop;	/* Offset of FPROP from start of FOD array;
			 * -1 = default properties */
};

/*  Structure of an FPROP within a FKP, valid for both the CHPs and the PAPs.
 *  Attention though, because only the first few characters of each will be
 *  present in the file, and to use this structure to decode the page from the
 *  write file, the compiler must provide both packed structures
 *  that leave no space between cch and the union, and the ability to access
 *  a structure that is aligned on any byte boundary.
 */
struct FPROP {
    unsigned char cch;	/* Number of bytes in this FPROP (excluding cch) */
    union {
	struct CHP chp;
	struct PAP pap;
    };
};

/*
 *  Structure of a page of CHP or PAP info
 */
/* How many FODs can there be in a FKP? */
#define MAX_FODS ((PAGESIZE-sizeof(FC)-1)/sizeof(struct FOD))
struct FKP {
    FC	fcFirst;    /* Byte number of first char covered by this page
		     * == 128 for the start of the document. */
    /* We want to be able to access the FODs in this block as an array,
     * and also to get to the FPROPS as vari-length parts of the array. */
    union {
	struct FOD rgFOD[MAX_FODS];
	char rgFPROP[PAGESIZE - sizeof(FC) - 1];
    };
    unsigned char cfod; /* Number of FODs in this page */
};

/*
 *  Structure of section info, as found in Write file
 */

struct SEP {
    unsigned char cch;	/* count of significant characters excluding cch */
    unsigned short res1;	/* 0,1 Reserved; must be 0 */
    unsigned short yaMac;	/* 2,3 Page length; default = 15840 = 11 inch */
    unsigned short xaMac;	/* 4,5 Page width; default = 12240  = 8.5 inch */
    unsigned short pgnFirst;	/* 6,7 Number of first printed page, 1-32767 or 0xFFFF */
    unsigned short yaTop;	/* 8,9 Top margin; default = 1440 */
    unsigned short dyaText; /* 10,11 Height of text; default = 12960 = 9 inch */
    unsigned short xaLeft;  /* 12,13 Left margin; default = 1800 = 1.25 inch */
    unsigned short dxaText; /* 14,15 Width of text; default = 8640, 6 inch */
    unsigned short res2;
    unsigned short yaHeader;	/* 18,19 Position of running header from top */
    unsigned short yaFooter;	/* 20,21 Position of running footer from top */
    char pad[102-2*11]; /* Pad to the size found in write files of 102 */
};

/*
 * Page length (yaMac) = yaTop + dyaText.
 * Page width (xaMac) = xaLeft + dxaText + (right margin, not stored).
 */

/*
 *  Structure of Section descriptors as found in Write file.
 */
struct SED {
    CP cp;	/* Byte address of first character following section */
    short fn;	/* Undefined */
    FC fcSep;	/* Byte Address of associated SEP */
};

struct SETB {
    unsigned short csed;	/* Number of sections: always == 2 */
    unsigned short csedMax;	/* Undefined */
    struct SED rgSED[2];	/* Array of SEDs */
};

/*
 * Maximum number of different fonts that can be used in one document = 64
 * because the font code is memorised in a 6-bit field in the CHP structure.
 * (Actually, there are two or three more bits somewhere else, but we don't
 * use them).
 */
#define MAX_FONTS 64

/* Revert to default structure packing */
#pragma pack()
