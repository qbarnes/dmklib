/*
 * dumpids
 *
 * $Id: dumpids.c,v 1.3 2002/10/19 23:36:47 eric Exp $
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
  int fm, rate;
  int reset_now;

  if (argc != 6)
    {
      fprintf (stderr, "usage:\n"
	       "%s device cylinder head fm rate\n", argv [0]);
      exit (1);
    }

  dev = open (argv [1], O_RDONLY | O_NDELAY, 0);

  if (0 > dev)
    {
      fprintf (stderr, "error opening drive\n");
      exit (2);
    }

  seek_cylinder = atoi (argv [2]);
  seek_head = atoi (argv [3]);
  fm = atoi (argv [4]);
  rate = atoi (argv [5]);

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
      fprintf (stderr, "unrecognized data rate, only 250, 300 and 500 supported\n");
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
