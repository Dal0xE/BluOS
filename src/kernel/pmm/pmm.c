#include <stdbool.h>
#include <kernel/assert.h>
#include <kernel/boot_info.h>
#include <kernel/address_space_bitmap.h>

#include <std/common.h>

#include "pmm.h"

void pmm_reserve_mem_region(pmm_state_t* pmm, uint32_t start, uint32_t size);

static uint32_t first_usable_pmm_index(pmm_state_t* pmm) {
    for (int i = 0; i < ADDRESS_SPACE_BITMAP_SIZE; i++) {
        uint32_t system_frames_entry = pmm->system_accessible_frames.set[i];
        //skip early if either of these entries are unusable
        //have all of these frames been reserved by system? (no bits set)
        if (!system_frames_entry) {
            continue;
        }

        uint32_t pmm_frames_entry = pmm->allocation_state.set[i];
        //have all these frames already been allocated by PMM? (all bits set)
        if (!(~pmm_frames_entry)) {
            continue;
        }

        //test each bit in both bitmaps
        //if we find a bit on in both bitmaps, we've found a usable index
        for (int j = 0; j < BITS_PER_BITMAP_ENTRY; j++) {
            //is this frame reserved by the system? (bit is off)
            if (!(system_frames_entry & (1 << j))) {
                continue;
            }
            //is this frame already allocated by PMM? (bit is on)
            if (pmm_frames_entry & (1 << j)) {
                continue;
            }
            //we found a bit which was on in the list of accessible frames,
            //and off in the list of allocated frames
            return BITMAP_BIT_INDEX(i, j);
        }
    }
    panic("first_usable_pmm_index() found nothing!");
    return 0;
}

static void set_memory_region(address_space_frame_bitmap_t* bitmap, uint32_t region_start_addr, uint32_t region_len) {
    if (region_start_addr % PAGING_FRAME_SIZE) {
        panic("region_start_addr wasn't page aligned");
    }
    if (region_len % PAGING_FRAME_SIZE) {
        panic("region_len wasn't frame aligned");
    }

    uint32_t frames_in_region = region_len / PAGING_FRAME_SIZE;
    for (int i = 0; i < frames_in_region; i++) {
        uint32_t frame = region_start_addr + (i * PAGING_FRAME_SIZE);
        addr_space_bitmap_set_address(bitmap, frame);
    }
}

static void unset_memory_region(address_space_frame_bitmap_t* bitmap, uint32_t region_start_addr, uint32_t region_len) {
    if (region_start_addr % PAGING_FRAME_SIZE) {
        panic("region_start_addr wasn't page aligned");
    }
    if (region_len % PAGING_FRAME_SIZE) {
        panic("region_len wasn't frame aligned");
    }

    uint32_t frames_in_region = region_len / PAGING_FRAME_SIZE;
    for (int i = 0; i < frames_in_region; i++) {
        uint32_t frame = region_start_addr + (i * PAGING_FRAME_SIZE);
        addr_space_bitmap_unset_address(bitmap, frame);
    }
}

pmm_state_t* pmm_get(void) {
    static pmm_state_t state = {0};
    return &state;
}

void pmm_dump(void) {
    pmm_state_t* pmm = pmm_get();
    printf("Physical memory manager state:\n");
    printf("\tSystem accessible frames (ranges are allocatable):\n");
    addr_space_bitmap_dump_set_ranges(&pmm->system_accessible_frames);
    printf("\tFrame allocation state (ranges are allocated):\n");
    addr_space_bitmap_dump_set_ranges(&pmm->allocation_state);
}

void pmm_init() {
    pmm_state_t* pmm = pmm_get();
    memset(pmm, 0, sizeof(pmm_state_t));

    boot_info_t* info = boot_info_get();
    //mark usable sections of the address space
    for (int i = 0; i < info->mem_region_count; i++) {
        physical_memory_region_t region = info->mem_regions[i];
        if (region.type != REGION_USABLE) {
            continue;
        }
        //mask to frame size
        //this cuts off a bit of usable memory but we'll only lose a few frames at most
        uint32_t addr = addr_space_frame_ceil(region.addr);
        uint32_t len = addr_space_frame_floor(region.len);
        set_memory_region(&(pmm->system_accessible_frames), addr, len);
    }

    //for identity mapping purposes
    //reserve any allocatable memory from 0x0 to start of kernel image
    pmm_reserve_mem_region(pmm, 0x00000000, info->kernel_image_start);
    //map out kernel image region
    pmm_reserve_mem_region(pmm, info->kernel_image_start, info->kernel_image_size);
    //give extra region for an identity_map_buffer
    uint32_t extra_identity_map_region_size = 0x100000;
    uint32_t kernel_max = info->kernel_image_start + info->kernel_image_size;
    pmm_reserve_mem_region(pmm, kernel_max, extra_identity_map_region_size);
    //map out framebuffer
    pmm_reserve_mem_region(pmm, info->framebuffer.address, info->framebuffer.size);
}

//marks a block of physical memory as unallocatable
//the size does not need to be frame-aligned, it will be aligned to the next largest frame
void pmm_reserve_mem_region(pmm_state_t* pmm, uint32_t start, uint32_t size) {
    uint32_t aligned_start = addr_space_frame_floor(start);
    uint32_t aligned_size = addr_space_frame_ceil(size);
    unset_memory_region(&(pmm->system_accessible_frames), aligned_start, aligned_size);
}

void pmm_alloc_address(uint32_t address) {
    pmm_state_t* pmm = pmm_get();
    //has this frame already been alloc'd?
    if (addr_space_bitmap_check_address(&pmm->allocation_state, address)) {
        printf("frame 0x%08x was alloc'd twice\n", address);
        panic("PMM double alloc");
    }
    addr_space_bitmap_set_address(&pmm->allocation_state, address);
}

uint32_t pmm_alloc(void) {
    pmm_state_t* pmm = pmm_get();
    uint32_t index = first_usable_pmm_index(pmm);
    uint32_t frame_address = index * PAGING_FRAME_SIZE;
    pmm_alloc_address(frame_address);
    return frame_address;
}


void pmm_free(uint32_t frame_address) {
    pmm_state_t* pmm = pmm_get();
    //sanity check
    if (!addr_space_bitmap_check_address(&pmm->allocation_state, frame_address)) {
        panic("attempted to free non-allocated frame");
    }
    addr_space_bitmap_unset_address(&pmm->allocation_state, frame_address);
}
