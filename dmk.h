/*
 * DMK disk image format definitions
 *
 * $Id: dmk.h,v 1.3 2002/10/19 23:35:50 eric Exp $
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

#ifndef DMKLIB_DMK_H
#define DMKLIB_DMK_H

#include <stdint.h>


/* see dmk_header_t in libdmk.h for the layout */
#define DMK_HEADER_LENGTH 16


#define DMK_WRITE_ENABLE  0x00
#define DMK_WRITE_PROTECT 0xff

#define DMK_TRACK_LENGTH_8I_SD 5208
#define DMK_TRACK_LENGTH_8I_DD 10416

#define DMK_FLAG_NO_DENSITY 7  /* obsolete */
#define DMK_FLAG_SD_BIT     6
#define DMK_FLAG_RX02_BIT   5  /* extension defined by Tim Mann,
				  note that PC floppy controllers can't
			          read or write RX02 format */
#define DMK_FLAG_SS_BIT     4

#define DMK_FLAG_SS_MASK         (1 << DMK_FLAG_SS_BIT)
#define DMK_FLAG_RX02_MASK       (1 << DMK_FLAG_RX02_BIT)
#define DMK_FLAG_SD_MASK         (1 << DMK_FLAG_SD_BIT)
#define DMK_FLAG_NO_DENSITY_MASK (1 << DMK_FLAG_NO_DENSITY)

#define DMK_REAL_DISK 0x12345678


/* header byte 5, cw2dmk extension recording how a disk was read */
#define DMK_QUIRK_ID_CRC     0x01  /* MFM ID CRC omits the A1 premarks */
#define DMK_QUIRK_DATA_CRC   0x02  /* MFM data CRC omits the A1 premarks */
#define DMK_QUIRK_PREMARK    0x04
#define DMK_QUIRK_EXTRA      0x08  /* extra bytes after the data CRC */
#define DMK_QUIRK_EXTRA_CRC  0x10  /* extra bytes after the data CRC,
				      ending in their own CRC */
#define DMK_QUIRK_EXTRA_DATA 0x20  /* 4 extra data bytes before the
				      data CRC */
#define DMK_QUIRK_IAM        0x40
#define DMK_QUIRK_MFM_CLOCK  0x80
#define DMK_QUIRK_ALL        0xff



#define DMK_IDAM_POINTER_MFM_BIT  15
#define DMK_IDAM_POINTER_MFM_MASK (1 << DMK_IDAM_POINTER_MFM_BIT)

#define DMK_IDAM_POINTER_RSV_BIT  14
#define DMK_IDAM_POINTER_RSV_MASK (1 << DMK_IDAM_POINTER_RSV_BIT)

#define DMK_IDAM_POINTER_FLAGS_MASK (DMK_IDAM_POINTER_MFM_MASK | \
				     DMK_IDAM_POINTER_RSV_MASK)


/* names used by cw2dmk and gw2dmk */
#define DMK_SSIDE_OPT  DMK_FLAG_SS_MASK
#define DMK_RX02_OPT   DMK_FLAG_RX02_MASK
#define DMK_SDEN_OPT   DMK_FLAG_SD_MASK
#define DMK_IGNDEN_OPT DMK_FLAG_NO_DENSITY_MASK
#define DMK_DDEN_FLAG  DMK_IDAM_POINTER_MFM_MASK
#define DMK_EXTRA_FLAG DMK_IDAM_POINTER_RSV_MASK

#endif
