/* exif-mnote-data-canon.c
 *
 * Copyright � 2002, 2003 Lutz M�ller <lutz@users.sourceforge.net>
 * Copyright � 2003 Matthieu Castet <mat-c@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "exif-mnote-data-canon.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libexif/exif-byte-order.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>

#define DEBUG

static void
exif_mnote_data_canon_clear (ExifMnoteDataCanon *n)
{
	unsigned int i;

	if (!n) return;

	if (n->entries) {
		for (i = 0; i < n->count; i++)
			if (n->entries[i].data) {
				free (n->entries[i].data);
				n->entries[i].data = NULL;
			}
		free (n->entries);
		n->entries = NULL;
		n->count = 0;
	}
}

static void
exif_mnote_data_canon_free (ExifMnoteData *n)
{
	if (!n) return;

	exif_mnote_data_canon_clear ((ExifMnoteDataCanon *) n);
}

static char *
exif_mnote_data_canon_get_value (ExifMnoteData *note, unsigned int n)
{
        ExifMnoteDataCanon *cnote = (ExifMnoteDataCanon *) note;

	if (!note) return NULL;
	if (cnote->count >= n) return NULL;
	return mnote_canon_entry_get_value (&cnote->entries[n]);
}

static void
exif_mnote_data_canon_set_byte_order (ExifMnoteData *d, ExifByteOrder o)
{
	ExifByteOrder o_orig;
	ExifMnoteDataCanon *n = (ExifMnoteDataCanon *) d;
	unsigned int i, fs;
	ExifShort s;
	ExifLong l;
	ExifSLong sl;
	ExifRational r;
	ExifSRational sr;

	if (!n) return;

	o_orig = n->order;
	n->order = o;
	for (i = 0; i < n->count; i++) {
		n->entries[i].order = o;
		fs = exif_format_get_size (n->entries[i].format);
		switch (n->entries[i].format) {
		case EXIF_FORMAT_SHORT:
			for (i = 0; i < n->entries[i].components; i++) {
				s = exif_get_short (n->entries[i].data + (i*fs),
						    o_orig);
				exif_set_short (n->entries[i].data + (i * fs),
						o, s);
			}
			break;
		case EXIF_FORMAT_LONG:
			for (i = 0; i < n->entries[i].components; i++) {
				l = exif_get_long (n->entries[i].data + (i*fs),
						   o_orig);
				exif_set_long (n->entries[i].data + (i * fs),
					       o, l);
			}
			break;
		case EXIF_FORMAT_RATIONAL:
			for (i = 0; i < n->entries[i].components; i++) {
				r = exif_get_rational (n->entries[i].data +
						       (i * fs), o_orig);
				exif_set_rational (n->entries[i].data +
					(i * fs), o, r);
			}
			break;
		case EXIF_FORMAT_SLONG:
			for (i = 0; i < n->entries[i].components; i++) {
				sl = exif_get_slong (n->entries[i].data +
						(i * fs), o_orig);
				exif_set_slong (n->entries[i].data +
						(i * fs), o, sl);
			}
			break;
		case EXIF_FORMAT_SRATIONAL:
			for (i = 0; i < n->entries[i].components; i++) {
				sr = exif_get_srational (n->entries[i].data +
						(i * fs), o_orig);
				exif_set_srational (n->entries[i].data +
						(i * fs), o, sr);
			}
			break;
		case EXIF_FORMAT_UNDEFINED:
		case EXIF_FORMAT_BYTE:
		case EXIF_FORMAT_ASCII:
		default:
			/* Nothing here. */
			break;
		}
	}
}

static void
exif_mnote_data_canon_set_offset (ExifMnoteData *n, unsigned int o)
{
	if (n) ((ExifMnoteDataCanon *) n)->offset = o;
}

