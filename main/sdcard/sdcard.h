#pragma once

#include <stddef.h>
#include <stdbool.h>

#define SDCARD_MOUNT_POINT      "/sdcard"
#define SDCARD_ENVIRONMENT_DIR  "/hexowl"
#define SDCARD_MAX_FILE_NAME    (64)
#define SDCARD_CD_PIN			(27)

typedef enum {
    SD_OK           = 0,
	SD_NOT_INSERTED = -1,
	SD_MOUNT_FAIL   = -2,
	SD_NOT_EXISTS   = -3,
	SD_WRITE_FAIL   = -4,
	SD_READ_FAIL    = -5,
	SD_LONG_NAME    = -6,
    SD_MKDIR_ERR    = -7,
} sd_err_t;

bool sdcard_is_inserted(void);
bool sdcard_is_mounted(void);

sd_err_t sdcard_mount(void);
sd_err_t sdcard_unmount(void);

int sdcard_file_size(const char *fname);
sd_err_t sdcard_open(const char *fname, const char *mode);
sd_err_t sdcard_close(void);

int sdcard_read(void *outbuf, size_t size);
int sdcard_write(const void *inbuf, size_t size);
