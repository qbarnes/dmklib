/*
 * rfloppy - read floppy disks
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: rfloppy.c,v 1.9 2002/08/08 02:59:18 eric Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fd.h>
#include <linux/fdreg.h>


#include "libdmk.h"


int verbose = 0;


typedef int bool;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;



/* rate codes are unfortunately NOT defined in fdreg.h */
#define FD_RATE_250_KBPS 2
#define FD_RATE_300_KBPS 1
#define FD_RATE_500_KBPS 0

/* 8272 command codes which are NOT defined in fdreg.h */
#define FD_READ_TRACK 0x42



typedef struct
{
  int density;
  int size_code;
  int min_sector;
  int max_sector;
} track_info_t;


typedef struct
{
  int floppy_dev;
  FILE *image_f;
  int data_rate;
  int cylinder_count;
  int head_count;
  int double_step;
  int max_retry;
  track_info_t track_info [4];
} disk_info_t;


void print_track_info (track_info_t *track_info)
{
  printf ("%s density, %d byte sectors numbered from %d to %d\n",
	  track_info->density ? "single" : "double",
	  128 << track_info->size_code,
	  track_info->min_sector, track_info->max_sector);
}


bool reset_drive (disk_info_t *disk_info)
{
  int reset_now = FD_RESET_IF_NEEDED; /* FD_RESET_ALWAYS */
  if (0 > ioctl (disk_info->floppy_dev, FDRESET, & reset_now))
    {
      fprintf (stderr, "can't reset drive\n");
      return (0);
    }
  if (verbose >= 2)
    fprintf (stderr, "floppy reset\n");
  return (1);
}


bool recalibrate (disk_info_t *disk_info)
{
  struct floppy_raw_cmd cmd;
  int i = 0;

  cmd.data = NULL;
  cmd.length = 0;
  cmd.rate = disk_info->data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd [i++] = FD_RECALIBRATE;
  cmd.cmd [i++] = 0;
  cmd.cmd_count = i;
  return (0 <= ioctl (disk_info->floppy_dev, FDRAWCMD, & cmd));
}


bool seek (disk_info_t *disk_info, int cylinder)
{
  struct floppy_raw_cmd cmd;
  int i = 0;

  cmd.data = NULL;
  cmd.length = 0;
  cmd.rate = disk_info->data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd[i++] = FD_SEEK;
  cmd.cmd[i++] = 0;
  cmd.cmd[i++] = cylinder << disk_info->double_step;
  cmd.cmd_count = i;

  return (0 <= ioctl (disk_info->floppy_dev, FDRAWCMD, & cmd));
}


bool fd_read_track (int floppy_dev, int cylinder, int head,
		    int fm, int data_rate, int size_code, u8 *buf)
{
  struct floppy_raw_cmd cmd;
  int i = 0;
  u8 mask = 0x5f;
  int sector_size = 128 << size_code;

  if (fm)
    mask &= ~0x40;

  cmd.data = buf;
  cmd.length = 128 << size_code;
  cmd.rate = data_rate;
  cmd.flags = FD_RAW_INTR | FD_RAW_READ;
  cmd.cmd[i++] = FD_READ_TRACK & mask;
  cmd.cmd[i++] = head ? 4 : 0;
  cmd.cmd[i++] = cylinder; /* Cylinder value (to check with header) */
  cmd.cmd[i++] = head ? 1 : 0; /* Head value (to check with header) */
  cmd.cmd[i++] = 0;
  cmd.cmd[i++] = size_code; /* 256 byte MFM sectors */
  cmd.cmd[i++] = 0xff; /* last sector number on a track */
  cmd.cmd[i++] = 14; /* gap length */
  cmd.cmd[i++] = (sector_size < 255) ? sector_size : 0xff;
  cmd.cmd_count=i;
  return ((0 <= ioctl (floppy_dev, FDRAWCMD, & cmd)) &&
	  ! (cmd.reply[0] & 0xC0));
}


typedef struct
{
  int cylinder;
  int head;
  int sector;
  int size_code;
} id_info_t;


