/*
 * libdmk - library for accessing DMK format disk images
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: libdmk.c,v 1.1 2002/08/06 07:29:59 eric Exp $
 */


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "dmk.h"
#include "libdmk.h"


typedef struct
{
  int dirty;  /* boolean */
  uint16_t idam_pointer [DMK_MAX_SECTOR];
  uint8_t *buf;
} track_state_t;

struct dmk_state
{
  FILE *f;

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
      h->crc = (h->crc << 1) | (((h->crc ^ d2) & 0x8000) ? 0x1021 : 0);
      d2 <<= 1;
    }
}


static void write_buf (dmk_handle h,
		       int len,
		       uint8_t *data)
{
  while (len--)
    {
      compute_crc (h, *data);
      h->cur_track->buf [(h->p)++] = *data;
      if (h->dd && (h->cur_mode == DMK_FM))
	h->cur_track->buf [(h->p)++] = *data;
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

 fail:
  if (h)
    free (h);
  return (NULL);
}


int dmk_close_image (dmk_handle h)
{
  int cylinder, head, sector;
  track_state_t *track;
  uint8_t b;

  /* $$$ write DMK header */

  for (cylinder = 0; cylinder < h->cylinders; cylinder++)
    for (head = 0; head <= h->ds; head++)
      {
	track = & h->track [2 * cylinder + head];

	/* write IDAM offsets */
	for (sector = 0; sector < DMK_MAX_SECTOR; sector++)
	  {
	    b = track->idam_pointer [sector] & 0xff;
	    if (1 != fwrite (& b, 1, 1, h->f))
	      return (0);
	    b = track->idam_pointer [sector] >> 8;
	    if (1 != fwrite (& b, 1, 1, h->f))
	      return (0);
	  }

	/* write track data */
	if (1 != fwrite (track->buf, h->track_length, 1, h->f))
	  return (0);
      }

  fclose (h->f);
  free (h);
  return (1);
}


int dmk_seek (dmk_handle h,
	      int cylinder,
	      int head)
{
  track_state_t *new_track;

  if (cylinder > h->cylinders)
    return (0);

  if (head && ! h->ds)
    return (0);

  if ((cylinder == h->cur_cylinder) &&
      (head == h->cur_head))
    return (1);  /* already there */

  new_track = & h->track [2 * cylinder + head];

  if (! new_track->buf)
    {
      new_track->buf = calloc (1, h->track_length);
      if (! new_track->buf)
	return (0);
      /* $$$ virgin image: fill the new track with FFs */
      /* $$$ existing image: read the track from the image file */
    }

  h->cur_cylinder = cylinder;
  h->cur_head = head;
  h->cur_track = new_track;

  return (1);
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
	  init_crc (h);
	  write_buf_const (h, 1, 0xfe);
	  write_buf_const (h, 1, sector_info [sector].cylinder);
	  write_buf_const (h, 1, sector_info [sector].head);
	  write_buf_const (h, 1, sector_info [sector].sector);
	  write_buf_const (h, 1, sector_info [sector].size);
	  write_crc (h);
	  write_buf_const (h, 11, 0xff);
	  write_buf_const (h, 6, 0x00);

	  /* data address mark */
	  init_crc (h);
	  write_buf_const (h, 1, 0xfb);
	  write_buf_const (h, 128 << sector_info [sector].size, 0xe5);
	  write_crc (h);
	  write_buf_const (h, 27, 0xff);
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
          h->cur_track->idam_pointer [sector] = h->p | DMK_IDAM_POINTER_MFM_MASK;
	  write_buf_const (h, 1, 0xfe);
	  write_buf_const (h, 1, sector_info [sector].cylinder);
	  write_buf_const (h, 1, sector_info [sector].head);
	  write_buf_const (h, 1, sector_info [sector].sector);
	  write_buf_const (h, 1, sector_info [sector].size);
	  write_crc (h);
	  write_buf_const (h, 22, 0x4e);
	  write_buf_const (h, 12, 0x00);

	  /* data address mark */
	  init_crc (h);
	  write_buf_const (h,  3, 0xa1);  /* missing clock between bits 4 and 5 */
	  write_buf_const (h, 1, 0xfb);
	  write_buf_const (h, 128 << sector_info [sector].size, 0xe5);
	  write_crc (h);
	  write_buf_const (h, 54, 0x4e);
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
