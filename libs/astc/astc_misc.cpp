#include "astc_codec_internals.h"
#include <stdio.h>

// TODO: make these as defines in header, so code will became smaller
int rgb_force_use_of_hdr = 0;
int alpha_force_use_of_hdr = 0;
int perform_srgb_transform = 0;

void astc_codec_internal_error(const char *filename, int linenum)
{
	printf("ASTC internal error: File=%s Line=%d\n", filename, linenum);
	exit(1);
}