int read_id (disk_info_t *disk_info, int fm, int seek_head, id_info_t *id_info)
{
  struct floppy_raw_cmd cmd;
  u8 mask = 0x5f;
  int i = 0;

  if (fm)
    mask &= ~0x40;

  cmd.data = NULL;   /* No data to transfer */
  cmd.length = 0;
  cmd.rate = disk_info->data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd[i++] = FD_READID & mask;
  cmd.cmd[i++] = seek_head ? 4 : 0;
  cmd.cmd_count = i;

  if (0 > ioctl (disk_info->floppy_dev, FDRAWCMD, & cmd))
    {
      if (verbose >= 2)
	{
	  perror ("ioctl FDRAWCMD in read_id");
	  fprintf (stderr, "error issuing FDRAWCMD ioctl\n");
	}
      reset_drive (disk_info);
      return (0);
    }
  
  if (cmd.reply [0] & 0xc0)
    {
      if (verbose >= 2)
	for (i = 0; i < 3; i++)
	  fprintf (stderr, "read ID status %d: %02x\n", i, cmd.reply [i]);
      return (0);
    }

  id_info->cylinder  = cmd.reply [3];
  id_info->head      = cmd.reply [4];
  id_info->sector    = cmd.reply [5];
  id_info->size_code = cmd.reply [6];

  return (1);
}


bool check_interleave_ids (track_info_t *track_info, id_info_t *id_info)
{
  int i;
  int last_pos;
  int count = 0;
  u8 found [256];

  memset (found, 0, sizeof (found));

  last_pos = track_info->max_sector - track_info->min_sector;

  for (i = 0; i <= last_pos; i++)
    {
      if (found [id_info [i].sector])
	{
	  /* already seen this one! */
	}
      else
	{
	  found [id_info [i].sector] = 1;
	  count ++;
	}
    }
  return (count == ((track_info->max_sector - track_info->min_sector) + 1));
}
				   

void print_interleave (track_info_t *track_info, id_info_t *id_info)
{
  int i;
  printf ("sector order:");
  for (i = 0; i <= track_info->max_sector - track_info->min_sector; i++)
    printf (" %d", id_info [i].sector);
  printf ("\n");
}


bool check_interleave (track_info_t *track_info,
		       id_info_t *id_info, 
		       int id_count)
{
  int i;
  int status;

  for (i = 0;
       i < id_count - ((track_info->max_sector - track_info->min_sector) + 1);
       i++)
    if (id_info [i].sector == track_info->min_sector)
      {
	status = check_interleave_ids (track_info, & id_info [i]);
	if (status)
	  {
	    print_interleave (track_info, & id_info [i]);
	    return (1);
	  }
      }
  return (0);
}


/* find the minimum and maximum sector numbers */
void find_min_max_sector (track_info_t *track_info,
			  id_info_t *id_info,
			  int id_count)
{
  int i;

  track_info->min_sector = id_info [0].sector;
  track_info->max_sector = id_info [0].sector;

  for (i = 1; i < id_count; i++)
    {
      if (id_info [i].sector < track_info->min_sector)
	track_info->min_sector = id_info [i].sector;
      if (id_info [i].sector > track_info->max_sector)
	track_info->max_sector = id_info [i].sector;
    }
}


/*
 * make sure all sectors have the same cylinder, head, and size,
 * and determine the minimum and maximum sector numbers
 */
bool check_id_match (track_info_t *track_info,
		     id_info_t *id_info,
		     int id_count)
{
  int i;
  bool status = 1;

  for (i = 1; i < id_count; i++)
    {
      if (id_info [i].cylinder != id_info [0].cylinder)
	{
	  fprintf (stderr, "track contains a mix of cylinder numbers\n");
	  status = 0;
	}
      if (id_info [i].head != id_info [0].head)
	{
	  fprintf (stderr, "track contains a mix of head numbers\n");
	  status = 0;
	}
      if (id_info [i].size_code != id_info [0].size_code)
	{
	  fprintf (stderr, "track contains a mix of sector sizes\n");
	  status = 0;
	}
      if (id_info [i].sector < track_info->min_sector)
	track_info->min_sector = id_info [i].sector;
      if (id_info [i].sector > track_info->max_sector)
	track_info->max_sector = id_info [i].sector;
    }
  return (status);
}


/*
 * Test whether all sectors in the range min_sector..max_sector are
 * represented in the id_info array.
 */
bool all_sectors_present (track_info_t *track_info,
			  id_info_t *id_info,
			  int id_count)
{
  int i, j;

  for (i = track_info->min_sector; i <= track_info->max_sector; i++)
    {
      for (j = 0; j < id_count; j++)
	{
	  if (id_info [j].sector == i)
	    break;
	}
      if (j >= id_count)
	return (0);
    }
  return (1);
}


#define MAX_ID_READ 100


