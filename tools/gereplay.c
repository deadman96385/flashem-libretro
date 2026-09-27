/* gereplay: run a VFLASH_GECAP capture through the current src/ge.c and
 * write the surface as a PPM - GE work without booting the machine.
 *
 *   gereplay <capture> <out.ppm> [x y w h]
 *
 * The default region is the whole surface from y = 240 to 720 (it holds the
 * three display buffers: (0,480), (512,480), (512,240)). Environment:
 * VFLASH_GETRACE=1 traces every command, VFLASH_GELOG=1 reports unknown
 * opcodes, VFLASH_NO3D / VFLASH_NODEPTH as in the emulator,
 * GEREPLAY_LISTS=a-b runs only lists a..b (0-based; RAM still follows),
 * GEREPLAY_RAM=<file> writes RAM as it is at the end (for tools/tex.py). */
#include "../src/ge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAM_SIZE (16u * 1024 * 1024)

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: gereplay <capture> <out.ppm> [x y w h]\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    char magic[6];
    uint32_t sz;
    static GE g;
    if (fread(magic, 1, 6, f) != 6 || memcmp(magic, "GECAP2", 6) || fread(&sz, 4, 1, f) != 1) {
        fprintf(stderr, "not a GE capture\n");
        return 1;
    }
    if (sz != sizeof(GE)) {
        fprintf(stderr, "capture has GE size %u, this build %zu: recapture after changing ge.h\n",
                sz, sizeof(GE));
        return 1;
    }
    if (fread(&g, sizeof g, 1, f) != 1) return 1;
    g.ram = calloc(1, RAM_SIZE);
    g.ram_size = RAM_SIZE;
    g.log = getenv("VFLASH_GELOG") != NULL;
    g.trace = getenv("VFLASH_GETRACE") != NULL;
    g.no3d = getenv("VFLASH_NO3D") != NULL;
    g.nodepth = getenv("VFLASH_NODEPTH") != NULL;
    memset(g.unknown, 0, sizeof g.unknown);
    g.sprites = g.pixels = g.lists = g.fills = g.calls = g.tris = 0;
    int la = 0, lb = 1 << 30;
    if (getenv("GEREPLAY_LISTS")) sscanf(getenv("GEREPLAY_LISTS"), "%d-%d", &la, &lb);

    int c, n = 0;
    uint32_t w[4];
    while ((c = fgetc(f)) != EOF && c != 'E') {
        if (c == 'P') {
            uint32_t a;
            if (fread(&a, 4, 1, f) != 1 || a - 0x10000000u > RAM_SIZE - 4096) break;
            if (fread(g.ram + (a - 0x10000000u), 1, 4096, f) != 4096) break;
        } else if (c == 'L') {
            if (fread(w, 4, 4, f) != 4) break;
            if (n > lb) break;
            if (n >= la && n <= lb) {
                g.surface = w[1];
                g.top = (int)w[2];
                if (g.trace) printf("[GET] list %d at %08X surface %08X top %u frame %u\n", n, w[0], w[1], w[2], w[3]);
                ge_run(&g, w[0]);
            }
            n++;
        } else {
            fprintf(stderr, "bad record %02X\n", c);
            return 1;
        }
    }
    fclose(f);
    if (getenv("GEREPLAY_RAM")) {
        FILE *r = fopen(getenv("GEREPLAY_RAM"), "wb");
        if (r) { fwrite(g.ram, 1, RAM_SIZE, r); fclose(r); }
    }

    int x0 = 0, y0 = 240, W = 1024, H = 480;
    if (argc >= 7) { x0 = atoi(argv[3]); y0 = atoi(argv[4]); W = atoi(argv[5]); H = atoi(argv[6]); }
    FILE *o = fopen(argv[2], "wb");
    if (!o) { perror(argv[2]); return 1; }
    fprintf(o, "P6\n%d %d\n255\n", W, H);
    for (int y = y0; y < y0 + H; y++)
        for (int x = x0; x < x0 + W; x++) {
            /* the same tiled layout as px() in ge.c */
            uint32_t a = g.surface + (uint32_t)(y >> 3) * 16384 + (uint32_t)(x >> 5) * 512 +
                         (uint32_t)(y & 7) * 64 + (uint32_t)(x & 31) * 2 - 0x10000000u;
            uint16_t p = (a < RAM_SIZE - 1) ? (uint16_t)(g.ram[a] | g.ram[a + 1] << 8) : 0;
            uint8_t rgb[3] = { (uint8_t)((p & 31) << 3), (uint8_t)((p >> 5 & 31) << 3), (uint8_t)((p >> 10 & 31) << 3) };
            fwrite(rgb, 1, 3, o);
        }
    fclose(o);
    fprintf(stderr, "%d lists: %u sprites, %u fills, %u calls, %u tris, %u pixels\n",
            n, g.sprites, g.fills, g.calls, g.tris, g.pixels);
    return 0;
}
