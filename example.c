	#include <stdlib.h>
	#include <libwrite.h>

	int
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
