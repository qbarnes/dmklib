/*
 * rfloppy - read floppy disks
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: rfloppy.c,v 1.6 2002/08/05 09:51:38 eric Exp $
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


#undef CHECK_INTERLEAVE  /* unfortunately we can't consistently read
			    consecutive sector IDs, so this doesn't work. */


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


FILE *image_f;
int floppy_dev;

/* parameters as specified by user */
int cylinder_count = 77;
int head_count = 1;
int double_step = 0;
int data_rate = 500;  /* Kbps */
int min_sector [2] = { 1, 1 };
int max_sector [2] = { 26, 26 };
int fm = 1;
int max_retry = 5;
int size_code = 0;  /* 128 bytes */


bool recalibrate (void)
{
  struct floppy_raw_cmd cmd;
  int i = 0;

  cmd.data = NULL;
  cmd.length = 0;
  cmd.rate = data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd [i++] = FD_RECALIBRATE;
  cmd.cmd [i++] = 0;
  cmd.cmd_count = i;
  return (0 <= ioctl (floppy_dev, FDRAWCMD, & cmd));
}


bool seek (int cylinder)
{
  struct floppy_raw_cmd cmd;
  int i = 0;

  cmd.data = NULL;
  cmd.length = 0;
  cmd.rate = data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd[i++] = FD_SEEK;
  cmd.cmd[i++] = 0;
  cmd.cmd[i++] = cylinder << double_step;
  cmd.cmd_count = i;

  return (0 <= ioctl (floppy_dev, FDRAWCMD, & cmd));
}


bool fd_read_track (int cylinder, int head,
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
  cmd.cmd[i++] = max_sector [head]; /* last sector number on a track */
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
  int size;
} id_info_t;


int read_id (int fm, int seek_head, id_info_t *id_info)
{
  struct floppy_raw_cmd cmd;
  u8 mask = 0x5f;
  int i = 0;

  if (fm)
    mask &= ~0x40;

  cmd.data = NULL;   /* No data to transfer */
  cmd.length = 0;
  cmd.rate = data_rate;
  cmd.flags = FD_RAW_INTR;
  cmd.cmd[i++] = FD_READID & mask;
  cmd.cmd[i++] = seek_head ? 4 : 0;
  cmd.cmd_count = i;

  if (0 > ioctl (floppy_dev, FDRAWCMD, & cmd))
    return (0);
  
  if (cmd.reply [0] & 0x40)
    return (0);

  id_info->cylinder = cmd.reply [3];
  id_info->head     = cmd.reply [4];
  id_info->sector   = cmd.reply [5];
  id_info->size     = 128 << cmd.reply [6];

  return (1);
}


#define MAX_ID_READ 100


#ifdef CHECK_INTERLEAVE
int check_interleave (id_info_t *id_info,
		      int min_sector,
		      int max_sector)
{
  int i;
  int first_pos = -1;
  int last_pos;
  u8 found [256];
  int count = 0;
  int status = 1;

  memset (found, 0, sizeof (found));

  for (i = 0; i < MAX_ID_READ; i++)
    if (id_info [i].sector == min_sector)
      {
	first_pos = i;
	break;
      }
  if (first_pos < 0)
    return (0);

  last_pos = first_pos + (max_sector - min_sector) - 1;

  for (i = first_pos; i <= last_pos; i++)
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
  if (count != ((max_sector - min_sector) + 1))
    {
      fprintf (stderr, "didn't get full range of sector IDs in one revolution\n");
      status = 0;
    }
  printf ("sector order:");
  for (i = first_pos; i <= last_pos; i++)
    printf (" %d", id_info [i].sector);
  printf ("\n");
  return (status);
}
#endif /* CHECK_INTERLEAVE */


