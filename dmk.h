/*
 * DMK disk image format definitions
 *
 * Copyright 2002 Eric Smith.
 *
 * $Id: dmk.h,v 1.1 2002/08/06 07:29:59 eric Exp $
 */

typedef struct
{
  uint8_t write_protect;  /* 0xff for protected, 0x00 for unprotected */  
  uint8_t track_count;
  uint16_t track_length;  /* little endian */
  uint8_t flags;
  uint8_t fill [7];
  uint32_t real;          /* little endian */
} dmk_disk_header_t;


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



#define DMK_IDAM_POINTER_MFM_BIT  15
#define DMK_IDAM_POINTER_MFM_MASK (1 << DMK_IDAM_POINTER_MFM_BIT)