static void
exif_mnote_data_canon_load (ExifMnoteData *ne,
	const unsigned char *buf, unsigned int buf_size)
{
	ExifMnoteDataCanon *n = (ExifMnoteDataCanon *) ne;
	ExifShort c;
	unsigned int i, o, s;

	if (!n || !buf || !buf_size || (buf_size < 6 + n->offset + 2)) return;

	/* Read the number of entries and remove old ones. */
	c = exif_get_short (buf + 6 + n->offset, n->order);
	exif_mnote_data_canon_clear (n);

	/* Parse the entries */
	for (i = 0; i < c; i++) {
	    o = 6 + 2 + n->offset + 12 * i;
	    if (o + 8 > buf_size) return;

	    n->count = i + 1;
	    n->entries = realloc (n->entries, sizeof (MnoteCanonEntry) * (i+1));
	    memset (&n->entries[i], 0, sizeof (MnoteCanonEntry));
	    n->entries[i].tag        = exif_get_short (buf + o, n->order);
	    n->entries[i].format     = exif_get_short (buf + o + 2, n->order);
	    n->entries[i].components = exif_get_long (buf + o + 4, n->order);
	    n->entries[i].order      = n->order;

	    /*
	     * Size? If bigger than 4 bytes, the actual data is not
	     * in the entry but somewhere else (offset).
	     */
	    s = exif_format_get_size (n->entries[i].format) *
		    		      n->entries[i].components;
	    if (!s) return;
	    o += 8;
	    if (s > 4) o = exif_get_long (buf + o, n->order) + 6;
	    if (o + s > buf_size) return;
	    
	    /* Sanity check */
	    n->entries[i].data = malloc (sizeof (char) * s);
	    if (!n->entries[i].data) return;
	    n->entries[i].size = s;
	    memcpy (n->entries[i].data, buf + o, s);
	}

#ifdef DEBUG
	printf ("Loaded %i entries.\n", n->count);
#endif
}

static unsigned int
exif_mnote_data_canon_count (ExifMnoteData *n)
{
	return n ? ((ExifMnoteDataCanon *) n)->count : 0;
}

static const char *
exif_mnote_data_canon_get_name (ExifMnoteData *note, unsigned int i)
{
	ExifMnoteDataCanon *cnote = (ExifMnoteDataCanon *) note;

	if (!note) return NULL;
	if (i >= cnote->count) return NULL;
	return mnote_canon_tag_get_name (cnote->entries[i].tag);
}

static const char *
exif_mnote_data_canon_get_title (ExifMnoteData *note, unsigned int i)
{
	ExifMnoteDataCanon *cnote = (ExifMnoteDataCanon *) note;

	if (!note) return NULL;
	if (i >= cnote->count) return NULL;
	return mnote_canon_tag_get_title (cnote->entries[i].tag);
}

static const char *
exif_mnote_data_canon_get_description (ExifMnoteData *note, unsigned int i)
{
	ExifMnoteDataCanon *cnote = (ExifMnoteDataCanon *) note;
	if (!note) return NULL;
	if (i >= cnote->count) return NULL;
	return mnote_canon_tag_get_description (cnote->entries[i].tag);
}

ExifMnoteData *
exif_mnote_data_canon_new (void)
{
	ExifMnoteData *d;

	d = malloc (sizeof (ExifMnoteDataCanon));
	if (!d) return NULL;
	memset (d, 0, sizeof (ExifMnoteDataCanon));

	exif_mnote_data_construct (d);

	/* Set up function pointers */
	d->methods.free            = exif_mnote_data_canon_free;
	d->methods.set_byte_order  = exif_mnote_data_canon_set_byte_order;
	d->methods.set_offset      = exif_mnote_data_canon_set_offset;
	d->methods.load            = exif_mnote_data_canon_load;
	d->methods.count           = exif_mnote_data_canon_count;
	d->methods.get_name        = exif_mnote_data_canon_get_name;
	d->methods.get_title       = exif_mnote_data_canon_get_title;
	d->methods.get_description = exif_mnote_data_canon_get_description;
	d->methods.get_value       = exif_mnote_data_canon_get_value;

	return d;
}