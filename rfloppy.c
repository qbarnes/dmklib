/*
 * rfloppy - read floppy disks
 *
 * Copyright 2002 Eric Smith.
 *
 */


#undef PRINT_STATUS
#undef VERBOSE_STATUS


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fd.h>
#include <linux/fdreg.h>


typedef int bool;
typedef unsigned char u8;


int double_step = 0;
int data_rate;


/* rate codes are unfortunately NOT defined in fdreg.h */
#define FD_RATE_250_KBPS 2
#define FD_RATE_300_KBPS 1
#define FD_RATE_500_KBPS 0


bool recalibrate (int dev)
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
  return (0 <= ioctl (dev, FDRAWCMD, & cmd));
}


bool seek (int dev, int cylinder)
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

  return (0 <= ioctl (dev, FDRAWCMD, & cmd));
}


bool read_sector (int dev,
		  int cylinder, int head, int sector,
		  int size_code, int max_sector, int fm, int rate, 
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
  cmd.cmd[i++] = max_sector; /* last sector number on a track */
  cmd.cmd[i++] = 14; /* gap length */
  cmd.cmd[i++] = (sector_size < 255) ? sector_size : 0xff;
  cmd.cmd_count=i;
  return ((0 <= ioctl (dev, FDRAWCMD, & cmd)) &&
	  ! (cmd.reply[0] & 0xC0));
}


int main (int argc, char *argv[])
{
  int dev;
  struct floppy_drive_params fdp;

  /* parameters as specified by user */
  int cylinder_count, head_count;
  int first_sector, last_sector;
  int sector_size; /* bytes */
  int rate;

  int cylinder, head, sector;
  int size_code;
  int fm;

  bool status;

  int reset_now;
  FILE *outf;
  u8 buf [512];

  if (argc != 9)
    {
      fprintf (stderr, "usage:\n"
	       "%s drive cylinders heads sectors secsize fm|mfm rate image\n", argv [0]);
      exit (1);
    }

  dev = open (argv [1], O_RDONLY | O_NDELAY, 0);

  if (0 > dev)
    {
      fprintf (stderr, "error opening drive\n");
      exit (2);
    }

  outf = fopen (argv [argc - 1], "wb");
  if (! outf)
    {
      fprintf (stderr, "error opening output file\n");
      exit (2);
    }

  cylinder_count = atoi (argv [2]);
  head_count = atoi (argv [3]);
  first_sector = 1;
  last_sector = atoi (argv [4]);
  sector_size = atoi (argv [5]);
  rate = atoi (argv [7]);

  if (strcasecmp (argv [6], "fm") == 0)
    fm = 1;
  else if (strcasecmp (argv [6], "mfm") == 0)
    fm = 0;
  else
    {
      fprintf (stderr, "unrecognized density, choices are fm and mfm\n");
      exit (1);
    }

  switch (sector_size)
    {
    case 128:
      size_code = 0;
      break;
    case 256:
      size_code = 1;
      break;
    case 512:
      size_code = 2;
      break;
    case 1024:
      size_code = 3;
      break;
    case 2048:
      size_code = 4;
      break;
    case 4096:
      size_code = 5;
      break;
    case 8192:
      size_code = 6;
      break;
    default:
      fprintf (stderr, "invalid sector size\n");
      exit (1);
    }

  switch (rate)
    {
    case 250:
      data_rate = FD_RATE_250_KBPS;
      break;
    case 300:
      data_rate = FD_RATE_300_KBPS;
      break;
    case 500:
      data_rate = FD_RATE_500_KBPS;
      break;
    default:
      fprintf (stderr, "unrecognized data transfer rate, must be 250, 300, or 500\n");
      exit (1);
    }

  if ((head_count != 1) && (head_count != 2))
    {
      fprintf (stderr, "head count must be 1 or 2\n");
      exit (1);
    }

  if (0 > ioctl (dev, FDRESET, & reset_now))
    {
      fprintf (stderr, "can't reset drive\n");
      exit (2);
    }

  if (0 > ioctl (dev, FDGETDRVPRM, & fdp))
    {
      fprintf (stderr, "can't get drive parameters\n");
      exit (2);
    }

#if 0
  printf ("cmos: %d\n", fdp.cmos);
  printf ("tracks: %d\n", fdp.tracks);
  printf ("rpm: %d\n", fdp.rps * 60);
#endif

  if (! recalibrate (dev))
    {
      fprintf (stderr, "error recalibrating drive\n");
      exit (2);
    }

  for (cylinder = 0; cylinder < cylinder_count; cylinder++)
    {
      if (! seek (dev, cylinder))
	{
	  fprintf (stderr, "error seeking\n");
	  exit (2);
	}
      for (head = 0; head < head_count; head++)
	{
	  for (sector = first_sector; sector <= last_sector; sector++)
	    {
#ifdef PRINT_STATUS
#ifdef VERBOSE_STATUS
	      printf ("%02d %d %02d: ", cylinder, head, sector);
#else
	      printf ("%02d %d %02d\r", cylinder, head, sector);
#endif
	      fflush (stdout);
#endif
	      status = read_sector (dev, cylinder, head, sector,
				    size_code, last_sector, fm, data_rate,
				    buf);
#ifdef VERBOSE_STATUS
	      printf ("%s\n", status ? "ok" : "err");
#endif
	      if (! status)
		{
#ifdef PRINT_STATUS
		  printf ("\n");
		  fflush (stdout);
#endif
		  fprintf (stderr, "error reading cyl %d head %d sect %d\n",
			   cylinder, head, sector);
#if 0
		  exit (2);
#endif
		}
	      if (1 != fwrite (buf, 128 << size_code, 1, outf))
		{
		  fprintf (stderr, "error writing output file\n");
		  exit (2);
		}
	    }
	}
    }

  if (! recalibrate (dev))
    {
      fprintf (stderr, "error recalibrating drive\n");
    }

#ifdef PRINT_STATUS
  printf ("\n");
#endif

  close (dev);

  fclose (outf);

  exit (0);
}
