/*
 * libdmk - library for accessing DMK format disk images
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: libdmk.c,v 1.3 2002/08/08 09:00:34 eric Exp $
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "dmk.h"
#include "libdmk.h"


typedef struct
{
  int resident;  /* boolean */
  int dirty;     /* boolean */
  uint8_t  mfm_sector   [DMK_MAX_SECTOR];
  uint16_t idam_pointer [DMK_MAX_SECTOR];
  uint8_t *buf;
} track_state_t;

struct dmk_state
{
  FILE *f;

  int new_image;  /* boolean */
  int writable;   /* boolean */

  /* parameters specified by user */
  int ds;    /* disk is double sided */
  int cylinders;
  int dd;    /* disk is double density */
  int rpm;   /* 300 or 360 RPM */
  int rate;  /* 125, 250, 300, or 500 Kbps */

  /* computed parameters */
  int track_length;  /* length of a track buffer */

  /* track information */
  track_state_t *track;  /* index by 2 * cylinder + head */

  /* current status */
  int cur_cylinder;
  int cur_head;
  sector_mode_t cur_mode;  /* current transfer mode */
  track_state_t *cur_track;

  uint16_t crc;
  int p;  /* index into buf */
};



static void init_crc (dmk_handle h)
{
  h->crc = 0xffff;
}


static void compute_crc (dmk_handle h, uint8_t data)
{
  int i;
  uint16_t d2 = data << 8;
  for (i = 0; i < 8; i++)
    {
      h->crc = (h->crc << 1) ^ ((((h->crc ^ d2) & 0x8000) ? 0x1021 : 0));
      d2 <<= 1;
    }
}


/* should never happen!  sectors aren't allowed to wrap around. */
static void wrap_p (dmk_handle h)
{
#if 0
  h->p = 0;
#else
  fprintf (stderr, "libdmk internal error 1\n");
  exit (2);
#endif
}


static inline void inc_p (dmk_handle h)
{
  h->p++;
  if (h->p >= h->track_length)
    wrap_p (h);
}

static void read_buf (dmk_handle h,
		      int len,
		      uint8_t *data)
{
  uint8_t b;
  while (len--)
    {
      b = h->cur_track->buf [h->p];
      inc_p (h);
      if (h->dd && (h->cur_mode == DMK_FM))
	inc_p (h);
      compute_crc (h, b);
      *(data++) = b;
    }
}


static uint8_t read_buf_byte (dmk_handle h)
{
  uint8_t b;

  read_buf (h, 1, & b);
  return (b);
}


static int check_crc (dmk_handle h)
{
  uint8_t d [2];

  read_buf (h, 2, d);
  /* $$$ byte order? */
  return ((d [0] != (h->crc >> 8)) &&
	  (d [1] != (h->crc & 0xff)));
}


static void write_buf (dmk_handle h,
		       int len,
		       uint8_t *data)
{
  if (! h->writable)
    return;  /* ideally we wouldn't get this far */
  h->cur_track->dirty = 1;
  while (len--)
    {
      compute_crc (h, *data);
      h->cur_track->buf [h->p] = *data;
      inc_p (h);
      if (h->dd && (h->cur_mode == DMK_FM))
	{
	  h->cur_track->buf [h->p] = *data;
	  inc_p (h);
	}
      data++;
    }
}


static void write_buf_const (dmk_handle h,
			     int count,
			     uint8_t val)
{
  while (count--)
    write_buf (h, 1, & val);
}


static void write_crc (dmk_handle h)
{
  uint8_t d [2];
  /* $$$ byte order? */
  d [0] = h->crc >> 8;
  d [1] = h->crc & 0xff;
  write_buf (h, 2, d);
}