int try_track (int cylinder, int head,
	       disk_info_t *disk_info,
	       track_info_t *track_info)
{
  int i;
  int density;
  int density_present [2];
  id_info_t id_info [MAX_ID_READ];

  if (! seek (disk_info, cylinder))
    {
      fprintf (stderr, "error seeking to cylinder %d\n", cylinder);
      exit (2);
    }

  for (density = 0; density <= 1; density++)
    {
      if (verbose >= 2)
	{
	  fprintf (stderr, "checking for %s density\n", density ? "single" : "double");
	  fflush (stderr);
	}
      density_present [density] = read_id (disk_info, density, head,
					   & id_info [0]);
    }
  if (density_present [0] && density_present [1])
    {
      fprintf (stderr, "both FM and MFM data on cylinder %d head %d\n",
	       cylinder, head);
      return (0);
    }

  if (density_present [0])
    track_info->density = 0;
  else if (density_present [1])
    track_info->density = 1;
  else
    {
      fprintf (stderr, "neither FM nor MFM data on cylinder %d head %d\n",
	       cylinder, head);
      return (0);
    }

  for (i = 0; i < MAX_ID_READ; i++)
    if (! read_id (disk_info, track_info->density, head, & id_info [i]))
      {
	fprintf (stderr, "error reading ID address mark on cylinder %d head %d\n",
		 cylinder, head);
	return (0);
      }

  track_info->size_code = id_info [0].size_code;

  /* find the minimum and maximum sector numbers */
  find_min_max_sector (track_info, id_info, MAX_ID_READ);

  /* make sure all the sector IDs have the same cylinder, head, and size
     code */
  if (! check_id_match (track_info, id_info, MAX_ID_READ))
    return (0);

  /* now make sure all sector numbers from min_sector to max_sector are
     represented */
  if (! all_sectors_present (track_info, id_info, MAX_ID_READ))
    {
      fprintf (stderr, "track contains discontiguous sector numbers\n");
      return (0);
    }

  check_interleave (track_info, id_info, MAX_ID_READ);

  return (1);
}


int try_disk (disk_info_t *disk_info)
{
  int cylinder, head;
  int i;
  int result [4];

  for (cylinder = 0; cylinder <= 1; cylinder++)
    for (head = 0; head <= 1; head++)
      {
	i = (cylinder != 0) * 2 + head;
	result [i] = try_track (cylinder, head,	disk_info,
				& disk_info->track_info [i]);
	if (verbose && ! result [i])
	  printf ("no data on cylinder %d, head %d\n", cylinder, head);
      }

  if (! result [0])
    {
      fprintf (stderr, "can't find data on cylinder 0, head 0\n");
      return (0);
    }

  if (result [1])
    disk_info->head_count = 2;
  else
    disk_info->head_count = 1;

  return (1);
}


bool read_sector (disk_info_t *disk_info,
		  int cylinder, int head, int sector,
		  track_info_t *track_info,
		  u8 *buf)
{
  struct floppy_raw_cmd cmd;
  int i = 0;
  u8 mask = 0x5f;
  int sector_length = 128 << track_info->size_code;

  if (track_info->density)
    mask &= ~0x40;

  cmd.data = buf;
  cmd.length = sector_length;
  cmd.rate = disk_info->data_rate;
  cmd.flags = FD_RAW_INTR | FD_RAW_READ;
  cmd.cmd[i++] = FD_READ & mask;
  cmd.cmd[i++] = head ? 4 : 0;
  cmd.cmd[i++] = cylinder; /* Cylinder value (to check with header) */
  cmd.cmd[i++] = head ? 1 : 0; /* Head value (to check with header) */
  cmd.cmd[i++] = sector;
  cmd.cmd[i++] = track_info->size_code; /* sector length */
  cmd.cmd[i++] = track_info->max_sector; /* last sector number on a track */
  cmd.cmd[i++] = 14; /* gap length */
  cmd.cmd[i++] = (sector_length < 255) ? sector_length : 0xff;
  cmd.cmd_count=i;

  if (0 > ioctl (disk_info->floppy_dev, FDRAWCMD, & cmd))
    {
      if (verbose >= 2)
	{
	  perror ("ioctl FDRAWCMD in read_sector");
	}
      reset_drive (disk_info);
      return (0);
    }

  if (cmd.reply [0] & 0xc0)
    {
      for (i = 0; i < 3; i++)
	fprintf (stderr, "read ID status %d: %02x\n", i, cmd.reply [i]);
      return (0);
    }

  return (1);
}


