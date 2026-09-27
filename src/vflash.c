/* The frontend-facing side of the emulator: finds the boot ROM, opens the
 * disc and hands everything else to the machine in hw.c. */
#include "vflash.h"
#include "arm9.h"
#include "cdrom.h"
#include "audio.h"
#include "hw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ROM_MAX (2u * 1024 * 1024)

struct VFlash {
    ARM9      cpu;
    CDROM    *cd;
    Audio    *audio;
    HW       *hw;
    uint8_t  *rom;
    uint32_t  rom_size;
    uint32_t  framebuf[VFLASH_FB_MAX_W * VFLASH_FB_MAX_H];
};

static char s_bios_dir[1024];
static void push_hardware_audio(void *ctx,const int16_t *samples,uint32_t count) {
    audio_push_samples((Audio *)ctx,samples,count);
}

void vflash_set_bios_dir(const char *dir) {
    if (!dir) { s_bios_dir[0] = '\0'; return; }
    snprintf(s_bios_dir, sizeof(s_bios_dir), "%s", dir);
}

/* The directory a frontend handed us (RetroArch's system directory), then
 * $FLASHEM_BIOS, then beside the binary for a standalone run. */
static int load_rom(VFlash *vf) {
    char sys_sub[1100], sys_rom[1100];
    const char *paths[5];
    int n = 0;

    if (s_bios_dir[0]) {
        snprintf(sys_sub, sizeof(sys_sub), "%s/flashem/70004.bin", s_bios_dir);
        snprintf(sys_rom, sizeof(sys_rom), "%s/70004.bin", s_bios_dir);
        paths[n++] = sys_sub;
        paths[n++] = sys_rom;
    }
    if (getenv("FLASHEM_BIOS"))
        paths[n++] = getenv("FLASHEM_BIOS");
    paths[n++] = "70004.bin";
    paths[n] = NULL;

    for (int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && (unsigned long)sz <= ROM_MAX) {
            vf->rom = malloc((size_t)sz);
            if (vf->rom && fread(vf->rom, 1, (size_t)sz, f) == (size_t)sz) {
                vf->rom_size = (uint32_t)sz;
                fclose(f);
                printf("[VFlash] Boot ROM: %s (%ld bytes)\n", paths[i], sz);
                return 1;
            }
            free(vf->rom);
            vf->rom = NULL;
        }
        fclose(f);
    }
    return 0;
}

VFlash *vflash_create(const char *disc_path) {
    VFlash *vf = calloc(1, sizeof(VFlash));
    if (!vf) return NULL;
    vf->cd    = cdrom_create();
    vf->audio = audio_create();

    if (!load_rom(vf)) {
        fprintf(stderr, "[VFlash] Boot ROM 70004.bin not found (system directory, "
                        "$FLASHEM_BIOS or the working directory)\n");
        vflash_destroy(vf);
        return NULL;
    }
    if (disc_path && !cdrom_open(vf->cd, disc_path)) {
        fprintf(stderr, "[VFlash] Failed to open disc: %s\n", disc_path);
        vflash_destroy(vf);
        return NULL;
    }
    vf->hw = hw_create(&vf->cpu, vf->rom, vf->rom_size, vf->cd, vf->framebuf);
    hw_set_audio_sink(vf->hw,vf->audio,push_hardware_audio);
    return vf;
}

void vflash_destroy(VFlash *vf) {
    if (!vf) return;
    hw_destroy(vf->hw);
    cdrom_destroy(vf->cd);
    audio_destroy(vf->audio);
    free(vf->rom);
    free(vf);
}

void      vflash_run_frame(VFlash *vf)                  { hw_run_frame(vf->hw); }

int vflash_fast_booting(VFlash *vf) {
    static int on = -1;
    if (on < 0) on = !getenv("VFLASH_FASTBOOT") || atoi(getenv("VFLASH_FASTBOOT")) != 0;
    return on && hw_booting(vf->hw);
}
void      vflash_set_input(VFlash *vf, uint32_t b)      { hw_set_input(vf->hw, b); }
uint32_t *vflash_get_framebuffer(VFlash *vf)            { return vf->framebuf; }
void      vflash_get_screen_size(VFlash *vf, int *w, int *h) { hw_screen_size(vf->hw, w, h); }
void     *vflash_get_audio(VFlash *vf)                  { return vf ? vf->audio : NULL; }
void      vflash_init_audio(VFlash *vf)                 { audio_init_sdl(vf->audio); }
void      vflash_set_debug(VFlash *vf, int on)          { (void)vf; (void)on; }

/* ---- Debugger API ---- */

uint32_t vflash_get_pc(VFlash *vf)         { return vf->cpu.r[15]; }
uint32_t vflash_get_reg(VFlash *vf, int r) { return (r >= 0 && r < 16) ? vf->cpu.r[r] : 0; }
void     vflash_set_reg(VFlash *vf, int r, uint32_t val) { if (r >= 0 && r < 16) vf->cpu.r[r] = val; }
uint32_t vflash_get_cpsr(VFlash *vf)       { return vf->cpu.cpsr; }
int      vflash_is_thumb(VFlash *vf)       { return (vf->cpu.cpsr >> 5) & 1; }

uint32_t vflash_read32(VFlash *vf, uint32_t a)             { return hw_read32(vf->hw, a); }
uint8_t  vflash_read8(VFlash *vf, uint32_t a)              { return hw_read8(vf->hw, a); }
void     vflash_write32(VFlash *vf, uint32_t a, uint32_t v) { hw_write32(vf->hw, a, v); }

int      vflash_step(VFlash *vf)                          { return hw_step(vf->hw); }
void     vflash_bp_set(VFlash *vf, uint32_t addr)         { hw_bp_set(vf->hw, addr); }
void     vflash_bp_clear(VFlash *vf, uint32_t addr)       { hw_bp_clear(vf->hw, addr); }
void     vflash_bp_clear_all(VFlash *vf)                  { hw_bp_clear_all(vf->hw); }
int      vflash_bp_hit(VFlash *vf)                        { return hw_bp_hit(vf->hw); }
uint32_t vflash_bp_list(VFlash *vf, uint32_t *out, int n) { return hw_bp_list(vf->hw, out, n); }
