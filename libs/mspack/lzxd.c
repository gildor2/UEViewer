/* This file is part of libmspack.
 * (C) 2003-2004 Stuart Caie.
 *
 * The LZX method was created by Jonathan Forbes and Tomi Poutanen, adapted
 * by Microsoft Corporation.
 *
 * libmspack is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 *
 * For further details, see the file COPYING.LIB distributed with libmspack
 */

/* LZX decompression implementation */

#include "system.h"		// Gildor: original: <system.h>
#include "lzx.h"		// Gildor: original: <lzx.h>

/* Microsoft's LZX document (in cab-sdk.exe) and their implementation
 * of the com.ms.util.cab Java package do not concur.
 *
 * In the LZX document, there is a table showing the correlation between
 * window size and the number of position slots. It states that the 1MB
 * window = 40 slots and the 2MB window = 42 slots. In the implementation,
 * 1MB = 42 slots, 2MB = 50 slots. The actual calculation is 'find the
 * first slot whose position base is equal to or more than the required
 * window size'. This would explain why other tables in the document refer
 * to 50 slots rather than 42.
 *
 * The constant NUM_PRIMARY_LENGTHS used in the decompression pseudocode
 * is not defined in the specification.
 *
 * The LZX document does not state the uncompressed block has an
 * uncompressed length field. Where does this length field come from, so
 * we can know how large the block is? The implementation has it as the 24
 * bits following after the 3 blocktype bits, before the alignment
 * padding.
 *
 * The LZX document states that aligned offset blocks have their aligned
 * offset huffman tree AFTER the main and length trees. The implementation
 * suggests that the aligned offset tree is BEFORE the main and length
 * trees.
 *
 * The LZX document decoding algorithm states that, in an aligned offset
 * block, if an extra_bits value is 1, 2 or 3, then that number of bits
 * should be read and the result added to the match offset. This is
 * correct for 1 and 2, but not 3, where just a huffman symbol (using the
 * aligned tree) should be read.
 *
 * Regarding the E8 preprocessing, the LZX document states 'No translation
 * may be performed on the last 6 bytes of the input block'. This is
 * correct.  However, the pseudocode provided checks for the *E8 leader*
 * up to the last 6 bytes. If the leader appears between -10 and -7 bytes
 * from the end, this would cause the next four bytes to be modified, at
 * least one of which would be in the last 6 bytes, which is not allowed
 * according to the spec.
 *
 * The specification states that the huffman trees must always contain at
 * least one element. However, many CAB files contain blocks where the
 * length tree is completely empty (because there are no matches), and
 * this is expected to succeed.
 *
 * The errors in LZX documentation appear have been corrected in the
 * new documentation for the LZX DELTA format.
 *
 *     http://msdn.microsoft.com/en-us/library/cc483133.aspx
 *
 * However, this is a different format, an extension of regular LZX.
 * I have noticed the following differences, there may be more:
 *
 * The maximum window size has increased from 2MB to 32MB. This also
 * increases the maximum number of position slots, etc.
 *
 * The format now allows for "reference data", supplied by the caller.
 * If match offsets go further back than the number of bytes
 * decompressed so far, that is them accessing the reference data.
 */

/* import bit-reading macros and code */
#define BITS_TYPE struct lzxd_stream
#define BITS_VAR lzx
#define BITS_ORDER_MSB
#define READ_BYTES do {			\
    unsigned char b0, b1;		\
    READ_IF_NEEDED; b0 = *i_ptr++;	\
    READ_IF_NEEDED; b1 = *i_ptr++;	\
    INJECT_BITS((b1 << 8) | b0, 16);	\
} while (0)
#include "readbits.h" // Gildor: original: <readbits.h>

/* import huffman-reading macros and code */
#define TABLEBITS(tbl)      LZX_##tbl##_TABLEBITS
#define MAXSYMBOLS(tbl)     LZX_##tbl##_MAXSYMBOLS
#define HUFF_TABLE(tbl,idx) lzx->tbl##_table[idx]
#define HUFF_LEN(tbl,idx)   lzx->tbl##_len[idx]
#define HUFF_ERROR          return lzx->error = MSPACK_ERR_DECRUNCH
#include "readhuff.h" // Gildor: original: <readhuff.h>

