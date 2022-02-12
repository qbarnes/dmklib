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


dmk_handle dmk_create_image (char *fn,
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
 * If rpm and rate are non-zero, they well be used (together with dd)
 * to set the appropriate track length.
 */


dmk_handle dmk_open_image (char *fn,
			   int write_enable,
			   int *ds,
			   int *cylinders,
			   int *dd);

int dmk_close_image (dmk_handle h);


int dmk_seek (dmk_handle h,
	      int cylinder,
	      int side);


int dmk_format_track (dmk_handle h,
		      sector_mode_t mode,
		      int sector_count,
		      sector_info_t *sector_info);


int dmk_read_id_with_crcs (dmk_handle h,
			   sector_info_t *sector_info,
			       uint16_t *actual_crc,
			       uint16_t *computed_crc);

int dmk_read_id (dmk_handle h,
		 sector_info_t *sector_info);

int dmk_read_sector_with_crcs (dmk_handle h,
			       sector_info_t *sector_info,
			       uint8_t *data,
			       uint16_t *actual_crc,
			       uint16_t *computed_crc);

int dmk_read_sector (dmk_handle h,
		     sector_info_t *sector_info,
		     uint8_t *data);

int dmk_write_sector (dmk_handle h,
		      sector_info_t *sector_info,
		      uint8_t *data);

int dmk_sector_size (sector_info_t *si);

#undef ADDRESS_MARK_DEBUG
#ifdef ADDRESS_MARK_DEBUG
int dmk_check_address_mark (dmk_handle h,
			    sector_info_t *sector_info);
#endif /* ADDRESS_MARK_DEBUG */

#ifdef __cplusplus
}
#endif

#endif