dmk_handle dmk_open_image (char *fn,
			   int write_enable,
			   int *ds,
			   int *cylinders,
			   int *dd)
{
  dmk_handle h;

  h = calloc (1, sizeof (struct dmk_state));
  if (! h)
    goto fail;

  h->f = fopen (fn, write_enable ? "r+" : "rb");
  if (! h->f)
    goto fail;

  /* $$$ read DMK header */

  /* $$$ if write requested, make sure the file isn't locked */

  h->writable = write_enable;

  /* change these to use the header */
#if 0
  h->ds = ???;
  h->cylinders = ???;
  h->dd = ???;
  h->track_length = ???:
#endif

  *ds = h->ds;
  *cylinders = h->cylinders;
  *dd = h->dd;

#if 0
  /* could guess, but why bother */
  h->rpm = rpm;
  h->rate = rate;
#endif

  h->track = calloc (h->cylinders * (h->ds + 1), sizeof (track_state_t));
  if (! h->track)
    goto fail;

  /* 
   * Make sure the first seek will do the right thing, by setting
   * the current position to a non-existent track
   */
  h->cur_cylinder = -1;
  h->cur_head = -1;

  return (h);

 fail:
  if (h)
    free (h);
  return (NULL);
}


dmk_handle dmk_create_image (char *fn,
			     int ds,    /* boolean */
			     int cylinders,
			     int dd,    /* boolean */
			     int rpm,   /* 300 or 360 RPM */
			     int rate)  /* 125, 250, 300, or 500 Kbps */
{
  dmk_handle h;

  h = calloc (1, sizeof (struct dmk_state));
  if (! h)
    goto fail;

  h->f = fopen (fn, "wb");
  if (! h->f)
    goto fail;

  h->new_image = 1;
  h->writable = 1;

  h->ds = ds;
  h->cylinders = cylinders;
  h->dd = dd;
  h->rpm = rpm;
  h->rate = rate;

  h->track_length = (rate * 7500L) / rpm;

  h->track = calloc (cylinders * (ds + 1), sizeof (track_state_t));
  if (! h->track)
    goto fail;

  /* 
   * Make sure the first seek will do the right thing, by setting
   * the current position to a non-existent track
   */
  h->cur_cylinder = -1;
  h->cur_head = -1;

  return (h);

 fail:
  if (h)
    free (h);
  return (NULL);
}


int dmk_image_file_seek_track (dmk_handle h, int cylinder, int head)
{
  int pos = DMK_HEADER_LENGTH + (((h->ds + 1) * cylinder + head) *
				 ((2 * DMK_MAX_SECTOR) + h->track_length));
  return (0 <= fseek (h->f, pos, SEEK_SET));
}


int dmk_close_image (dmk_handle h)
{
  int cylinder, head, sector;
  track_state_t *track;
  uint8_t b;

  if (! h->writable)
    goto done;

  if (h->new_image)
    {
      uint8_t dmk_header [DMK_HEADER_LENGTH];

      memset (dmk_header, 0, DMK_HEADER_LENGTH);
      dmk_header [0] = 0x00;  /* unprotected */
      dmk_header [1] = h->cylinders;
      dmk_header [2] = h->track_length & 0xff;
      dmk_header [3] = h->track_length >> 8;
      dmk_header [4] = 0x00;  /* flags */
      if (! h->ds)
	dmk_header [4] |= DMK_FLAG_SS_MASK;
      if (! h->dd)
	dmk_header [4] |= DMK_FLAG_SD_MASK;
    
      /* note that we should still be positioned to the start of the file */
      if (1 != fwrite (dmk_header, sizeof (dmk_header), 1, h->f))
	{
	  fprintf (stderr, "error writing DMK header\n");
	  return (0);
	}
    }

  for (cylinder = 0; cylinder < h->cylinders; cylinder++)
    for (head = 0; head <= h->ds; head++)
      {
	track = & h->track [(h->ds + 1) * cylinder + head];

	if (track->buf && track->dirty)
	  {
	    if (! dmk_image_file_seek_track (h, cylinder, head))
	      {
		fprintf (stderr, "error seeking image file\n");
		return (0);
	      }
	    /* write IDAM offsets */
	    for (sector = 0; sector < DMK_MAX_SECTOR; sector++)
	      {
		int idam_ptr = track->idam_pointer [sector];
		if (track->mfm_sector [sector])
		  idam_ptr |= DMK_IDAM_POINTER_MFM_MASK;
		b = idam_ptr & 0xff;
		if (1 != fwrite (& b, 1, 1, h->f))
		  {
		    fprintf (stderr, "error writing image file\n");
		    return (0);
		  }
		b = idam_ptr >> 8;
		if (1 != fwrite (& b, 1, 1, h->f))
		  {
		    fprintf (stderr, "error writing image file\n");
		    return (0);
		  }
	      }

	    /* write track data */
	    if (1 != fwrite (track->buf, h->track_length, 1, h->f))
	      {
		fprintf (stderr, "error writing image file\n");
		return (0);
	      }
	    track->dirty = 0;
	  }
	if (track->buf)
	  free (track->buf);
      }

 done:
  fclose (h->f);
  free (h);
  return (1);
}