/* BUILD_TABLE(tbl) builds a huffman lookup table from code lengths */
#define BUILD_TABLE(tbl)						\
    if (make_decode_table(MAXSYMBOLS(tbl), TABLEBITS(tbl),		\
			  &HUFF_LEN(tbl,0), &HUFF_TABLE(tbl,0)))	\
    {									\
        D(("failed to build %s table", #tbl))				\
	return lzx->error = MSPACK_ERR_DECRUNCH;			\
    }

#define BUILD_TABLE_MAYBE_EMPTY(tbl) do {				\
    lzx->tbl##_empty = 0;						\
    if (make_decode_table(MAXSYMBOLS(tbl), TABLEBITS(tbl),		\
                          &HUFF_LEN(tbl,0), &HUFF_TABLE(tbl,0)))	\
    {									\
	for (i = 0; i < MAXSYMBOLS(tbl); i++) {				\
	    if (HUFF_LEN(tbl, i) > 0) {					\
		D(("failed to build %s table", #tbl))			\
		return lzx->error = MSPACK_ERR_DECRUNCH;		\
	    }								\
	}								\
	/* empty tree - allow it, but don't decode symbols with it */	\
	lzx->tbl##_empty = 1;						\
    }									\
} while (0)

/* READ_LENGTHS(tablename, first, last) reads in code lengths for symbols
 * first to last in the given table. The code lengths are stored in their
 * own special LZX way.
 */
#define READ_LENGTHS(tbl, first, last) do {		\
  STORE_BITS;						\
  if (lzxd_read_lens(lzx, &HUFF_LEN(tbl, 0), (first),	\
    (unsigned int)(last))) return lzx->error;		\
  RESTORE_BITS;						\
} while (0)

static int lzxd_read_lens(struct lzxd_stream *lzx, unsigned char *lens,
			  unsigned int first, unsigned int last)
{
  /* bit buffer and huffman symbol decode variables */
  register unsigned int bit_buffer;
  register int bits_left, i;
  register unsigned short sym;
  unsigned char *i_ptr, *i_end;

  unsigned int x, y;
  int z;

  RESTORE_BITS;

  /* read lengths for pretree (20 symbols, lengths stored in fixed 4 bits) */
  for (x = 0; x < 20; x++) {
    READ_BITS(y, 4);
    lzx->PRETREE_len[x] = y;
  }
  BUILD_TABLE(PRETREE);

  for (x = first; x < last; ) {
    READ_HUFFSYM(PRETREE, z);
    if (z == 17) {
      /* code = 17, run of ([read 4 bits]+4) zeros */
      READ_BITS(y, 4); y += 4;
      while (y--) lens[x++] = 0;
    }
    else if (z == 18) {
      /* code = 18, run of ([read 5 bits]+20) zeros */
      READ_BITS(y, 5); y += 20;
      while (y--) lens[x++] = 0;
    }
    else if (z == 19) {
      /* code = 19, run of ([read 1 bit]+4) [read huffman symbol] */
      READ_BITS(y, 1); y += 4;
      READ_HUFFSYM(PRETREE, z);
      z = lens[x] - z; if (z < 0) z += 17;
      while (y--) lens[x++] = z;
    }
    else {
      /* code = 0 to 16, delta current length entry */
      z = lens[x] - z; if (z < 0) z += 17;
      lens[x++] = z;
    }
  }

  STORE_BITS;

  return MSPACK_ERR_OK;
}

/* LZX static data tables:
 *
 * LZX uses 'position slots' to represent match offsets.  For every match,
 * a small 'position slot' number and a small offset from that slot are
 * encoded instead of one large offset.
 *
 * position_base[] is an index to the position slot bases
 *
 * extra_bits[] states how many bits of offset-from-base data is needed.
 *
 * They are generated like so:
 * for (i = 0;        i < 4;  i++)  extra_bits[i] = 0;
 * for (i = 4, j = 0; i < 36; i+=2) extra_bits[i] = extra_bits[i+1] = j++;
 * for (i = 36;       i < 51; i++)  extra_bits[i] = 17;
 * for (i = 0, j = 0; i < 51; j += 1 << extra_bits[i++]) position_base[i] = j;
 */
static const unsigned int position_base[51] = {
  0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256,
  384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288,
  16384, 24576, 32768, 49152, 65536, 98304, 131072, 196608, 262144,
  393216, 524288, 655360, 786432, 917504, 1048576, 1179648, 1310720,
  1441792, 1572864, 1703936, 1835008, 1966080, 2097152
};
static const unsigned char extra_bits[51] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
  9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

static void lzxd_reset_state(struct lzxd_stream *lzx) {
  int i;

  lzx->R0              = 1;
  lzx->R1              = 1;
  lzx->R2              = 1;
  lzx->header_read     = 0;
  lzx->block_remaining = 0;
  lzx->block_type      = LZX_BLOCKTYPE_INVALID;

  /* initialise tables to 0 (because deltas will be applied to them) */
  for (i = 0; i < LZX_MAINTREE_MAXSYMBOLS; i++) lzx->MAINTREE_len[i] = 0;
  for (i = 0; i < LZX_LENGTH_MAXSYMBOLS; i++)   lzx->LENGTH_len[i]   = 0;
}

/*-------- main LZX code --------*/

struct lzxd_stream *lzxd_init(struct mspack_system *system,
			      struct mspack_file *input,
			      struct mspack_file *output,
			      int window_bits,
			      int reset_interval,
			      int input_buffer_size,
			      off_t output_length)
{
  unsigned int window_size = 1 << window_bits;
  struct lzxd_stream *lzx;

  if (!system) return NULL;

  /* LZX supports window sizes of 2^15 (32Kb) through 2^21 (2Mb) */
  if (window_bits < 15 || window_bits > 21) return NULL;

  input_buffer_size = (input_buffer_size + 1) & -2;
  if (!input_buffer_size) return NULL;

  /* allocate decompression state */
  if (!(lzx = (struct lzxd_stream *) system->alloc(system, sizeof(struct lzxd_stream)))) {
    return NULL;
  }

  /* allocate decompression window and input buffer */
  lzx->window = (unsigned char *) system->alloc(system, (size_t) window_size);
  lzx->inbuf  = (unsigned char *) system->alloc(system, (size_t) input_buffer_size);
  if (!lzx->window || !lzx->inbuf) {
    system->free(lzx->window);
    system->free(lzx->inbuf);
    system->free(lzx);
    return NULL;
  }

  /* initialise decompression state */
  lzx->sys             = system;
  lzx->input           = input;
  lzx->output          = output;
  lzx->offset          = 0;
  lzx->length          = output_length;

  lzx->inbuf_size      = input_buffer_size;
  lzx->window_size     = 1 << window_bits;
  lzx->window_posn     = 0;
  lzx->frame_posn      = 0;
  lzx->frame           = 0;
  lzx->reset_interval  = reset_interval;
  lzx->intel_filesize  = 0;
  lzx->intel_curpos    = 0;
  lzx->intel_started   = 0;
  lzx->error           = MSPACK_ERR_OK;

  /* window bits:    15  16  17  18  19  20  21
   * position slots: 30  32  34  36  38  42  50  */
  lzx->posn_slots      = ((window_bits == 21) ? 50 :
			  ((window_bits == 20) ? 42 : (window_bits << 1)));

  lzx->o_ptr = lzx->o_end = &lzx->e8_buf[0];
  lzxd_reset_state(lzx);
  INIT_BITS;
  return lzx;
}

void lzxd_set_output_length(struct lzxd_stream *lzx, off_t out_bytes) {
  if (lzx) lzx->length = out_bytes;
}

int lzxd_decompress(struct lzxd_stream *lzx, off_t out_bytes) {
  /* bitstream and huffman reading variables */
  register unsigned int bit_buffer;
  register int bits_left, i=0;
  unsigned char *i_ptr, *i_end;
  register unsigned short sym;

  int match_length, length_footer, extra, verbatim_bits, bytes_todo;
  int this_run, main_element, aligned_bits, j;
  unsigned char *window, *runsrc, *rundest, buf[12];
  unsigned int frame_size=0, end_frame, match_offset, window_posn;
  unsigned int R0, R1, R2;

  /* easy answers */
  if (!lzx || (out_bytes < 0)) return MSPACK_ERR_ARGS;
  if (lzx->error) return lzx->error;

  /* flush out any stored-up bytes before we begin */
  i = lzx->o_end - lzx->o_ptr;
  if ((off_t) i > out_bytes) i = (int) out_bytes;
  if (i) {
    if (lzx->sys->write(lzx->output, lzx->o_ptr, i) != i) {
      return lzx->error = MSPACK_ERR_WRITE;
    }
    lzx->o_ptr  += i;
    lzx->offset += i;
    out_bytes   -= i;
  }
  if (out_bytes == 0) return MSPACK_ERR_OK;

  /* restore local state */
  RESTORE_BITS;
  window = lzx->window;
  window_posn = lzx->window_posn;
  R0 = lzx->R0;
  R1 = lzx->R1;
  R2 = lzx->R2;

  end_frame = (unsigned int)((lzx->offset + out_bytes) / LZX_FRAME_SIZE) + 1;

  while (lzx->frame < end_frame) {
    /* have we reached the reset interval? (if there is one?) */
    if (lzx->reset_interval && ((lzx->frame % lzx->reset_interval) == 0)) {
      if (lzx->block_remaining) {
	D(("%d bytes remaining at reset interval", lzx->block_remaining))
	return lzx->error = MSPACK_ERR_DECRUNCH;
      }

      /* re-read the intel header and reset the huffman lengths */
      lzxd_reset_state(lzx);
      R0 = lzx->R0;
      R1 = lzx->R1;
      R2 = lzx->R2;
    }

    /* read header if necessary */
    if (!lzx->header_read) {
      /* read 1 bit. if bit=0, intel filesize = 0.
       * if bit=1, read intel filesize (32 bits) */
      j = 0; READ_BITS(i, 1); if (i) { READ_BITS(i, 16); READ_BITS(j, 16); }
      lzx->intel_filesize = (i << 16) | j;
      lzx->header_read = 1;
    }

    /* calculate size of frame: all frames are 32k except the final frame
     * which is 32kb or less. this can only be calculated when lzx->length
     * has been filled in. */
    frame_size = LZX_FRAME_SIZE;
    if (lzx->length && (lzx->length - lzx->offset) < (off_t)frame_size) {
      frame_size = lzx->length - lzx->offset;
    }

    /* decode until one more frame is available */
    bytes_todo = lzx->frame_posn + frame_size - window_posn;
    while (bytes_todo > 0) {
      /* initialise new block, if one is needed */
      if (lzx->block_remaining == 0) {
	/* realign if previous block was an odd-sized UNCOMPRESSED block */
	if ((lzx->block_type == LZX_BLOCKTYPE_UNCOMPRESSED) &&
	    (lzx->block_length & 1))
	{
	  READ_IF_NEEDED;
//	  i_ptr++; -- Gildor: wrong for XBox360 decompression
	}

	/* read block type (3 bits) and block length (24 bits) */
	READ_BITS(lzx->block_type, 3);
	READ_BITS(i, 16); READ_BITS(j, 8);
	lzx->block_remaining = lzx->block_length = (i << 8) | j;
	/*D(("new block t%d len %u", lzx->block_type, lzx->block_length))*/

	/* read individual block headers */
	switch (lzx->block_type) {
	case LZX_BLOCKTYPE_ALIGNED:
	  /* read lengths of and build aligned huffman decoding tree */
	  for (i = 0; i < 8; i++) { READ_BITS(j, 3); lzx->ALIGNED_len[i] = j; }
	  BUILD_TABLE(ALIGNED);
	  /* no break -- rest of aligned header is same as verbatim */
	case LZX_BLOCKTYPE_VERBATIM:
	  /* read lengths of and build main huffman decoding tree */
	  READ_LENGTHS(MAINTREE, 0, 256);
	  READ_LENGTHS(MAINTREE, 256, LZX_NUM_CHARS + (lzx->posn_slots << 3));
	  BUILD_TABLE(MAINTREE);
	  /* if the literal 0xE8 is anywhere in the block... */
	  if (lzx->MAINTREE_len[0xE8] != 0) lzx->intel_started = 1;
	  /* read lengths of and build lengths huffman decoding tree */
	  READ_LENGTHS(LENGTH, 0, LZX_NUM_SECONDARY_LENGTHS);
	  BUILD_TABLE_MAYBE_EMPTY(LENGTH);
	  break;

	case LZX_BLOCKTYPE_UNCOMPRESSED:
	  /* because we can't assume otherwise */
	  lzx->intel_started = 1;

	  /* read 1-16 (not 0-15) bits to align to bytes */
	  ENSURE_BITS(16);
	  if (bits_left > 16) i_ptr -= 2;
	  bits_left = 0; bit_buffer = 0;

	  /* read 12 bytes of stored R0 / R1 / R2 values */
	  for (rundest = &buf[0], i = 0; i < 12; i++) {
	    READ_IF_NEEDED;
	    *rundest++ = *i_ptr++;
	  }
	  R0 = buf[0] | (buf[1] << 8) | (buf[2]  << 16) | (buf[3]  << 24);
	  R1 = buf[4] | (buf[5] << 8) | (buf[6]  << 16) | (buf[7]  << 24);
	  R2 = buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24);
	  break;

	default:
	  D(("bad block type"))
	  return lzx->error = MSPACK_ERR_DECRUNCH;
	}
      }

      /* decode more of the block:
       * run = min(what's available, what's needed) */
      this_run = lzx->block_remaining;
      if (this_run > bytes_todo) this_run = bytes_todo;

      /* assume we decode exactly this_run bytes, for now */
      bytes_todo           -= this_run;
      lzx->block_remaining -= this_run;

      /* decode at least this_run bytes */
      switch (lzx->block_type) {
      case LZX_BLOCKTYPE_VERBATIM:
	while (this_run > 0) {
	  READ_HUFFSYM(MAINTREE, main_element);
	  if (main_element < LZX_NUM_CHARS) {
	    /* literal: 0 to LZX_NUM_CHARS-1 */
	    window[window_posn++] = main_element;
	    this_run--;
	  }
	  else {
	    /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
	    main_element -= LZX_NUM_CHARS;

	    /* get match length */
	    match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
	    if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
	      if (lzx->LENGTH_empty) {
                D(("LENGTH symbol needed but tree is empty"))
                return lzx->error = MSPACK_ERR_DECRUNCH;
              }
	      READ_HUFFSYM(LENGTH, length_footer);
	      match_length += length_footer;
	    }
	    match_length += LZX_MIN_MATCH;

	    /* get match offset */
	    switch ((match_offset = (main_element >> 3))) {
	    case 0: match_offset = R0;                                  break;
	    case 1: match_offset = R1; R1=R0;        R0 = match_offset; break;
	    case 2: match_offset = R2; R2=R0;        R0 = match_offset; break;
	    case 3: match_offset = 1;  R2=R1; R1=R0; R0 = match_offset; break;
	    default:
	      extra = extra_bits[match_offset];
	      READ_BITS(verbatim_bits, extra);
	      match_offset = position_base[match_offset] - 2 + verbatim_bits;
	      R2 = R1; R1 = R0; R0 = match_offset;
	    }

	    if ((window_posn + match_length) > lzx->window_size) {
	      D(("match ran over window wrap"))
	      return lzx->error = MSPACK_ERR_DECRUNCH;
	    }

	    /* copy match */
	    rundest = &window[window_posn];
	    i = match_length;
	    /* does match offset wrap the window? */
	    if (match_offset > window_posn) {
	      if (match_offset > lzx->offset) {
		D(("match offset beyond LZX stream"))
		return lzx->error = MSPACK_ERR_DECRUNCH;
	      }
	      /* j = length from match offset to end of window */
	      j = match_offset - window_posn;
	      if (j > (int) lzx->window_size) {
		D(("match offset beyond window boundaries"))
		return lzx->error = MSPACK_ERR_DECRUNCH;
	      }
	      runsrc = &window[lzx->window_size - j];
	      if (j < i) {
		/* if match goes over the window edge, do two copy runs */
		i -= j; while (j-- > 0) *rundest++ = *runsrc++;
		runsrc = window;
	      }
	      while (i-- > 0) *rundest++ = *runsrc++;
	    }
	    else {
	      runsrc = rundest - match_offset;
	      while (i-- > 0) *rundest++ = *runsrc++;
	    }

	    this_run    -= match_length;
	    window_posn += match_length;
	  }
	} /* while (this_run > 0) */
	break;

      case LZX_BLOCKTYPE_ALIGNED:
	while (this_run > 0) {
	  READ_HUFFSYM(MAINTREE, main_element);
	  if (main_element < LZX_NUM_CHARS) {
	    /* literal: 0 to LZX_NUM_CHARS-1 */
	    window[window_posn++] = main_element;
	    this_run--;
	  }
	  else {
	    /* match: LZX_NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
	    main_element -= LZX_NUM_CHARS;

	    /* get match length */
	    match_length = main_element & LZX_NUM_PRIMARY_LENGTHS;
	    if (match_length == LZX_NUM_PRIMARY_LENGTHS) {
              if (lzx->LENGTH_empty) {
                D(("LENGTH symbol needed but tree is empty"))
                return lzx->error = MSPACK_ERR_DECRUNCH;
              }
	      READ_HUFFSYM(LENGTH, length_footer);
	      match_length += length_footer;
	    }
	    match_length += LZX_MIN_MATCH;

	    /* get match offset */
	    switch ((match_offset = (main_element >> 3))) {
	    case 0: match_offset = R0;                             break;
	    case 1: match_offset = R1; R1 = R0; R0 = match_offset; break;
	    case 2: match_offset = R2; R2 = R0; R0 = match_offset; break;
	    default:
	      extra = extra_bits[match_offset];
	      match_offset = position_base[match_offset] - 2;
	      if (extra > 3) {
		/* verbatim and aligned bits */
		extra -= 3;
		READ_BITS(verbatim_bits, extra);
		match_offset += (verbatim_bits << 3);
		READ_HUFFSYM(ALIGNED, aligned_bits);
		match_offset += aligned_bits;
	      }
	      else if (extra == 3) {
		/* aligned bits only */
		READ_HUFFSYM(ALIGNED, aligned_bits);
		match_offset += aligned_bits;
	      }
	      else if (extra > 0) { /* extra==1, extra==2 */
		/* verbatim bits only */
		READ_BITS(verbatim_bits, extra);
		match_offset += verbatim_bits;
	      }
	      else /* extra == 0 */ {
		/* ??? not defined in LZX specification! */
		match_offset = 1;
	      }
	      /* update repeated offset LRU queue */
	      R2 = R1; R1 = R0; R0 = match_offset;
	    }

	    if ((window_posn + match_length) > lzx->window_size) {
	      D(("match ran over window wrap"))
	      return lzx->error = MSPACK_ERR_DECRUNCH;
	    }

	    /* copy match */
	    rundest = &window[window_posn];
	    i = match_length;
	    /* does match offset wrap the window? */
	    if (match_offset > window_posn) {
	      if (match_offset > lzx->offset) {
		D(("match offset beyond LZX stream"))
		return lzx->error = MSPACK_ERR_DECRUNCH;
	      }
	      /* j = length from match offset to end of window */
	      j = match_offset - window_posn;
	      if (j > (int) lzx->window_size) {
		D(("match offset beyond window boundaries"))
		return lzx->error = MSPACK_ERR_DECRUNCH;
	      }
	      runsrc = &window[lzx->window_size - j];
	      if (j < i) {
		/* if match goes over the window edge, do two copy runs */
		i -= j; while (j-- > 0) *rundest++ = *runsrc++;
		runsrc = window;
	      }
	      while (i-- > 0) *rundest++ = *runsrc++;
	    }
	    else {
	      runsrc = rundest - match_offset;
	      while (i-- > 0) *rundest++ = *runsrc++;
	    }

	    this_run    -= match_length;
	    window_posn += match_length;
	  }
	} /* while (this_run > 0) */
	break;

      case LZX_BLOCKTYPE_UNCOMPRESSED:
	/* as this_run is limited not to wrap a frame, this also means it
	 * won't wrap the window (as the window is a multiple of 32k) */
	rundest = &window[window_posn];
	window_posn += this_run;
	while (this_run > 0) {
	  if ((i = i_end - i_ptr) == 0) {
	    READ_IF_NEEDED;
	  }
	  else {
	    if (i > this_run) i = this_run;
	    lzx->sys->copy(i_ptr, rundest, (size_t) i);
	    rundest  += i;
	    i_ptr    += i;
	    this_run -= i;
	  }
	}
	break;

      default:
	return lzx->error = MSPACK_ERR_DECRUNCH; /* might as well */
      }

      /* did the final match overrun our desired this_run length? */
      if (this_run < 0) {
	if ((unsigned int)(-this_run) > lzx->block_remaining) {
	  D(("overrun went past end of block by %d (%d remaining)",
	     -this_run, lzx->block_remaining ))
	  return lzx->error = MSPACK_ERR_DECRUNCH;
	}
	lzx->block_remaining -= -this_run;
      }
    } /* while (bytes_todo > 0) */

    /* streams don't extend over frame boundaries */
    if ((window_posn - lzx->frame_posn) != frame_size) {
      D(("decode beyond output frame limits! %d != %d",
	 window_posn - lzx->frame_posn, frame_size))
      return lzx->error = MSPACK_ERR_DECRUNCH;
    }

    /* re-align input bitstream */
    if (bits_left > 0) ENSURE_BITS(16);
    if (bits_left & 15) REMOVE_BITS(bits_left & 15);

    /* check that we've used all of the previous frame first */
    if (lzx->o_ptr != lzx->o_end) {
      D(("%ld avail bytes, new %d frame", lzx->o_end-lzx->o_ptr, frame_size))
      return lzx->error = MSPACK_ERR_DECRUNCH;
    }

    /* does this intel block _really_ need decoding? */
    if (lzx->intel_started && lzx->intel_filesize &&
	(lzx->frame <= 32768) && (frame_size > 10))
    {
      unsigned char *data    = &lzx->e8_buf[0];
      unsigned char *dataend = &lzx->e8_buf[frame_size - 10];
      signed int curpos      = lzx->intel_curpos;
      signed int filesize    = lzx->intel_filesize;
      signed int abs_off, rel_off;

      /* copy e8 block to the e8 buffer and tweak if needed */
      lzx->o_ptr = data;
      lzx->sys->copy(&lzx->window[lzx->frame_posn], data, frame_size);

      while (data < dataend) {
	if (*data++ != 0xE8) { curpos++; continue; }
	abs_off = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
	if ((abs_off >= -curpos) && (abs_off < filesize)) {
	  rel_off = (abs_off >= 0) ? abs_off - curpos : abs_off + filesize;
	  data[0] = (unsigned char) rel_off;
	  data[1] = (unsigned char) (rel_off >> 8);
	  data[2] = (unsigned char) (rel_off >> 16);
	  data[3] = (unsigned char) (rel_off >> 24);
	}
	data += 4;
	curpos += 5;
      }
      lzx->intel_curpos += frame_size;
    }
    else {
      lzx->o_ptr = &lzx->window[lzx->frame_posn];
      if (lzx->intel_filesize) lzx->intel_curpos += frame_size;
    }
    lzx->o_end = &lzx->o_ptr[frame_size];

    /* write a frame */
    i = (out_bytes < (off_t)frame_size) ? (unsigned int)out_bytes : frame_size;
    if (lzx->sys->write(lzx->output, lzx->o_ptr, i) != i) {
      return lzx->error = MSPACK_ERR_WRITE;
    }
    lzx->o_ptr  += i;
    lzx->offset += i;
    out_bytes   -= i;

    /* advance frame start position */
    lzx->frame_posn += frame_size;
    lzx->frame++;

    /* wrap window / frame position pointers */
    if (window_posn == lzx->window_size)     window_posn = 0;
    if (lzx->frame_posn == lzx->window_size) lzx->frame_posn = 0;

  } /* while (lzx->frame < end_frame) */

  if (out_bytes) {
    D(("bytes left to output"))
    return lzx->error = MSPACK_ERR_DECRUNCH;
  }

  /* store local state */
  STORE_BITS;
  lzx->window_posn = window_posn;
  lzx->R0 = R0;
  lzx->R1 = R1;
  lzx->R2 = R2;

  return MSPACK_ERR_OK;
}

void lzxd_free(struct lzxd_stream *lzx) {
  struct mspack_system *sys;
  if (lzx) {
    sys = lzx->sys;
    sys->free(lzx->inbuf);
    sys->free(lzx->window);
    sys->free(lzx);
  }
}