char *progname;


void usage (void)
{
  fprintf (stderr, "usage:\n"
	   "%s [options] <image-file>\n"
	   "    -d <drive>            drive (default /dev/fd0)\n"
	   "    -ss                   single sided (default)\n"
	   "    -ds                   double sided\n"
	   "    -sd                   single density (FM, default)\n"
	   "    -dd                   double density (MFM)\n"
	   "    -bc <sector-size>     sector size in bytes (default 128/256 for FM/MFM)\n"
	   "    -sc <sector-count>    sector count (default 26)\n"
	   "    -cc <cylinder-count>  cylinder count (default 77)\n"
	   "    -mr <retry-count>     maximum retries (default 5)\n",
	   progname);
  fprintf (stderr, "If no disk characteristics are specified, the program will attempt\n"
	   "to automatically determine them\n");
  exit (1);
}


void read_track (disk_info_t *disk_info,
		 int cylinder,
		 int head,
		 track_info_t *track_info)
{
  int retry_count;
  bool status;
  int sector;
  u8 buf [16384];

  if (verbose == 1)
    {
      printf ("%02d %d\r", cylinder, head);
      fflush (stdout);
    }
  for (sector = track_info->min_sector;
       sector <= track_info->max_sector;
       sector++)
    {
      if (verbose == 2)
	{
	  printf ("%02d %d %02d\r", cylinder, head, sector);
	  fflush (stdout);
	}
      else if (verbose == 3)
	{
	  printf ("%02d %d %02d: ", cylinder, head, sector);
	  fflush (stdout);
	}
      retry_count = disk_info->max_retry;
      status = 0;
      while ((! status) && (retry_count-- > 0))
	status = read_sector (disk_info, cylinder, head, sector,
			      track_info, buf);
      if (verbose == 3)
	{
	  printf ("%s\n", status ? "ok" : "err");
	  fflush (stdout);
	}
      if (! status)
	{
	  if (verbose)
	    {
	      printf ("\n");
	      fflush (stdout);
	    }
	  fprintf (stderr, "error reading cyl %d head %d sect %d\n",
		   cylinder, head, sector);
#if 0
	  exit (2);
#endif
	}
      if (1 != fwrite (buf, 128 << track_info->size_code, 1,
		       disk_info->image_f))
	{
	  fprintf (stderr, "error writing output file\n");
	  exit (2);
	}
    }
}


void read_disk (disk_info_t *disk_info)
{
  int cylinder, head;
  track_info_t *track_info;

  for (cylinder = 0; cylinder < disk_info->cylinder_count; cylinder++)
    {
      if (! seek (disk_info, cylinder))
	{
	  fprintf (stderr, "error seeking\n");
	  exit (2);
	}
      for (head = 0; head < disk_info->head_count; head++)
	{
	  track_info = & disk_info->track_info [(cylinder != 0) * 2 + head];
	  read_track (disk_info, cylinder, head, track_info);
	}
    }

  if (! recalibrate (disk_info))
    {
      fprintf (stderr, "error recalibrating drive\n");
    }

  if (verbose)
    printf ("\n");
}


int open_drive (disk_info_t *disk_info, char *fn)
{
  struct floppy_drive_params fdp;

  disk_info->floppy_dev = open (fn, O_RDONLY | O_NDELAY, 0);
  if (! disk_info->floppy_dev)
    return (0);

#if 0
  if (0 > ioctl (disk_info->floppy_dev, FDMSGON, NULL))
    {
      fprintf (stderr, "can't enable floppy driver debug messages\n");
      return (0);
    }
#endif

  if (! reset_drive (disk_info))
    {
      fprintf (stderr, "can't reset drive\n");
      return (0);
    }

  if (verbose >= 2)
    {
      if (0 > ioctl (disk_info->floppy_dev, FDGETDRVPRM, & fdp))
	fprintf (stderr, "can't get drive parameters\n");
      else
	{
	  printf ("drive parameters:\n");
	  printf ("cmos: %d\n", fdp.cmos);
	  printf ("tracks: %d\n", fdp.tracks);
	  printf ("rpm: %d\n", fdp.rps * 60);
	}
    }

  if (! recalibrate (disk_info))
    {
      fprintf (stderr, "error recalibrating drive\n");
      return (0);
    }

  return (1);
}


