= Introduction =

Libwrite ia a C library that allows you to create documents in the format
used by Microsoft Write, directly from a program.

Most of the library's functions correspond to the menu items in Write itself.

If you already know Write and C programming, generating Write files
is very simple.

== Limitations ==

Libwrite does not handle the printing functions from the File menu,
nor the Find/Replace functions.

When a font is specified for the following text, the library does not check
that the font is installed on the current system, and if the font is not
known by the library, its "font family" is not assigned.

libwrite knows the font families of the following fonts:
Arial, Courier, Dutch SWA, Helv, Modern, Roman, Roman 5cpi, Roman 6cpi,
Roman 10cpi, Roman 12cpi, Roman 15cpi, Roman 17cpi, Roman 20cpi, Roman PS,
Script, Symbol, Terminal, Tms Rmn.

You cannot include more than 64 different fonts in a Write document.

libwrite does not handle OLE (Object Linking and Embedding) objects.

== General considerations ==

For some functions, you need to specify distances in "twips", which are
equivalent to a twentieth of a point, a 1440th of an inch.

To make the conversions easier, there are two macros, WRI_IN(x) and WRI_CM(x)
which let you think in inches or centimeters.

Example

	wri_para_indent_left(WRI_CM(0.5));

sets the paragraph indent to half a centimeter.

== Special notes ==

Libwrite has one function that the Write program does not have, the ability
to read a Write document inside the current one.

This functionality allows you to generate a Write document from other
Write files that you prepared beforehand.

