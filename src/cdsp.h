#pragma once
#include <stdint.h>

struct CDROM;

/* The CD servo/signal-processor chip on the far end of the 0xC4000000 serial
 * bus, and the disc under it. The command set follows Sony's CD DSPs
 * (CXD2545 family): the top nibble of each command is the register address,
 * and the SENS pin shows a status signal chosen by the last address written.
 *
 * Modelled behaviourally: servos lock as soon as they are switched on, and
 * the pickup's radius is tracked so that sled moves, track jumps and the
 * innermost-track switch agree with the subcode the disc returns. The disc
 * is a CLV spiral (1.3 m/s, 1.6 µm pitch) whose program area starts at
 * 25 mm; the lead-in lies inside that and the table of contents comes from
 * the image's cue sheet. */

typedef struct CDSP {
    struct CDROM *disc;
    uint8_t  sel;          /* first byte of the last command: picks SENS */
    uint8_t  tmode;        /* $2X data: bits 3-2 tracking, 1-0 sled */
    uint8_t  func;         /* $9X data 1: DCLV, DSPB (double speed), ASEQ, DPLL */
    uint8_t  spindle_mode; /* $EX data 2/3: spindle clock/control mode */
    uint8_t  velocity;     /* $DX data 2/3: VP7..0 */
    uint8_t  velocity_mul; /* $DX data 4: VPCTL1..0 */
    uint8_t  clock_config; /* extended $AE0011xx clock configuration */
    int      focused;      /* FOK */
    int      busy;         /* auto sequence running (XBUSY low) */
    uint64_t busy_until;   /* µs */
    uint32_t n;            /* $7X auto-sequence track count */
    uint32_t bcount;       /* $BX traverse monitor count */
    uint64_t comp_fall;    /* µs when COMP falls (bcount tracks traversed); 0 = high */
    double   traversed;    /* tracks crossed by sled moves since $BX was written */
    int      toggle;       /* FZC/TZC/COUT source */
    double   radius;       /* pickup, mm from the disc centre */
    double   lba;          /* sector under the pickup (follows radius) */
    uint64_t last_us;
    uint32_t cmds;
} CDSP;

extern int cdsp_log;   /* CD logging on (VFLASH_CDLOG=<from frame>) */
void     cdsp_reset(CDSP *d, struct CDROM *disc);
void     cdsp_command(CDSP *d, uint64_t now_us, uint32_t bits, uint64_t data);
int      cdsp_sens(CDSP *d, uint64_t now_us);
uint32_t cdsp_readback(CDSP *d, uint64_t now_us, uint32_t bits, uint64_t data, uint32_t rbits);

/* The disc side. cdsp_reading: the pickup is focused on a recorded area,
 * so frames (and subcode) come off the disc; cdsp_lba is then the sector
 * under it. cdsp_speed: playback rate relative to 75 sectors/s, including
 * the programmable CAV-W rate (which need not be an integer). */
void     cdsp_advance(CDSP *d, uint64_t now_us);
int      cdsp_reading(CDSP *d);
int32_t  cdsp_lba(CDSP *d);
double   cdsp_speed(CDSP *d);
/* Q channel for a sector, 10 bytes BCD: control/ADR, track, index, relative
 * M:S:F, 0, absolute M:S:F (lead-in: point and its M:S:F instead). */
void     cdsp_subq(CDSP *d, int32_t lba, uint8_t q[10]);
