#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * srd: Sub Region Disable
 * srs: Sub Region Size
 */

#ifndef NDEBUG
#define DEBUG_LOG(fmt, ...) printf("<%s:%d:%s()> "fmt"\r\n", __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) do {} while (0);
#endif

#define ARRAY_SIZE(item)    (sizeof(item) / sizeof(item[0]))

#define B(bytes)    (bytes)
#define KB(bytes)   (B((bytes) * 1024ul))
#define MB(Mbytes)  (KB((Mbytes) * 1024ul))
#define GB(Gbytes)  (MB((Gbytes) * 1024ul))
#define MPU_REGIONS (8)

#ifndef MEMORIES
#define MEMORIES {\
    {"rom", 0x00000000, KB(1024)},\
    {"sram", 0x20000000, KB(384)},\
    {"dram", 0x60000000, KB(1000)},\
}
#endif

typedef struct mem_blk {
    const char *name;
    size_t addr;
    size_t size;
} mem_blk_t;

typedef struct region {
    size_t base_addr;
    size_t size;
    uint8_t srd;
} region_t;

typedef struct mpu {
    region_t regions[MPU_REGIONS];
} mpu_t;

typedef enum region_size {
    SIZE_4GB = 0,
    SIZE_2GB,
    SIZE_1GB,
    SIZE_512MB,
    SIZE_256MB,
    SIZE_128MB,
    SIZE_64MB,
    SIZE_32MB,
    SIZE_16MB,
    SIZE_8MB,
    SIZE_4MB,
    SIZE_2MB,
    SIZE_1MB,
    SIZE_512KB,
    SIZE_256KB,
    SIZE_128KB,
    SIZE_64KB,
    SIZE_32KB,
    SIZE_16KB,
    SIZE_8KB,
    SIZE_4KB,
    SIZE_2KB,
    SIZE_1KB,
    SIZE_512B,
    SIZE_256B,
    SIZE_128B,
    SIZE_64B,
    SIZE_32B,
} region_size_t;

static const size_t alignment[] = {
    [SIZE_4GB] = GB(4),
    [SIZE_2GB] = GB(2),
    [SIZE_1GB] = GB(1),
    [SIZE_512MB] = MB(512),
    [SIZE_256MB] = MB(256),
    [SIZE_128MB] = MB(128),
    [SIZE_64MB] = MB(64),
    [SIZE_32MB] = MB(32),
    [SIZE_16MB] = MB(16),
    [SIZE_8MB] = MB(8),
    [SIZE_4MB] = MB(4),
    [SIZE_2MB] = MB(2),
    [SIZE_1MB] = MB(1),
    [SIZE_512KB] = KB(512),
    [SIZE_256KB] = KB(256),
    [SIZE_128KB] = KB(128),
    [SIZE_64KB] = KB(64),
    [SIZE_32KB] = KB(32),
    [SIZE_16KB] = KB(16),
    [SIZE_8KB] = KB(8),
    [SIZE_4KB] = KB(4),
    [SIZE_2KB] = KB(2),
    [SIZE_1KB] = KB(1),
    [SIZE_512B] = B(512),
    [SIZE_256B] = B(256),
    [SIZE_128B] = B(128),
    [SIZE_64B] = B(64),
    [SIZE_32B] = B(32),
};

static mem_blk_t memory[] = MEMORIES;

static size_t align_up(size_t x, size_t align)
{
    return (x + (align - 1)) & ~(align - 1);
}

static size_t align_down(size_t x, size_t align)
{
    return x - (x & (align - 1));
}

static bool is_aligned(size_t x, size_t align)
{
    return (0 == (x))         ? true  :
           (0 == (align))     ? false :
           (0 == (x % align)) ? true  : false;
}

static bool is_legal(size_t align)
{
    for (size_t i = 0; i < ARRAY_SIZE(alignment); i++)
    {
        if (align == alignment[i])
        {
            return true;
        }
    }

    return false;
}

static uint8_t calc_srd(size_t addr, size_t region_base, size_t region_size)
{
    assert(addr > 0);
    assert(0 == region_base % region_size);

    size_t srs = region_size / 8;
    size_t subregions = (addr - region_base) / srs;
    assert(subregions < 8/2);

    uint8_t srd = 0x00;
    for (size_t i = 0; i < subregions; i++)
    {
        srd |= (1 << i);
    }

    return srd;
}