All the public functions of the library start with the prefix "wri_" and
all the constants (#defines) start with "WRI_".

To use the function in the library, you need to #include the file "libwrite.h".

== Error management ==

All the functions in libwrite return 0 (zero) if they finish correctly;
otherwise they return a non-zero value, in which case the function that
you called has no effect.

If a fatal error occurs, such as running out of memory or problems
writing to the disk, the wri_err() function will return a non-zero value
from then on.

= libwrite's functions =

== Functions for managing files ==

=== wri_new ===

Initializes all parameters to their default values and starts a new document.

	int wri_new(void);

It frees all memory that was being used to remember the current document.

It is not necessary for a program to call wri_new() when it starts creating
the first document, as all default values are already set up by the library
itself.

It cannot fail and it sets the value to wri_err to 0.

=== wri_open ===

Opens a Write file as the current document.

	int wri_open(char *filename);

	filename: The name of the file to open, with path name and extension.

Using wri_open() forgets any existing text, settings or other operations
performed on the current document (like wri_new)

If an error happens, the current document is forgotten anyway. This function
can give an error if

    - it cannot read the file
    - the file is not in Write format
    - the file contains OLE objects
    - the file is damaged
    - there's not enough space in memory or on the disk to memorize
      the contents of the file

=== wri_read ===

Adds another Write file to the current document.

	int wri_open(char *filename, int what);

	filename: The name of the file to read, with path name and extension.

	what: Specifies which kinds of information to read from the file.

wri_read can be used to read, as well as the text, other characteristics
of the Write file, by orring together into "what" the following #defines:

	WRI_TEXT	Read only the text and paragraph layout

	WRI_CHAR_INFO	Read the character properties from the document
			(bold, italic, underline, font, size, super/subscript).

	WRI_DOCUMENT	Read the document properties (page width and height,
			page margins and distance of header and footer from
			the top and bottom edges).

	WRI_TABS	Read the position of the document's tab stops.

	WRI_ALL		Read all of the above (it is the logical OR of the
			above flags).

	WRI_PAR_INFO	Read only the paragraph format, but not the text.

Examples

	WRI_TEXT | WRI_CHAR_INFO
			Reads the text and character attributes

	WRI_ALL & ^WRI_DOCUMENT
			Read everything except the page size and header/footer
			distances

If the function fails, the current document is not modified in any way.

It fails if:

    - The file is not in Write format
    - The file contains OLE objects
    - It cannot read the file
    - The file is damaged
    - There is not enough memory or disk space to memorize the file's contents

Since the tab stops have a single setting for the whole document, it could
modify the appearance of text that you have previously defined.

After inserting a document with wri_read, the paragraph layout remains the
same as the last paragraph in the inserted document. Similarly, if
WRI_CHAR_INFO are specified, the character properties remain the same as
the last character of the document that was inserted.

If WRI_CHAR_INFO is given without WRI_TEXT, there is no effect on the
following characters, but it has the side effect of adding the file's
font table to the current document', adding any fonts not forseen in the
"Limitations" section above.

Files containing OLE objects will not be read, though it can read Write files
that contain bitmap images.

Headers and footers will not be imported from the document that is read.

If the text that was inserted into the document before calling wri_read
did not end with a new paragraph (that is \n or \f), then the first paragraph read from the file will be appended to the previous one, and will take on its
formatting.

=== wri_save ===

Saves the current document on the disk with the specified filename.

	int wri_save(char *filemane);

	filename: The name of the file to save in, including path and extension.

wri_save does not add the ".wri" extension automatically, so you have to
include it.

It's a good idea to check the wri_err function before calling wri_save
in case any fatal errors occured during the creation of the document.

The function fails if it cannot create the named file, or if there is
insufficient space on the disk.

=== wri_exit ==

Indicates that all operations on the current document are finished.

	int wri_exit(void);

wri_exit frees all memory allocated to the current document and signals
the end of document operations. It does not prepar for a new document.
Once it has been called, no other function of the library may be called.

It cannot fail.

== Functions for managing characters ==

The following functions correspond to the items in Write's "Character" menu
and they modify the properties of characters that are subsequently inserted.

All the following functions can fail if there's not enough memory available
to record the new settings, or if a parameter has an unexpected value.

=== wri_text ===

Adds a string of text to the document

	int wri_text(char *text);

	text: The string of text to insert into the document

This adds a string of text to the end of the document, with the current
text and paragraph settings. The string can be composed of Windows characters
as well as the following controol characters:

	\n	start a new paragraph
	\f	starts a new physical page (and a new paragraph)
	\t	tabulation character
	\001	insert the number of the current page (only valid in
		headers and footers)

Any other character will be ignored.

There is no limit to the length of the "text" string.

It can only fail if it cannot create or write to its temporary file
(e.g. if the disk is full).

=== wri_char_normal ===

Cancels any bold, italic, underline or sub/superscript settings.

	int wri_char_normal(void);

=== wri_char_bold ===

Manages characters' boldness.

	int wri_char_bold(int value)

	value:	Activates or disactivates the effect:

value == 1 turns bold on; value == 0 turns it off.

=== wri_char_italic ===

Manages characters' italicness.

	int wri_char_italic(int value)

	value:	Activates or disactivates the effect:

value == 1 turns italics on; value == 0 turns them off.

=== wri_char_script ===

Manages superscript and subscript

	int wri_char_script(int value)

	value:	Activates or disactivates the effect:

value == WRI_SUBSCRIPT activates subscripted characters.

value == WRI_SUPERSCRIPT activates superscripted characters.

value == WRI_NORMAL disactivates them and returns to normal text.

You can pass from subscripted to superscripted characters without having
to return to normal characters first.

In practice, this implies a reduction in character size, which is undone
when you leave sub/superscript mode, to the same sizes as are used by
wri_char_reduce() and wri_char_enlarge().

=== wri_char_font_name ===

Selects the character font.

	int wri_char_font_name(char *font_name);

	font_name: The name of the chosen font.

The default value is Arial.

This function fails if you go over the maximum number of different fonts (64).

=== wri_char_font_size ===

Chooses the size of the font

	int wri_char_font_size (int value);

	value: size of the font in points

This modifies the font's height to the specified value, which is in 72nds
of an inch. "value" can be from 4 to 127 and the default value is 10.

=== wri_char_reduce ===

Shrinks the size of the current font to the next smallest dimension in
the following list: 6, 8, 10, 12, 14, 18, 24, 30, 36, 48 points.

	int wri_char_reduce(void);

This function has no effect if you try to reduce below 6 points.

=== wri_char_enlarge ===

Grows the size of the current font to the next largest dimension in
the following list: 6, 8, 10, 12, 14, 18, 24, 30, 36, 48 points.

	int wri_char_enlarge(void);

This function has no effect if you try to enlarge bwyond 48 points.

== Functions to manage paragraphs ==

The following functions, present in Write's "Paragraph" menu, determine
the layout of the text in the current paragraph, and in the ones that follow.

Unlike the character-related functions, these functions affect any text that
has already been inserted into the current paragraph.

All the following functions can fail if there is insufficient memory to
remember the new format, or if they are given unforeseen parameters.

=== wri_para_normal ===

Assigns the default values to all characteristics of a paragraph.

	int wri_para_normal(void);


By default, a paragraph is left-justified, has single line spacing and is
not indented.

=== wri_para_justify ===

Sets the type of text alignment.

	int wri_para_justify(int jc);

	jc: Type of alignment required

jc == WRI_LEFT aligns text on the left side, leaving a ragged right edge.

jc == WRI_CENTER centres each row of text.

jc == WRI_RIGHT aligns the right edge of the text, leaving a ragged left edge.

jc == WRI_BOTH aligns both the left and right edges of the text ("justified").

The default value is WRI_LEFT.

=== wri_para_interline ===

Sets the distance between the lines of a paragraph

	int wri_para_interline(int spacing);

	spacing: a value giving the distance in 1/1440ths of an inch

spacing == WRI_SINGLE give single spacing between the lines.

spacing == WRI_ONE_1_2 gives one and a half times single spacing.

spacing == WRI_DOUBLE gives double line spacing.

You can directly specify the spacing between lines in 1/1440ths of an inch.
The values corresponding to the above #defines are240, 360 and 480 respectively,
corresponding to 12, 18 and 24 points, or 6, 4 and 3 lines per inch.

=== wri_para_indent_left ===

Sets the paragraph indent with respect to the left page margin.

	int wri_para_indent_left(int indent);

	indent: a value indicating the indentation in twips.

Default value: 0.

=== wri_para_indent_right ===

Sets the paragraph indent with respect to the right page margin.

	int wri_para_indent_right(int indent);

	indent: a value indicating the indentation in twips.

Default value: 0.

=== wri_para_indent_first ===

Sets the indentation of the first line of each paragraph,
with respect to the left page margin.

	int wri_para_indent_first(int indent);

	indent: a value indicating the indentation in twips.

You can also use negative values and the default value is 0.

== Functions to manage the document ==

These functions control the latout of the text in all pages of the document
and correspond to the items in Write's "Document" menu.

You can specify the desired page margins with respect to the edges of the paper.

Normally, Write calculates the height and width of the paper according to the
driver used by Windows, which specifies the size of the paper to use.

Since libwrite does not analyse the Windows drivers is use, there are
functions here that allow you to modify the height and width of the page
from their default values of 11 and 8.25 inches (standard format A4).

The following functions can only fail if they are passed values outside
the permitted limits. See the General Notes above, on "twips".

=== wri_doc_number_from ===

Specifies the page number that will be assigned to the first page of the
document.

	int wri_doc_number_from(int pgnFirst):

	pgnFirst: The number of the first page

The page number is printed if the header or the footer contain a \001 character.

The default value is 1, and the limits are from 1 to 127.

=== wri_doc_martin_left ===

Specifies the disrance between the left margin and the left edge of the page.

	int wri_doc_martin_left(int margin):

	margin: The left page margin, in twips

The default value is 1800 (1.25 inches) and it accepts values from 0 to 32767.

=== wri_doc_martin_top ===

Specifies the distance between the top margin and the top edge of the page.

	int wri_doc_martin_top(int margin):

	margin: The left page margin, in twips

The default value is 1440 (1 inch) and it accepts values from 0 to 32767.

=== wri_doc_martin_right ===

Specifies the distance between the right margin and the right edge of the page.

	int wri_doc_martin_right(int margin):

	margin: The left page margin, in twips

The default value is 1800 (1.25 inches) and it accepts values from 0 to 32767.

=== wri_doc_martin_bottom ===

Specifies the distance between the bottom margin and the bottom edge of the
page.

	int wri_doc_martin_bottom(int margin):

	margin: The left page margin, in twips

The default value is 1440 (1 inch) and it accepts values from 0 to 32767.

=== wri_doc_martin_bottom ===

Specifies the distance between the bottom margin and the bottom edge of the
page.

	int wri_doc_martin_top(int margin):

	margin: The left page margin, in twips

The default value is 1440 (1 inch) and it accepts values from 0 to 32767.

=== wri_doc_page_width ===

Specifies the width of the printed page.

	int wri_doc_page_width(int width);

	width: The width of the printed page, in twips.

The default value is 12240 (8.25 inches) and it accepts values from 1 to 32767.

=== wri_doc_page_height ===

Specifies the height of the printed page.

	int wri_doc_page_height(int height);

	height: The height of the printed page, in twips.

The default value is 15840 (11 inches) and it accepts values from 1 to 32767.

=== wri_doc_tab_set ===

Specifies the height of the printed page.

	int wri_doc_tab_set(int position, int decimal);

	position: The position of the tab stop
	decimal: The type of tabulator required

Tabulators indicate the distance from the left margin in twips.

If "decimal" is 0, text will be aligned at the tabstop. If its value is 1,
this specified a decimal tab stop, at which the decimal points will be aligned.
You can also use the constants WRI_NORMAL and WRI_DECIMAL.

Here is some example code to set a tab stop every half inch, using the return
value to know when all the tab stops have been assigned.

	int tabstop;

	tabstop = 0;
	do {
	    tabstop += WRI_IN(0.5);
	} while wri_doc_tab_set(tabstop, WRI_NORMAL) == 0);

