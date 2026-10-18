/*
 * dmk2raw - convert a DMK disk image to a raw sector image
 *
 * $Id: dmk2raw.c,v 1.3 2002/10/19 23:36:21 eric Exp $
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


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libdmk.h"


void print_sector_info (sector_info_t *sector_info)
{
  printf ("cyl %d head %d sector %d size %d\n",
	  sector_info->cylinder,
	  sector_info->head,
	  sector_info->sector,
	  dmk_sector_size (sector_info));
}


int main (int argc, char *argv[])
{
  dmk_handle h;
  FILE *outf;

  int ds, dd;
  int cylinders;

  int cylinder, head, sector;

  int sector_count;
  int min_sector, max_sector;
  sector_info_t sector_info [DMK_MAX_SECTOR];
  int sector_index [256];

  uint8_t buf [4096];  /* largest sector size dmklib returns, 16 * 255 for FM */

  int i;

  if (argc != 3)
    {
      fprintf (stderr, "usage: %s image.dmk image.raw\n", argv [0]);
      exit (1);
    }

  h = dmk_open_image (argv [1], 0, & ds, & cylinders, & dd);
  if (! h)
    {
      fprintf (stderr, "error opening input DMK file\n");
      exit (2);
    }

  outf = fopen (argv [2], "wb");
  if (! outf)
    {
      fprintf (stderr, "error opening output raw file\n");
      exit (2);
    }

  for (cylinder = 0; cylinder < cylinders; cylinder++)
    for (head = 0; head <= ds; head++)
      {
	for (i = 0; i < 256; i++)
	  sector_index [i] = -1;

	if (! dmk_seek (h, cylinder, head))
	  {
	    fprintf (stderr, "error seeking to cylinder %d\n", cylinder);
	    exit (2);
	  }

	if (! dmk_read_id (h, & sector_info [0]))
	  {
	    fprintf (stderr, "error reading sector info on cylinder %d head %d\n", cylinder, head);
	    exit (2);
	  }
	sector_index [sector_info [0].sector] = 0;
	min_sector = sector_info [0].sector;
	max_sector = sector_info [0].sector;
	for (i = 1; i < DMK_MAX_SECTOR; i++)
	  {
	    if (! dmk_read_id (h, & sector_info [i]))
	      break;
	    if (sector_info [i].sector == sector_info [0].sector)
	      break;
	    sector_index [sector_info [i].sector] = i;
	    if (sector_info [i].sector < min_sector)
	      min_sector = sector_info [i].sector;
	    if (sector_info [i].sector > max_sector)
	      max_sector = sector_info [i].sector;
	  }
	sector_count = i;

#if 0
	printf ("sector count %d, from %d to %d\n", sector_count, min_sector, max_sector);
#endif
	if (sector_count != ((max_sector - min_sector) + 1))
	  {
	    fprintf (stderr, "sectors discontigous\n");
	    exit (2);
	  }
	/* a duplicated sector number can hide a gap from the count check */
	for (sector = min_sector; sector <= max_sector; sector++)
	  if (sector_index [sector] < 0)
	    {
	      fprintf (stderr, "sectors discontigous\n");
	      exit (2);
	    }

	for (sector = min_sector; sector <= max_sector; sector++)
	  {
	    int size = dmk_sector_size (& sector_info [sector_index [sector]]);

	    if (! dmk_read_sector (h,
				   & sector_info [sector_index [sector]],
				   buf))
	      {
		printf ("error reading cyl %d head %d sector %d size code %d\n",
			sector_info [sector_index [sector]].cylinder,
			sector_info [sector_index [sector]].head,
			sector_info [sector_index [sector]].sector,
			sector_info [sector_index [sector]].size_code);
		/* keep later sectors at their offsets in the raw file */
		memset (buf, 0xe5, size);
	      }
#if 0
	    printf ("cyl %d head %d sector %d size code %d:\n",
		    sector_info [sector_index [sector]].cylinder,
		    sector_info [sector_index [sector]].head,
		    sector_info [sector_index [sector]].sector,
		    sector_info [sector_index [sector]].size_code);
	    hex_dump (buf, size);
#endif
	    if (1 != fwrite (buf, size, 1, outf))
	      {
		fprintf (stderr, "error writing raw file\n");
		exit (2);
	      }
	  }
      }

  dmk_close_image (h);

  fclose (outf);

  exit (0);
}