void print_disk_info (disk_info_t *disk_info)
{
  int cylinder, head;

  printf ("%s sided\n", (disk_info->head_count - 1) ? "double" : "single");
  for (cylinder = 0; cylinder <= 1; cylinder++)
    for (head = 0; head < disk_info->head_count; head++)
      {
	printf ("cylinder %d head %d: ", cylinder, head);
	print_track_info (& disk_info->track_info [cylinder * 2 + head]);
      }
}


int sector_length_to_size_code (int sector_length)
{
  switch (sector_length)
    {
    case  128:   return (0);
    case  256:   return (1);
    case  512:   return (2);
    case 1024:   return (3);
    default:
      fprintf (stderr, "invalid sector length %d\n", sector_length);
      exit (1);
    }
}


int main (int argc, char *argv[])
{
  char *drive_fn = NULL;
  char *image_fn = NULL;

  int manual = 0;
  int sector_length;

  int i;

  disk_info_t disk_info =
  {
    0,    /* floppy_dev */
    NULL, /* image_f */
    500,  /* data rate */
    77,   /* cylinder_count */
    1,    /* head_count */
    0,    /* double_step */
    5,    /* max_retry */
    {     /* track_info */
      {
	1,    /* density - FM */
	0,    /* size code - 128 bytes */
	1,    /* min sector */
	26    /* max sector */
      }
    }
  };

  progname = argv [0];

  printf ("%s version $Revision: 1.9 $\n", progname);
  printf ("Copyright 2002 Eric Smith <eric@brouhaha.com>\n");

  while (argc > 1)
    {
      if (argv [1][0] == '-')
	{
	  if (strcmp (argv [1], "-d") == 0)
	    {
	      if ((drive_fn) || (argc < 3))
		usage ();
	      drive_fn = argv [2];
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-ss") == 0)
	    { manual = 1; disk_info.head_count = 1; }
	  else if (strcmp (argv [1], "-ds") == 0)
	    { manual = 1; disk_info.head_count = 2; }
	  else if (strcmp (argv [1], "-sd") == 0)
	    { manual = 1; disk_info.track_info [0].density = 1; }
	  else if (strcmp (argv [1], "-dd") == 0)
	    { manual = 1; disk_info.track_info [0].density = 0; }
	  else if (strcmp (argv [1], "-bc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      manual = 1;
	      sector_length = atoi (argv [2]);
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-sc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      manual = 1;
	      disk_info.track_info [0].max_sector = atoi (argv [2]);
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-cc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      disk_info.cylinder_count = atoi (argv [2]);
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-mr") == 0)
	    {
	      if (argc < 3)
		usage ();
	      disk_info.max_retry = atoi (argv [2]);
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-v") == 0)
	    {
	      verbose++;
	    }
	  else
	    {
	      fprintf (stderr, "unrecognized option '%s'\n", argv [1]);
	      usage ();
	    }
	}
      else if (! image_fn)
	image_fn = argv [1];
      else
	{
	  fprintf (stderr, "unrecognized argument '%s'\n", argv [1]);
	  usage ();
	}
      argc--;
      argv++;
    }

  if (! drive_fn)
    drive_fn = "/dev/fd0";

  if (! image_fn)
    {
      fprintf (stderr, "must specify image file name\n");
      usage ();
    }

  if (! open_drive (& disk_info, drive_fn))
    {
      fprintf (stderr, "error opening drive\n");
      exit (2);
    }

  if (! manual)
    {
      printf ("Attempting automatic disk characteristics discovery.\n");
      fflush (stdout);
      if (! try_disk (& disk_info))
	{
	  fprintf (stderr, "auto detect failed\n");
	  exit (2);
	}
      print_disk_info (& disk_info);
    }

  if (manual)
    {
      if (sector_length == 0)
	sector_length = disk_info.track_info [0].density ? 128 : 256;

      disk_info.track_info [0].size_code = sector_length_to_size_code (sector_length);

      for (i = 1; i <= 3; i++)
	memcpy (& disk_info.track_info [i],
		& disk_info.track_info [0],
		sizeof (track_info_t));
      /* following is only for debugging the command parsing */
      print_disk_info (& disk_info);
    }

  disk_info.image_f = fopen (image_fn, "wb");
  if (! disk_info.image_f)
    {
      fprintf (stderr, "error opening output file\n");
      exit (2);
    }

  read_disk (& disk_info);

  close (disk_info.floppy_dev);

  fclose (disk_info.image_f);

  exit (0);
}