The function wil lfail if "position" is negative or zero, of if 14 tab stops
have already been assigned (Write only allows you to modify the first 12).
By default, no tab stops are initially set.

=== wri_doc_tab_clear ===

Eliminate a tab stop.

	int wri_doc_tab_clear(int position);

	position: The position of the tabulator in twips.

This function fails if "position" is negative or zero, or if no tab stop
has been set at the specified position.

=== wri_doc_tab_cancel ===

Eliminate all the defined tab stops.

	int wri_doc_tab_clear(void);

== Management of the page header and footer ==

The page header and footer must be defined before any other text is entered
into the body of the document. If you try to define them after having inserted
some text into the document, their contents will simply be added to the text
already in the body of the document.

As in Write, the header and footer normally use the standard font and layout.
If you wish to modify those characteristics, they must be defined during the
definition of the header and the footer, i.e. after having called
wri_doc_header() or wri_doc_footer().

If you wish to insert the number of the current page in the header or footer,
you can do so by calling wri_text("\001");

=== wri_doc_header ===

Indicates the start of the settings for the page header

	int wri_doc_header(void);

This function fails if you have already sent some text to the current document.

=== wri_doc_footer ===

Indicates the start of the settings for the page footer

	int wri_doc_footer(void);

This function fails if you have already sent some text to the current document.