int dmk_seek (dmk_handle h,
	      int cylinder,
	      int head)
{
  track_state_t *new_track;
  int i;

  if (cylinder > h->cylinders)
    return (0);

  if (head && ! h->ds)
    return (0);

  if ((cylinder == h->cur_cylinder) &&
      (head == h->cur_head))
    return (1);  /* already there */

  new_track = & h->track [(h->ds + 1) * cylinder + head];

  if (! new_track->buf)
    {
      new_track->buf = calloc (1, h->track_length);
      if (! new_track->buf)
	return (0);
      if (h->new_image)
	{
	  /* virgin image: fill the new track with FFs */
	  for (i = 0; i < h->track_length; i++)
	    new_track->buf [i] = 0xff;
	}
      else
	{
	  /* existing image: read the track from the image file */
	  if (! dmk_image_file_seek_track (h, cylinder, head))
	    {
	      fprintf (stderr, "error seeking image file\n");
	      exit (2);
	    }
	  for (i = 0; i < DMK_MAX_SECTOR; i++)
	    {
	      uint8_t d [2];
	      uint16_t idam_ptr;
	      if (1 != fread (d, 2, 1, h->f))
		{
		  fprintf (stderr, "error reading image file\n");
		  exit (2);
		}
	      idam_ptr = d [1] << 8 | d [0];
	      if (idam_ptr & DMK_IDAM_POINTER_MFM_MASK)
		{
		  new_track->mfm_sector [i] = 1;
		  idam_ptr &= ~ DMK_IDAM_POINTER_FLAGS_MASK;
		}
	      else
		new_track->mfm_sector [i] = 0;
	      new_track->idam_pointer [i] = idam_ptr;
	    }
	  if (1 != fread (new_track->buf, h->track_length, 1, h->f))
	    {
	      fprintf (stderr, "error reading image file\n");
	      exit (2);
	    }
	}
    }

  h->cur_cylinder = cylinder;
  h->cur_head = head;
  h->cur_track = new_track;

  return (1);
}


static int write_data_field (dmk_handle h,
			     sector_info_t *sector_info,
			     int single_value,  /* boolean */
			     uint8_t *data)
{
  switch (sector_info->mode)
    {
    case DMK_FM:
      write_buf_const   (h,  11, 0xff);  /* $$$ not this many! */
      write_buf_const   (h,   6, 0x00);

      /* data address mark */
      init_crc (h);
      write_buf_const   (h,   1, 0xfb);
      if (single_value)
	write_buf_const (h, 128 << sector_info->size_code, *data);
      else
	write_buf       (h, 128 << sector_info->size_code, data);
      write_crc (h);
      write_buf_const   (h,  27, 0xff);  /* $$$ not this many! */
      break;

    case DMK_MFM:
      write_buf_const   (h,  22, 0x4e);  /* $$$ not this many! */
      write_buf_const   (h,  12, 0x00);

      /* data address mark */
      init_crc (h);
      write_buf_const   (h,   3, 0xa1);  /* missing clock b/w bits 4 and 5 */
      write_buf_const   (h,   1, 0xfb);
      if (single_value)
	write_buf_const (h, 128 << sector_info->size_code, *data);
      else
	write_buf       (h, 128 << sector_info->size_code, data);
      write_crc (h);
      write_buf_const   (h,  54, 0x4e);  /* $$$ not this many! */
      break;

    default:
      printf ("mode not yet supported\n");
      return (0);
    }

  return (1);
}