/* return best align size in subregion disable mode */
static bool srd_fit(region_t *region, size_t addr, size_t limit)
{
    assert(region);

    for (size_t i = 0; i < ARRAY_SIZE(alignment); i++)
    {
        if (0 == addr && alignment[i] < limit)
        {
            region->base_addr = addr;
            region->size = addr + align_up(alignment[SIZE_32B], alignment[i]);
            region->srd = 0x00;

            return true;
        }

        if (addr && addr > alignment[i])
        {
            size_t quotient = addr / alignment[i];
            size_t srs = addr - quotient * alignment[i];
            size_t size = srs * 8;

            if (is_legal(size) && (align_down(addr, size) + size) <= limit)
            {
                region->base_addr = quotient * alignment[i];
                region->size = size;
                region->srd = calc_srd(addr, region->base_addr, region->size);

                return true;
            }
        }
    }

    region->base_addr = 0;
    region->size = 0;
    region->srd = 0xff;
    return false;
}

/* return best align size in standard mode */
static bool std_fit(region_t *region, size_t addr, size_t limit)
{
    assert(region);

    for (size_t i = 0; i < ARRAY_SIZE(alignment); i++)
    {
        if (0 == addr && alignment[i] < limit)
        {
            region->base_addr = addr;
            region->size = addr + align_up(alignment[SIZE_32B], alignment[i]);
            region->srd = 0x00;

            return true;
        }

        if (addr && addr >= alignment[i])
        {
            if (0 == addr % alignment[i] && addr + alignment[i] <= limit)
            {
                region->base_addr = addr;
                region->size = alignment[i];
                region->srd = 0x00;

                return true;
            }
        }
    }

    region->base_addr = 0;
    region->size = 0;
    region->srd = 0xff;
    return false;
}

static int32_t try_fit(mpu_t *mpu, mem_blk_t *mem)
{
    size_t addr = mem->addr;
    size_t addr_next = addr;
    size_t addr_end = mem->addr + mem->size;

    for (size_t i = 0; i < ARRAY_SIZE(mpu->regions); i++)
    {
        region_t srd = {0, 0, 0};
        region_t std = {0, 0, 0};

        if (false == srd_fit(&srd, addr, addr_end)
         && false == std_fit(&std, addr, addr_end))
        {
            return -1;
        }

        size_t srd_next = (srd.base_addr + srd.size);
        size_t std_next = (std.base_addr + std.size);
        if (srd_next > std_next)
        {
            addr_next = srd_next;
            memcpy(&mpu->regions[i], &srd, sizeof(region_t));

            DEBUG_LOG("[0x%08lx++0x%08lx] we choose srd: [0x%08lx++0x%08lx, 0x%02x]", addr, addr_end, srd.base_addr, srd_next, srd.srd);
        }
        else
        {
            addr_next = std_next;
            memcpy(&mpu->regions[i], &std, sizeof(region_t));

            DEBUG_LOG("[0x%08lx++0x%08lx] we choose std: [0x%08lx++0x%08lx, 0x%02x]", addr, addr_end, std.base_addr, std_next, std.srd);
        }

        if (addr_next < addr_end)
        {
            addr = addr_next;
        }
        else
        {
            /* return total count of used regions */
            return i + 1;
        }
    }

    return 0;
}

static int32_t merge_region(mpu_t *mpu, size_t num)
{
    if (num < 0)
    {
        return num;
    }

    return num;
}

static void report(int32_t num, mpu_t *mpu, mem_blk_t *mem)
{
    if (num < 0)
    {
        printf("[0x%08lx--0x%08lx] %s: failed to generate mpu layout!!\r\n", mem->addr, mem->size, mem->name);
    }
    else
    {
        for (size_t i = 0; i < num; i++)
        {
            region_t *region = &mpu->regions[i];
            printf("[0x%08lx--0x%08lx] %s[%zu]: [0x%08lx--0x%08lx, 0x%02x]\r\n", mem->addr, mem->size, mem->name, i,
                                                                                 region->base_addr, region->size, region->srd);
        }
    }

    if (num > 0)
    {
        printf("\r\n");
    }
    return;
}

int main (int argc, char **argv)
{
    mpu_t mpu;

    for (size_t i = 0; i < ARRAY_SIZE(memory); i++)
    {
        mem_blk_t *mem = &memory[i];

        memset(&mpu, 0x00, sizeof(mpu));

        report(merge_region(&mpu, try_fit(&mpu, mem)), &mpu, mem);
    }

    return 0;
}

#undef DEBUG_LOG
#undef ARRAY_SIZE
#undef KB
#undef MB
#undef GB
#undef MPU_REGIONS
#undef MEMORIES
