#include <stdio.h>
#include <stdlib.h>
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
  //  return (0 <= ioctl (dev, FDRAWCMD, & cmd));
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


bool read_id (int dev, int fm, int seek_head, int *cylinder, int *head, int *sector, int *sector_size)
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

  if (0 > ioctl (dev, FDRAWCMD, & cmd))
    return (0);
  
  if (cmd.reply [0] & 0x40)
    return (0);

  *cylinder    = cmd.reply [3];
  *head        = cmd.reply [4];
  *sector      = cmd.reply [5];
  *sector_size = 128 << cmd.reply [6];

  return (1);
}



int main (int argc, char *argv[])
{
  int dev;
  struct floppy_drive_params fdp;
  int seek_cylinder, seek_head;
  int cylinder, head, sector, sector_size;
  int fm;
  int reset_now;

  dev = open (argv [1], O_RDONLY | O_NDELAY, 0);

  if (0 > dev)
    {
      fprintf (stderr, "error opening drive\n");
      exit (2);
    }

  seek_cylinder = atoi (argv [2]);
  seek_head = atoi (argv [3]);
  fm = atoi (argv [4]);

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

  printf ("cmos: %d\n", fdp.cmos);
  printf ("tracks: %d\n", fdp.tracks);
  printf ("rpm: %d\n", fdp.rps * 60);

  switch (fdp.rps)
    {
    case 5:  /* 300 rpm */
      data_rate = FD_RATE_250_KBPS;
      break;
    case 6:  /* 360 rpm */
      data_rate = FD_RATE_300_KBPS;
      break;
    default:
      fprintf (stderr, "unrecognized drive rotation rate %d rpm\n", fdp.rps * 60);
      exit (2);
    }

  if (! recalibrate (dev))
    {
      fprintf (stderr, "error recalibrating drive\n");
      exit (2);
    }

  if (! seek (dev, seek_cylinder))
    {
      fprintf (stderr, "error seeking\n");
      exit (2);
    }

  while (1)
    {
      if (read_id (dev, fm, seek_head, & cylinder, & head, & sector, & sector_size))
	{
	  printf ("cyl %02d, head %d, sector %02d, size %d\n",
		  cylinder, head, sector, sector_size);
	}
      else
	{
	  fprintf (stderr, "error reading ID\n");
	  exit (2);
	}
    }

  close (dev);

  exit (0);
}