static int read_data_field (dmk_handle h,
			    sector_info_t *sector_info,
			    uint8_t *data)
{
  /* $$$ is there a data mark with ? bytes? */

  init_crc (h);
  if (sector_info->mode == DMK_MFM)
    {
      /* In MFM, the three A1 bytes are included in the CRC */
      compute_crc (h, 0xa1);
      compute_crc (h, 0xa1);
      compute_crc (h, 0xa1);
    }
  read_buf_byte (h);  /* eat data mark */
  read_buf (h, 128 << sector_info->size_code, data);
  return (check_crc (h));
}


int dmk_format_track (dmk_handle h,
		      sector_mode_t mode,
		      int sector_count,
		      sector_info_t *sector_info)
{
  int sector;

  /* make sure we have a physical position */
  if (h->cur_cylinder < 0)
    return (0);

  /* can't write double-density track to a
     single-density image */
  if (mode & ! h->dd)
    return (0);

  h->cur_mode = mode;

  memset (h->cur_track->idam_pointer, 0, sizeof (h->cur_track->idam_pointer));

  h->p = 0;

  switch (mode)
    {
    case DMK_FM:
    case DMK_RX02:
      write_buf_const (h, 40, 0xff);
      write_buf_const (h,  6, 0x00);

      /* index mark */
      write_buf_const (h,  1, 0xfc);
      write_buf_const (h, 26, 0xff);

      for (sector = 0; sector < sector_count; sector++)
	{
	  write_buf_const (h, 6, 0x00);

	  /* ID address mark */
          h->cur_track->idam_pointer [sector] = h->p;
	  h->cur_track->mfm_sector [sector] = 0;
	  init_crc (h);
	  printf ("initial crc: %04x\n", h->crc);
	  write_buf_const (h, 1, 0xfe);
	  printf ("after AM: %04x\n", h->crc);
	  write_buf_const (h, 1, sector_info [sector].cylinder);
	  printf ("after cyl %02x: %04x\n", sector_info [sector].cylinder, h->crc);
	  write_buf_const (h, 1, sector_info [sector].head);
	  printf ("after head %02x: %04x\n", sector_info [sector].head, h->crc);
	  write_buf_const (h, 1, sector_info [sector].sector);
	  printf ("after sector %02x: %04x\n", sector_info [sector].sector, h->crc);
	  write_buf_const (h, 1, sector_info [sector].size_code);
	  printf ("after size code %02x: %04x\n", sector_info [sector].size_code, h->crc);
	  write_crc (h);

	  if (sector_info [sector].write_data)
	    {
	      write_buf_const (h,  11, 0xff);
	      write_buf_const (h,   6, 0x00);

	      /* data address mark */
	      init_crc (h);
	      write_buf_const (h,   1, 0xfb);
	      write_buf_const (h, 128 << sector_info [sector].size_code,
			       sector_info [sector].data_value);
	      write_crc (h);
	      write_buf_const (h,  27, 0xff);
	    }
	  else
	    write_buf_const (h, 47 + (128 << sector_info [sector].size_code), 0xff);
	}

      /* fill rest of track (gap 5) */
      write_buf_const (h, h->track_length - h->p, 0xff);

      break;

    case DMK_MFM:
      write_buf_const (h, 80, 0x4e);
      write_buf_const (h, 12, 0x00);
      write_buf_const (h,  3, 0xc2);  /* missing clock between bits 3 & 4 */

      /* index mark */
      write_buf_const (h,  1, 0xfc);
      write_buf_const (h, 50, 0x4e);

      for (sector = 0; sector < sector_count; sector++)
	{
	  write_buf_const (h, 12, 0x00);

	  /* ID address mark */
	  init_crc (h);
	  write_buf_const (h,  3, 0xa1);  /* missing clock between bits 4 and 5 */
          h->cur_track->idam_pointer [sector] = h->p;
	  h->cur_track->mfm_sector [sector] = 1;
	  write_buf_const (h, 1, 0xfe);
	  write_buf_const (h, 1, sector_info [sector].cylinder);
	  write_buf_const (h, 1, sector_info [sector].head);
	  write_buf_const (h, 1, sector_info [sector].sector);
	  write_buf_const (h, 1, sector_info [sector].size_code);
	  write_crc (h);

	  if (sector_info [sector].write_data)
	    {
	      write_buf_const (h, 22, 0x4e);
	      write_buf_const (h, 12, 0x00);

	      /* data address mark */
	      init_crc (h);
	      write_buf_const (h,  3, 0xa1);  /* missing clock between bits 4 and 5 */
	      write_buf_const (h,  1, 0xfb);
	      write_buf_const (h, 128 << sector_info [sector].size_code,
			       sector_info [sector].data_value);
	      write_crc (h);
	      write_buf_const (h,  54, 0x4e);
	    }
	  else
	    write_buf_const (h, 94 + (128 << sector_info [sector].size_code), 0x4e);
	}

      /* fill rest of track (gap 5) */
      write_buf_const (h, h->track_length - h->p, 0x4e);

      break;

    default:
      printf ("mode not yet supported\n");
      return (0);
    }

  return (1);
}


