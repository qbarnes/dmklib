/*
 * libdmk - library for accessing DMK format disk images
 *
 * $Id: libdmk.h,v 1.6 2002/10/19 23:36:09 eric Exp $
 *
 * Copyright 2002 Eric Smith.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  Note that permission is
 * not granted to redistribute this program under the terms of any
 * other version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111  USA
 */

#ifndef DMKLIB_LIBDMK_H
#define DMKLIB_LIBDMK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DMK_MAX_SECTOR 64


typedef enum
{
  DMK_FM,    /* single-density */
  DMK_MFM,   /* IBM double-density */
  DMK_RX02,  /* DEC double-density */
  DMK_M2FM,  /* Intel double-density */
  MAX_SECTOR_MODE  /* must be last */
} sector_mode_t;

typedef struct
{
  uint8_t cylinder;
  uint8_t head;
  uint8_t sector;
  uint8_t size_code;
  sector_mode_t mode;
  int write_data;  /* if false, formatting writes
		      only index and address
		      fields */
  uint8_t data_value;  /* initial data value when
		     formatting, normally 0xe5 */
} sector_info_t;


typedef struct dmk_state *dmk_handle;


/*
 * The 16-byte header at the start of a DMK file.  On disk the
 * multi-byte fields are little-endian, so use dmk_header_decode () and
 * dmk_header_encode () rather than reading or writing the structure
 * directly.  Both conversions are lossless.
 */
typedef struct
{
  uint8_t  write_protect;  /* byte 0, 0xff protected, 0x00 not */
  uint8_t  track_count;    /* byte 1, cylinders */
  uint16_t track_length;   /* bytes 2-3, including the 128-byte IDAM
			      pointer table */
  uint8_t  flags;          /* byte 4, DMK_FLAG_*_MASK in dmk.h */
  uint8_t  quirks;         /* byte 5, DMK_QUIRK_* in dmk.h */
  uint8_t  reserved [6];   /* bytes 6-11 */
  uint32_t real_format;    /* bytes 12-15, DMK_REAL_DISK for a
			      real-disk pointer file */
} dmk_header_t;

/* raw points to the 16 header bytes */
void dmk_header_decode (const uint8_t *raw, dmk_header_t *hdr);
void dmk_header_encode (const dmk_header_t *hdr, uint8_t *raw);

/* For an image from dmk_create_image (), the header it will write. */
void dmk_get_header (dmk_handle h, dmk_header_t *hdr);


/*
 * The library prints diagnostics to stderr by default.  Pass another
 * stream to redirect them, or NULL to suppress them.  The setting is
 * global to the library.
 */
void dmk_set_diagnostic_stream (FILE *f);


dmk_handle dmk_create_image (const char *fn,
			     int ds,    /* boolean */
			     int cylinders,
			     int dd,    /* boolean */
			     int rpm,   /* 300 or 360 RPM */
			     int rate); /* 125, 250, 300, or 500 Kbps */

/*
 * Set ds true for double-sided disks.
 *
 * Set dd true for double-density disks, or for
 * single density if some sectors may be double
 * density (e.g., DEC RX02 format).
 *
 * rpm and rate are used (together with dd) to set the appropriate
 * track length, and must be non-zero.
 *
 * Returns NULL with errno set to EINVAL if cylinders isn't 1..255, or
 * if rpm or rate isn't positive or yields an unrepresentable track
 * length.  The arguments are checked before fn is created or truncated.
 */


#define DMK_CREATE_EXCL 0x01  /* fail with EEXIST if fn exists */

/*
 * Create an image whose header is exactly hdr, including the write
 * protect, quirks, reserved, and real_format bytes, for copying an
 * image.  flags is 0 or DMK_CREATE_EXCL.
 *
 * Returns NULL with errno set to EINVAL if flags is unknown,
 * track_count is 0, track_length leaves no room after the IDAM pointer
 * table, or real_format is DMK_REAL_DISK, which are checked before fn
 * is created or truncated.  Nothing is printed.
 */
dmk_handle dmk_create_image_from_header (const char *fn,
					 const dmk_header_t *hdr,
					 int flags);


dmk_handle dmk_open_image (const char *fn,
			   int write_enable,
			   int *ds,
			   int *cylinders,
			   int *dd);

/*
 * Returns NULL with errno set to EINVAL if the header has no tracks, a
 * track length that leaves no room after the IDAM pointer table, or
 * real_format set to DMK_REAL_DISK (a pointer to a real drive, not an
 * image).  Opening for write also fails if the image is write
 * protected.  Use dmk_get_header () for the header fields not returned
 * here.
 */

int dmk_close_image (dmk_handle h);


int dmk_seek (dmk_handle h,
	      int cylinder,
	      int side);


/*
 * Read or write one track exactly as stored in the file: the 128-byte
 * IDAM pointer table followed by the track data.  len must equal the
 * header's track_length.  Nothing is decoded, checked, or rebuilt, so a
 * copy made this way is byte-for-byte identical.  Reading an unwritten
 * track of a new image gives a blank track (zero pointer table, FF
 * data), which is what dmk_close_image () writes for it.
 *
 * Returns 1 on success, or 0 with errno set: EINVAL for a bad cylinder,
 * side, or length, EBADF to write an image opened read-only, EIO if the
 * track is past the end of a truncated file, or ENOMEM.  Nothing is
 * printed.
 */
int dmk_read_track_raw (dmk_handle h,
			int cylinder,
			int side,
			uint8_t *buf,
			size_t len);

int dmk_write_track_raw (dmk_handle h,
			 int cylinder,
			 int side,
			 const uint8_t *buf,
			 size_t len);


int dmk_format_track (dmk_handle h,
		      sector_mode_t mode,
		      int sector_count,
		      const sector_info_t *sector_info);

/* dmk_format_track () and dmk_write_sector () don't support DMK_RX02
   yet, and return 0 for it.  They also return 0 on an image whose
   quirks byte has DMK_QUIRK_EXTRA_DATA. */


int dmk_read_id_with_crcs (dmk_handle h,
			   sector_info_t *sector_info,
			       uint16_t *actual_crc,
			       uint16_t *computed_crc);

/* Returns 1 for a good ID, 0 at the end of the IDs or for an ID with
   a CRC error.  Use dmk_read_id_with_crcs() to read past bad IDs. */
int dmk_read_id (dmk_handle h,
		 sector_info_t *sector_info);

int dmk_read_sector_with_crcs (dmk_handle h,
			       sector_info_t *sector_info,
			       uint8_t *data,
			       uint16_t *actual_crc,
			       uint16_t *computed_crc);

/* Returns 1 for a good read, 0 if the sector is missing or has a CRC
   error.  Use dmk_read_sector_with_crcs() to tell the two apart. */
int dmk_read_sector (dmk_handle h,
		     sector_info_t *sector_info,
		     uint8_t *data);

int dmk_write_sector (dmk_handle h,
		      const sector_info_t *sector_info,
		      const uint8_t *data);

int dmk_sector_size (const sector_info_t *si);

#undef ADDRESS_MARK_DEBUG
#ifdef ADDRESS_MARK_DEBUG
int dmk_check_address_mark (dmk_handle h,
			    sector_info_t *sector_info);
#endif /* ADDRESS_MARK_DEBUG */

#ifdef __cplusplus
}
#endif

#endif
