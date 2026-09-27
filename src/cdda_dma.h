#pragma once
#include <stddef.h>
#include <stdint.h>

#define CDDA_DMA_DONE 1
#define CDDA_DMA_MARK 2
/* C400 register file, raw 2352-byte stereo CDDA sector, physical RAM mapping.
 * Return0 if either ring is invalid (no writes). DONE plus MARK if the left
 * cursor crosses +80. Source byte order is preserved for each 16-bit sample. */
int cdda_dma_sector(uint32_t *regs, const uint8_t raw[2352],
                    uint8_t *ram, size_t ram_size, uint32_t ram_base);