=== wri_doc_return ===

Returns from setting of the document's header or footer.

	int wri_doc_return(void);

Note that you can pass straight from defining the header to defining the footer
without having to call wri_doc_return() in between.

=== wri_doc_pofp ===

Indicates whether the header and/or the footer should be printed on the
first page of the document.

	int wri_doc_pofp(int print);

	print: Whether to show on the first page

If "print" is 0, the header or the footer will not be printed on the first page,
or if it is 1, they will be printed there too. The default value is 0.

This function must b called during the definition of the header or footer.

"pofp" is an acronym for "Print On First Page".

This function fails if it is called outside the definition of the header
or footer.

=== wri_doc_distance_from_top ===

Defines the distance of the page headers from the top edge of the page.

	int wri_doc_distance_from_top(int distance);

	distance: THe distance in twips

This function can be called at any time, non necessarily while you are defining
a page header or footer.

The default value is 1080, corresponding to 0.75 inches, and the distance can
range from 0 to 31680 (22 inches).

=== wri_doc_distance_from_bottom ===

Defines the distance of the page footers from the bottom edge of the page.

	int wri_doc_distance_from_bottom(int distance);

	distance: THe distance in twips

This function can be called at any time, non necessarily while you are defining
a page header or footer.

The default value is 1080, corresponding to 0.75 inches, and the distance can
range from 0 to 31680 (22 inches).

Example

	#include <stdlib.h>
	#include <libwrite.h>

	main()
	{
	    /* Set the header for the whole document */
	    wri_doc_header();
	    wri_char_font_name("Helv");
	    wri_para_justify(WRI_RIGHT);
	    wri_text("Manuale della WriteKit" );

	    /* Insert the page number at the foot of every page */
	    wri_doc_footer();
	    wri_char_font_name("Helv");
	    wri_para_justify(WRI_CENTER);
	    wri_text("- \001 -");
	    wri_doc_pofp(1);

	    /* Go back to normal text */
	    wri_doc_return();
	
	    /* Add some text and save the document in the file example.wri */
	    wri_text("Writing some other text");
	    wri_save("example.wri");
	}

== Handling fatal errors ==

=== wri_err ===

Is used to know if libwrite has failed unrecoverably at smoe point.

	int wri_err(void);

It returns a non-zero value if some fatal error has occurred, or 0 if not.

Fatal errors are things like running out of memory or being unable to write to
the hard disk.
