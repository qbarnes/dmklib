/*
 * libdmk - library for accessing DMK format disk images
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: libdmk.h,v 1.1 2002/08/06 07:29:59 eric Exp $
 */


#define DMK_MAX_SECTOR 64


typedef enum
{
  DMK_FM,    /* single-density */
  DMK_MFM,   /* IBM double-density */
  DMK_RX02,  /* DEC double-density */
  DMK_M2FM   /* Intel double-density */
} sector_mode_t;

typedef struct
{
  uint8_t cylinder;
  uint8_t head;
  uint8_t sector;
  uint8_t size;
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


dmk_handle dmk_open_image (char *fn);

int dmk_close_image (dmk_handle h);


int dmk_seek (dmk_handle h,
	      int cylinder,
	      int side);


int dmk_format_track (dmk_handle h,
		      sector_mode_t mode,
		      int sector_count,
		      sector_info_t *sector_info);


int dmk_read_sector (dmk_handle h,
		     sector_mode_t mode,
		     int log_cylinder,
		     int log_side,
		     int log_sector,
		     int sector_size,
		     uint8_t *data);

int dmk_write_sector (dmk_handle h,
		      sector_mode_t mode,
		      int log_cylinder,
		      int log_side,
		      int log_sector,
		      int sector_size,
		      uint8_t *data);
