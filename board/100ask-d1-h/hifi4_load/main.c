/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <types.h>

#include <config.h>
#include <log.h>

#include <common.h>
#include <mmu.h>

#include "sys-dram.h"
#include "sys-sdcard.h"
#include "sys-sid.h"
#include "sys-spi.h"

#include "elf_loader.h"
#include "ff.h"

#define CONFIG_HIFI4_ELF_FILENAME "c906.elf"
#define CONFIG_HIFI4_ELF_LOADADDR (0x45000000)

#define CONFIG_SDMMC_SPEED_TEST_SIZE 1024// (unit: 512B sectors)

extern sunxi_serial_t uart_dbg;

extern sdhci_t sdhci0;

extern dram_para_t dram_para;

#define FILENAME_MAX_LEN 64
typedef struct {
    uint8_t *dest;
    char filename[FILENAME_MAX_LEN];
} image_info_t;

image_info_t image;

#define CHUNK_SIZE 0x20000

static int fatfs_loadimage(char *filename, BYTE *dest) {
    FIL file;
    UINT byte_to_read = CHUNK_SIZE;
    UINT byte_read;
    UINT total_read = 0;
    FRESULT fret;
    int ret;
    uint32_t start, time;

    fret = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ);
    if (fret != FR_OK) {
        printk(LOG_LEVEL_ERROR,
               "FATFS: open, filename: [%s]: error %d\n", filename,
               fret);
        ret = -1;
        goto open_fail;
    }

    start = time_ms();

    do {
        byte_read = 0;
        fret = f_read(&file, (void *) (dest), byte_to_read, &byte_read);
        dest += byte_to_read;
        total_read += byte_read;
    } while (byte_read >= byte_to_read && fret == FR_OK);

    time = time_ms() - start + 1;

    if (fret != FR_OK) {
        printk(LOG_LEVEL_ERROR, "FATFS: read: error %d\n", fret);
        ret = -1;
        goto read_fail;
    }
    ret = 0;

read_fail:
    fret = f_close(&file);

    printk(LOG_LEVEL_DEBUG, "FATFS: read in %ums at %.2fMB/S\n", time,
           (f32) (total_read / time) / 1024.0f);

open_fail:
    return ret;
}

static int load_sdcard(image_info_t *image) {
    FATFS fs;
    FRESULT fret;
    int ret;
    uint32_t start;

    uint32_t test_time;
    start = time_ms();
    sdmmc_blk_read(&card0, (uint8_t *) (SDRAM_BASE), 0, CONFIG_SDMMC_SPEED_TEST_SIZE);
    test_time = time_ms() - start;
    printk(LOG_LEVEL_DEBUG, "SDMMC: speedtest %uKB in %ums at %uKB/S\n",
           (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / 1024, test_time,
           (CONFIG_SDMMC_SPEED_TEST_SIZE * 512) / test_time);

    start = time_ms();

    fret = f_mount(&fs, "", 1);
    if (fret != FR_OK) {
        printk(LOG_LEVEL_ERROR, "FATFS: mount error: %d\n", fret);
        return -1;
    } else {
        printk(LOG_LEVEL_DEBUG, "FATFS: mount OK\n");
    }

    printk(LOG_LEVEL_INFO, "FATFS: read %s addr=%x\n", image->filename, (unsigned int) image->dest);
    ret = fatfs_loadimage(image->filename, image->dest);
    if (ret)
        return ret;

    /* umount fs */
    fret = f_mount(0, "", 0);
    if (fret != FR_OK) {
        printk(LOG_LEVEL_ERROR, "FATFS: unmount error %d\n", fret);
        return -1;
    } else {
        printk(LOG_LEVEL_DEBUG, "FATFS: unmount OK\n");
    }
    printk(LOG_LEVEL_DEBUG, "FATFS: done in %ums\n", time_ms() - start);

    return 0;
}

int main(void) {
    sunxi_serial_init(&uart_dbg);// Initialize the serial interface for debugging

    show_banner();// Display a banner

    sunxi_clk_init();// Initialize clock configurations

    sunxi_dram_init(&dram_para);// Initialize DRAM parameters

    sunxi_clk_dump();// Dump clock information

    memset(&image, 0, sizeof(image_info_t));// Clear the image structure

    // Set destination addresses for different images
    image.dest = (uint8_t *) CONFIG_HIFI4_ELF_LOADADDR;

    // Set filenames for different images
    strcpy(image.filename, CONFIG_HIFI4_ELF_FILENAME);

    // Initialize SDHCI controller
    if (sunxi_sdhci_init(&sdhci0) != 0) {
        printk(LOG_LEVEL_ERROR, "SMHC: %s controller init failed\n", sdhci0.name);
        return 0;
    } else {
        printk(LOG_LEVEL_INFO, "SMHC: %s controller v%x initialized\n", sdhci0.name, sdhci0.reg->vers);
    }

    // Initialize SD/MMC card
    if (sdmmc_init(&card0, &sdhci0) != 0) {
        printk(LOG_LEVEL_ERROR, "SMHC: init failed\n");
        return 0;
    }

    // Load image from SD card
    if (load_sdcard(&image) != 0) {
        printk(LOG_LEVEL_ERROR, "SMHC: loading failed\n");
        return 0;
    }

    sunxi_hifi4_clock_reset();// Reset C906 clock

    // Get entry address of RISC-V ELF
    uint32_t elf_run_addr = elf64_get_entry_addr((phys_addr_t) image.dest);
    printk(LOG_LEVEL_INFO, "RISC-V ELF run addr: 0x%08x\n", elf_run_addr);

    // Load RISC-V ELF image
    if (load_elf64_image((phys_addr_t) image.dest)) {
        printk(LOG_LEVEL_ERROR, "RISC-V ELF load FAIL\n");
    }

    printk(LOG_LEVEL_INFO, "RISC-V C906 Core now Running... \n");

    mdelay(100);// Delay for 100 milliseconds

    sunxi_hifi4_clock_init(elf_run_addr);// Initialize C906 clock with entry address

    abort();// Abort A7 execution, loop forever

    jmp_to_fel();// Jump to FEL mode

    return 0;
}