int try_track (int cylinder, int head,
	       int *fm, int *sector_size,
	       int *min_sector, int *max_sector)
{
  int i, j;
  int density;
  int density_present [2];
  id_info_t id_info [MAX_ID_READ];
  int status = 1;
  int min_s, max_s;

  for (density = 0; density <= 1; density++)
    density_present [density] = read_id (density, head, & id_info [0]);
  if (density_present [0] && density_present [1])
    {
      fprintf (stderr, "both FM and MFM data on track\n");
      return (0);
    }

  if (density_present [0])
    *fm = 0;
  else if (density_present [1])
    *fm = 1;
  else
    return (0);

  for (i = 0; i < MAX_ID_READ; i++)
    if (! read_id (*fm, head, & id_info [i]))
      {
	fprintf (stderr, "error reading ID address mark\n");
	return (0);
      }

  /*
   * make sure all sectors have the same cylinder, head, and size,
   * and determine the minimum and maximum sector numbers
   */
  min_s = max_s = id_info [0].sector;
  for (i = 1; i < MAX_ID_READ; i++)
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
      if (id_info [i].size != id_info [0].size)
	{
	  fprintf (stderr, "track contains a mix of sector sizes\n");
	  status = 0;
	}
      if (id_info [i].sector < min_s)
	min_s = id_info [i].sector;
      if (id_info [i].sector > max_s)
	max_s = id_info [i].sector;
    }

  if (! status)
    return (0);

  /* now make sure all sector numbers from min_sector to max_sector are
     represented */
  for (i = min_s; i <= max_s; i++)
    {
      for (j = 0; j < MAX_ID_READ; j++)
	{
	  if (id_info [j].sector == i)
	    break;
	}
      if (j >= MAX_ID_READ)
	{
	  fprintf (stderr, "track contains discontiguous sector numbers\n");
	  return (0);
	}
    }
      
#ifdef CHECK_INTERLEAVE
  check_interleave (id_info, min_s, max_s);
#endif /* CHECK_INTERLEAVE */

  *sector_size = id_info [0].size;
  *min_sector = min_s;
  *max_sector = max_s;

  return (1);
}


int try_disk (int *ds, int *fm, int *sector_size, int *min_sector,
	      int *max_sector)
{
  int density [2];
  int size [2];
  int status = 1;  /* assume success */

  if (! try_track (0, 0, & density [0], & size [0],
		   & min_sector [0], & max_sector [0]))
    {
      fprintf (stderr, "can't find data on cylinder 0, head 0\n");
      return (0);
    }

  if (! try_track (0, 1, & density [1], & size [1],
		   & min_sector [1], & max_sector [1]))
    {
      *ds = 0;
      *fm = density [0];
      *sector_size = size [0];
      return (1);
    }

  if (density [0] != density [1])
    {
      fprintf (stderr, "density mismatch on cylinder 0, heads 0 and 1\n");
      status = 0;
    }

  if (size [0] != size [1])
    {
      fprintf (stderr, "sector size mismatch on cylinder 0, heads 0 and 1\n");
      status = 0;
    }

  if (status)
    {
      *ds = 1;
      *fm = density [0];
      *sector_size = size [0];
    }
  
  return (status);
}


bool read_sector (int cylinder, int head, int sector,
		  u8 *buf)
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
  cmd.cmd[i++] = FD_READ & mask;
  cmd.cmd[i++] = head ? 4 : 0;
  cmd.cmd[i++] = cylinder; /* Cylinder value (to check with header) */
  cmd.cmd[i++] = head ? 1 : 0; /* Head value (to check with header) */
  cmd.cmd[i++] = sector;
  cmd.cmd[i++] = size_code; /* 256 byte MFM sectors */
  cmd.cmd[i++] = max_sector [head]; /* last sector number on a track */
  cmd.cmd[i++] = 14; /* gap length */
  cmd.cmd[i++] = (sector_size < 255) ? sector_size : 0xff;
  cmd.cmd_count=i;
  return ((0 <= ioctl (floppy_dev, FDRAWCMD, & cmd)) &&
	  ! (cmd.reply[0] & 0xC0));
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


void read_track (int cylinder,
		 int head)
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
  for (sector = min_sector [head]; sector <= max_sector [head]; sector++)
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
      retry_count = max_retry;
      status = 0;
      while ((! status) && (retry_count-- > 0))
	status = read_sector (cylinder, head, sector, buf);
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
      if (1 != fwrite (buf, 128 << size_code, 1, image_f))
	{
	  fprintf (stderr, "error writing output file\n");
	  exit (2);
	}
    }
}


