/*
 * rfloppy - read floppy disks
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: dmkformat.c,v 1.2 2002/08/18 08:39:25 eric Exp $
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "libdmk.h"


#define CYLINDER_COUNT 77


int main (int argc, char *argv[])
{
  dmk_handle h;
  int cylinder;
  int i;
  sector_info_t sector_info [26];

  if (argc != 2)
    exit (1);

  h = dmk_create_image (argv [1],
			0,  /* double-sided */
			CYLINDER_COUNT,
			0, /* not double density */
			360, /* RPM */
			250); /* rate */
  if (! h)
    {
      fprintf (stderr, "error opening output file\n");
      exit (2);
    }

  for (cylinder = 0; cylinder < CYLINDER_COUNT; cylinder++)
    {
      if (! dmk_seek (h, cylinder, 0))
	{
	  fprintf (stderr, "error seeking to cylinder %d\n", cylinder);
	  exit (2);
	}

      for (i = 0; i < 26; i++)
	{
	  sector_info [i].cylinder   = cylinder;
	  sector_info [i].head       = 0;
	  sector_info [i].sector     = i + 1;
	  sector_info [i].size_code  = 0;
	  sector_info [i].mode       = DMK_FM;
	  sector_info [i].write_data = 1;
	  sector_info [i].data_value = 0xe5;  /* not used */
	}

      if (! dmk_format_track (h, DMK_FM, 26, sector_info))
	{
	  fprintf (stderr, "error formatting cylinder %d\n", cylinder);
	  exit (2);
	}
    }

#ifdef ADDRESS_MARK_DEBUG
  for (cylinder = 0; cylinder < CYLINDER_COUNT; cylinder++)
    {
      if (! dmk_seek (h, cylinder, 0))
	{
	  fprintf (stderr, "error seeking to cylinder %d\n", cylinder);
	  exit (2);
	}

      for (i = 1; i <= 26; i++)
	{
	  sector_info [0].cylinder   = cylinder;
	  sector_info [0].head       = 0;
	  sector_info [0].sector     = i;
	  sector_info [0].size_code  = 0;
	  sector_info [0].mode       = DMK_FM;
	  sector_info [0].write_data = 1;
	  sector_info [0].data_value = 0xe5;  /* not used */

	  if (! dmk_check_address_mark (h, & sector_info [0]))
	    {
	      fprintf (stderr, "address mark error on %d/%d\n",
		       cylinder, i);
	      exit (2);
	    }
	}

    }
#endif /* ADDRESS_MARK_DEBUG */

  dmk_close_image (h);

  exit (0);
}
