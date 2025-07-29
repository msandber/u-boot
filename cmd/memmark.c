// SPDX-License-Identifier: GPL-2.0+
/*
 * memmark - RAM test command to write markers skipping SPL, U-Boot and reserved regions
 */

#include <command.h>
#include <mapmem.h>
#include <asm/io.h>   // for readl/writel if needed

#define RAM_START       0x80000000UL
#define RAM_SIZE        0x10000000UL   /* 256 MB */

#define SPL_START       0x80100000UL
#define SPL_END         (SPL_START + 0x100000UL)

#define UBOOT_START     0x80200000UL
#define UBOOT_END       (UBOOT_START + 0x100000UL)

#define RESERVED_START  0x8ecfd000UL
#define RESERVED_END    0x8fffffffUL

#define MIN_RANGE_LEN   0x1000UL

static void print_progress(unsigned long addr, unsigned long end,
                           unsigned long *last_printed)
{
    const unsigned long coarse_step      = 0x1000000UL;  // 16 MB

    if (addr - *last_printed >= coarse_step || addr + sizeof(u32) >= end) {
        *last_printed = addr;
        printf("Processed up to 0x%08lx\n", addr);
    }
}

static int memmark_write(void)
{
    unsigned long addr;
    unsigned long end = RAM_START + RAM_SIZE;
    unsigned long last_printed = 0;
    const u32 marker = 0xA5A5A5A5;

    printf("=== memmark: writing markers to RAM ===\n");

    for (addr = RAM_START; addr < end; addr += sizeof(u32)) {
        if ((addr >= SPL_START && addr < SPL_END) ||
            (addr >= UBOOT_START && addr < UBOOT_END) ||
            (addr >= RESERVED_START && addr <= RESERVED_END)) {
            continue;
        }

        writel(marker, map_sysmem(addr, sizeof(u32)));

        print_progress(addr, end, &last_printed);
    }

    printf("Markers written from 0x%08lx to 0x%08lx (skipped reserved regions)\n",
           RAM_START, end);
    return 0;
}

static int memmark_test(void)
{
    unsigned long addr;
    unsigned long end = RAM_START + RAM_SIZE;
    unsigned long last_printed = 0;
    const u32 marker = 0xA5A5A5A5;

    unsigned long range_start = 0;
    unsigned long range_end = 0;
    int in_range = 0;
    int found_any = 0;

    printf("=== memmark: scanning for markers in RAM ===\n");

    for (addr = RAM_START; addr < end; addr += sizeof(u32)) {
        if ((addr >= SPL_START && addr < SPL_END) ||
            (addr >= UBOOT_START && addr < UBOOT_END) ||
            (addr >= RESERVED_START && addr <= RESERVED_END)) {
            // Close range if open before skipping reserved
            if (in_range) {
                unsigned long length = range_end - range_start + sizeof(u32);
                if (length >= MIN_RANGE_LEN) {
                    printf("  0x%08lx - 0x%08lx (length: 0x%lx bytes)\n",
                           range_start, range_end, length);
                    found_any = 1;
                }
                in_range = 0;
            }
            continue;
        }

        u32 val = readl(map_sysmem(addr, sizeof(u32)));
        if (val == marker) {
            if (!in_range) {
                range_start = addr;
                range_end = addr;
                in_range = 1;
            } else {
                range_end = addr;
            }
        } else {
            if (in_range) {
                unsigned long length = range_end - range_start + sizeof(u32);
                if (length >= MIN_RANGE_LEN) {
                    printf("  0x%08lx - 0x%08lx (length: 0x%lx bytes)\n",
                           range_start, range_end, length);
                    found_any = 1;
                }
                in_range = 0;
            }
        }

        print_progress(addr, end, &last_printed);
    }

    // Close any open range at the end
    if (in_range) {
        unsigned long length = range_end - range_start + sizeof(u32);
        if (length >= MIN_RANGE_LEN) {
            printf("  0x%08lx - 0x%08lx (length: 0x%lx bytes)\n",
                   range_start, range_end, length);
            found_any = 1;
        }
    }

    if (!found_any)
        printf("No marker ranges found.\n");

    return found_any ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

static int do_memmark(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    if (argc < 2) {
        printf("Usage: memmark write|test\n");
        return CMD_RET_USAGE;
    }

    if (!strcmp(argv[1], "write"))
        return memmark_write();
    else if (!strcmp(argv[1], "test"))
        return memmark_test();

    return CMD_RET_USAGE;
}

U_BOOT_CMD(
    memmark, 2, 0, do_memmark,
    "Write and test RAM markers skipping SPL, U-Boot and reserved regions",
    "write   - fill RAM with test marker, skipping reserved ranges\n"
    "memmark test    - verify markers"
);
