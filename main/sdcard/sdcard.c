#include "sdcard.h"

#include <stdio.h>
#include <string.h>

#include <sys/unistd.h>
#include <sys/stat.h>

#include <esp_vfs_fat.h>
#include <esp_log.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/gpio.h>

#define BASE_PATH_LEN (sizeof(sd_mount_point) + sizeof(sd_env_dir) - 2)

sdmmc_card_t *sd_card = NULL;

static const char sd_mount_point[] = SDCARD_MOUNT_POINT;
static const char sd_env_dir[] = SDCARD_ENVIRONMENT_DIR;
static FILE *file = NULL;
static char file_path[BASE_PATH_LEN + SDCARD_MAX_FILE_NAME + 7] = {SDCARD_MOUNT_POINT SDCARD_ENVIRONMENT_DIR};

bool sdcard_is_inserted(void)
{
    gpio_set_direction(SDCARD_CD_PIN, GPIO_MODE_INPUT);
    return gpio_get_level(SDCARD_CD_PIN) == 0;
}

bool sdcard_is_mounted(void)
{
    return sd_card != NULL;
}

sd_err_t sdcard_mount(void)
{
    if (!sdcard_is_inserted())
    {
        return SD_NOT_INSERTED;
    }

    esp_err_t err;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.command_timeout_ms = 250;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    slot_config.width = 4;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        // .disk_status_check_enable = true,
        .format_if_mount_failed = false,
        .allocation_unit_size = 1024,
        .max_files = 2,
    };

    ESP_LOGI("sdcard", "mounting filesystem...");
    err = esp_vfs_fat_sdmmc_mount(sd_mount_point, &host, &slot_config, &mount_config, &sd_card);
    if (err != ESP_OK)
    {
        if (err == ESP_FAIL) {
            ESP_LOGE("sdcard", "mount failed.");
            return SD_MOUNT_FAIL;
        } else {
            ESP_LOGE("sdcard", "unable to initialize SD card.");
            return SD_NOT_INSERTED;
        }
    }

    struct stat st;
    if (stat(SDCARD_MOUNT_POINT SDCARD_ENVIRONMENT_DIR, &st) == -1)
    {
        if (mkdir(SDCARD_MOUNT_POINT SDCARD_ENVIRONMENT_DIR, 0755) != 0)
        {
            ESP_LOGE("sdcard", "environment directory creation error.");
            sdcard_unmount();
            return SD_MKDIR_ERR;
        }
    }

    sdmmc_card_print_info(stdout, sd_card);
    return SD_OK;
}

sd_err_t sdcard_unmount(void)
{
    esp_vfs_fat_sdcard_unmount(sd_mount_point, sd_card);
    sd_card = NULL;
    return SD_OK;
}

int sdcard_file_size(const char *fname)
{
    struct stat st;

    if (strlen(fname) > SDCARD_MAX_FILE_NAME)
    {
        ESP_LOGE("sdcard", "too long file name '%s'", fname);
        return SD_LONG_NAME;
    }

    if (strchr(fname, '.') == NULL)
    {
        snprintf(file_path + BASE_PATH_LEN, SDCARD_MAX_FILE_NAME, "/%s.json", fname);
    }
    else
    {
        snprintf(file_path + BASE_PATH_LEN, SDCARD_MAX_FILE_NAME, "/%s", fname);
    }

    if (stat(file_path, &st) < 0)
    {
        return SD_NOT_EXISTS;
    }

    return st.st_size;
}

sd_err_t sdcard_open(const char *fname, const char *mode)
{
    if (strlen(fname) > SDCARD_MAX_FILE_NAME)
    {
        ESP_LOGE("sdcard", "too long file name '%s'", fname);
        return SD_LONG_NAME;
    }

    if (strchr(fname, '.') == NULL)
    {
        snprintf(file_path + BASE_PATH_LEN, SDCARD_MAX_FILE_NAME, "/%s.json", fname);
    }
    else
    {
        snprintf(file_path + BASE_PATH_LEN, SDCARD_MAX_FILE_NAME, "/%s", fname);
    }

    file = fopen(file_path, mode);
    if (file == NULL)
    {
        ESP_LOGE("sdcard", "unable to open file: %s '%s'", file_path, mode);
        return SD_NOT_EXISTS;
    }

    fseek(file, 0, SEEK_SET);
    return SD_OK;
}

sd_err_t sdcard_close(void)
{
    if (file == NULL) {
        return SD_OK;
    }

    fclose(file);
    return SD_OK;
}

int sdcard_read(void *outbuf, size_t size)
{
    int n = fread(outbuf, 1, size, file);
    if (n < 0)
    {
        return SD_READ_FAIL;
    }
    return n;
}

int sdcard_write(const void *inbuf, size_t size)
{
    int n = fwrite(inbuf, 1, size, file);
    if (n < 0)
    {
        return SD_WRITE_FAIL;
    }
    return n;
}