void read_disk (void)
{
  int cylinder, head;

  for (cylinder = 0; cylinder < cylinder_count; cylinder++)
    {
      if (! seek (cylinder))
	{
	  fprintf (stderr, "error seeking\n");
	  exit (2);
	}
      for (head = 0; head < head_count; head++)
	{
	  read_track (cylinder, head);
	}
    }

  if (! recalibrate ())
    {
      fprintf (stderr, "error recalibrating drive\n");
    }

  if (verbose)
    printf ("\n");

}


int open_drive (char *fn)
{
  int reset_now;
  struct floppy_drive_params fdp;

  floppy_dev = open (fn, O_RDONLY | O_NDELAY, 0);
  if (! floppy_dev)
    return (0);

  if (0 > ioctl (floppy_dev, FDRESET, & reset_now))
    {
      fprintf (stderr, "can't reset drive\n");
      return (0);
    }

  if (0 > ioctl (floppy_dev, FDGETDRVPRM, & fdp))
    {
      fprintf (stderr, "can't get drive parameters\n");
      return (0);
    }

#if 0
  printf ("cmos: %d\n", fdp.cmos);
  printf ("tracks: %d\n", fdp.tracks);
  printf ("rpm: %d\n", fdp.rps * 60);
#endif
  if (! recalibrate ())
    {
      fprintf (stderr, "error recalibrating drive\n");
      return (0);
    }

  return (1);
}


int main (int argc, char *argv[])
{
  char *drive_fn = NULL;
  char *image_fn = NULL;

  int sector_size = 0; /* bytes */

  int manual = 0;

  progname = argv [0];

  printf ("%s version $Revision: 1.6 $\n", progname);
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
	    { head_count = 1; manual = 1; }
	  else if (strcmp (argv [1], "-ds") == 0)
	    { head_count = 2; manual = 1; }
	  else if (strcmp (argv [1], "-sd") == 0)
	    { fm = 1; manual = 1; }
	  else if (strcmp (argv [1], "-dd") == 0)
	    { fm = 0; manual = 1; }
	  else if (strcmp (argv [1], "-bc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      sector_size = atoi (argv [2]);
	      manual = 1;
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-sc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      max_sector [0] = max_sector [1] = atoi (argv [2]);
	      manual = 1;
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-cc") == 0)
	    {
	      if (argc < 3)
		usage ();
	      cylinder_count = atoi (argv [2]);
	      argc--;
	      argv++;
	    }
	  else if (strcmp (argv [1], "-mr") == 0)
	    {
	      if (argc < 3)
		usage ();
	      max_retry = atoi (argv [2]);
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

  if (! open_drive (drive_fn))
    {
      fprintf (stderr, "error opening drive\n");
      exit (2);
    }

  if (! manual)
    {
      int i, ds;
      printf ("Attempting automatic disk characteristics discovery.\n");
      fflush (stdout);
      if (! try_disk (& ds, & fm, & sector_size, min_sector, max_sector))
	{
	  fprintf (stderr, "auto detect failed\n");
	  exit (2);
	}
      head_count = ds + 1;
      printf ("%s sided, %s density, %d byte sectors\n",
	      ds ? "double" : "single",
	      fm ? "single" : "double",
	      sector_size);
      for (i = 0; i <= ds; i++)
	printf ("head %d sectors numbered from %d to %d\n",
		i, min_sector [i], max_sector [i]);
      fflush (stdout);
    }

  switch (sector_size)
    {
    case 0:
      if (fm)
	{ sector_size = 128; size_code = 0; }
      else
	{ sector_size = 256; size_code = 1; }
      break;
    case  128:   size_code = 0;   break;
    case  256:   size_code = 1;   break;
    case  512:   size_code = 2;   break;
    case 1024:   size_code = 3;   break;
    case 2048:   size_code = 4;   break;
    case 4096:   size_code = 5;   break;
    case 8192:   size_code = 6;   break;
    default:
      fprintf (stderr, "invalid sector size\n");
      exit (1);
    }

  image_f = fopen (image_fn, "wb");
  if (! image_f)
    {
      fprintf (stderr, "error opening output file\n");
      exit (2);
    }

  data_rate = FD_RATE_500_KBPS;

  read_disk ();

  close (floppy_dev);

  fclose (image_f);

  exit (0);
}