static int find_address_mark (dmk_handle h,
			      sector_info_t *req_sector)
{
  int i;
  uint8_t *buf;
  sector_info_t sector_info;

  /* make sure we have a physical position */
  if (h->cur_cylinder < 0)
    return (0);

  buf = h->cur_track->buf;

  for (i = 0; i < DMK_MAX_SECTOR; i++)
    {
      h->p = h->cur_track->idam_pointer [i];
      if (h->p == 0)
	continue;

      /* is there room in the track for a complete address mark? */
      if ((h->p + 7) > h->track_length)
	continue;


      init_crc (h);
      /* for MFM, CRC includes the three A1 bytes */
      if (req_sector->mode == DMK_MFM)
	{
	  compute_crc (h, 0xa1);
	  compute_crc (h, 0xa1);
	  compute_crc (h, 0xa1);
	}
      /* is it actually an address mark? */
      if (read_buf_byte (h) != 0xfb)
	continue;
      sector_info.cylinder  = read_buf_byte (h);
      sector_info.head      = read_buf_byte (h);
      sector_info.sector    = read_buf_byte (h);
      sector_info.size_code = read_buf_byte (h);
      if (! check_crc (h))
	continue;

      if ((req_sector->cylinder  != sector_info.cylinder) ||
	  (req_sector->head      != sector_info.head) ||
	  (req_sector->sector    != sector_info.sector) ||
	  (req_sector->size_code != sector_info.size_code))
	continue;

      return (1);
    }

  return (0);
}


int dmk_read_id (dmk_handle h,
		 sector_info_t *sector_info)
{
  /* $$$ */
  return (0);
}


int dmk_read_sector (dmk_handle h,
		     sector_info_t *sector_info,
		     uint8_t *data)
{
  /* find address mark */
  if (! find_address_mark (h, sector_info))
    return (0);

  return (read_data_field (h, sector_info, data));
}


int dmk_write_sector (dmk_handle h,
		      sector_info_t *sector_info,
		      uint8_t *data)
{
  /* find address mark */
  if (! find_address_mark (h, sector_info))
    return (0);

  /* $$$ skip ahead a few bytes */
  
  return (write_data_field (h, sector_info, 0, data));
}
