/*
 * rfloppy - read floppy disks
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: dmk2raw.c,v 1.1 2002/08/08 09:01:22 eric Exp $
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "libdmk.h"


void print_sector_info (sector_info_t *sector_info)
{
  printf ("cyl %d head %d sector %d size %d\n",
	  sector_info->cylinder,
	  sector_info->head,
	  sector_info->sector,
	  128 << sector_info->size_code);
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

  uint8_t buf [1024];

  int i;

  if (argc != 3)
    exit (1);

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

	if (! dmk_seek (h, cylinder, 0))
	  {
	    fprintf (stderr, "error seeking to cylinder %d\n", cylinder);
	    exit (2);
	  }

	if (! dmk_read_id (h, & sector_info [0]))
	  {
	    fprintf (stderr, "error reading sector info on cylinder %d\n", cylinder);
	    exit (2);
	  }
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

	printf ("sector count %d, from %d to %d\n", sector_count, min_sector, max_sector);
	if (sector_count != ((max_sector - min_sector) + 1))
	  {
	    fprintf (stderr, "sectors discontigous\n");
	    exit (2);
	  }

	for (sector = min_sector; sector <= max_sector; sector++)
	  {
	    if (dmk_read_sector (h,
				 & sector_info [sector_index [sector]],
				 buf))
	      {
		printf ("cyl %d head %d sector %d size code %d:\n",
			sector_info [sector_index [sector]].cylinder,
			sector_info [sector_index [sector]].head,
			sector_info [sector_index [sector]].sector,
			sector_info [sector_index [sector]].size_code);
		/* hex_dump (buf, 128 << sector_info [sector_index [sector]].size_code); */
		if (1 != fwrite (buf, 128 << sector_info [sector_index [sector]].size_code, 1, outf))
		  {
		    fprintf (stderr, "error writing raw file\n");
		    exit (2);
		  }
	      }
	    else
	      printf ("error reading cyl %d head %d sector %d size code %c\n",
		      sector_info [sector_index [sector]].cylinder,
		      sector_info [sector_index [sector]].head,
		      sector_info [sector_index [sector]].sector,
		      sector_info [sector_index [sector]].size_code);
	  }

	if (! dmk_format_track (h, DMK_FM, 26, sector_info))
	  {
	    fprintf (stderr, "error formatting cylinder %d\n", cylinder);
	    exit (2);
	  }
      }

  dmk_close_image (h);

  fclose (outf);

  exit (0);
}
