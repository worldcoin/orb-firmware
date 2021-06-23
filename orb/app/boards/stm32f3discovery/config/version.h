#ifndef VERSION_H__
#define VERSION_H__

/* clang-format off */
#define     FIRMWARE_BRANCH_NAME_SIZE   4
#define     FIRMWARE_BRANCH_NAME        "b'main'"
#define     FIRMWARE_BRANCH_SHA         0xb'e2e4ba83'

#define     FIRMWARE_TYPE_DEV           0xFF
#define     FIRMWARE_TYPE_PROD          0x00

#define     FIRMWARE_VERSION_MAJOR      0
#define     FIRMWARE_VERSION_MINOR      1
#define     FIRMWARE_VERSION_PATCH      1

#ifdef DEBUG
	#define     FIRMWARE_TYPE       FIRMWARE_TYPE_DEV
#else
	#define     FIRMWARE_TYPE       FIRMWARE_TYPE_PROD
#endif

#define     HARDWARE_REV                1
/* clang-format on */
#endif
