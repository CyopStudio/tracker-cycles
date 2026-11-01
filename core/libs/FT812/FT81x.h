/*
 *******************************************************************************
 *            FT81x simple library for PIC MCU                                 *
 *******************************************************************************
Copyright (c) Future Technology Devices International

THIS SOFTWARE IS PROVIDED BY FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

FTDI DRIVERS MAY BE USED ONLY IN CONJUNCTION WITH PRODUCTS BASED ON FTDI PARTS.

FTDI DRIVERS MAY BE DISTRIBUTED IN ANY FORM AS LONG AS LICENSE INFORMATION IS NOT MODIFIED.

IF A CUSTOM VENDOR ID AND/OR PRODUCT ID OR DESCRIPTION STRING ARE USED, IT IS THE
RESPONSIBILITY OF THE PRODUCT MANUFACTURER TO MAINTAIN ANY CHANGES AND SUBSEQUENT WHQL
RE-CERTIFICATION AS A RESULT OF MAKING THESE CHANGES.

Author : FTDI
*/

#ifndef FT8XX_H
#define FT8XX_H

#include <stdint.h>

#define FT_81X_ENABLE 1

/* Definitions used for co-processor command buffer */
#define FT_DL_SIZE       (8 * 1024L) // 8KB Display List buffer size
#define FT_CMD_FIFO_SIZE (4 * 1024L) // 4KB coprocessor Fifo size
#define FT_CMD_SIZE      (4)         // 4 byte per coprocessor command of EVE

#define RAM_DL 3145728UL

/* For FT801, FT811 and FT813 */
#define CTOUCH_MODE_COMPATIBILITY 1
#define CTOUCH_MODE_EXTENDED      0

#define LOW_FREQ_BOUND 58800000L // 98% of 60Mhz

#define RAM_G_SIZE    1024 * 1024L
#define FT81X_VERSION "1.0.4"

#define ADC_DIFFERENTIAL 1UL
#define ADC_SINGLE_ENDED 0UL
#define ADPCM_SAMPLES    2UL
#define ALWAYS           7UL
#define ARGB1555         0UL
#define ARGB2            5UL
#define ARGB4            6UL
#define BARGRAPH         11UL
#define BILINEAR         1UL
#define BITMAPS          1UL
#define BORDER           0UL
#define CMDBUF_SIZE      4096UL

#define DECR                4UL
#define DST_ALPHA           3UL
#define EDGE_STRIP_A        7UL
#define EDGE_STRIP_B        8UL
#define EDGE_STRIP_L        6UL
#define EDGE_STRIP_R        5UL
#define EQUAL               5UL
#define GEQUAL              4UL
#define GREATER             3UL
#define INCR                3UL
#define INT_CMDEMPTY        32UL
#define INT_CMDFLAG         64UL
#define INT_CONVCOMPLETE    128UL
#define INT_G8              18UL
#define INT_L8C             12UL
#define INT_PLAYBACK        16UL
#define INT_SOUND           8UL
#define INT_SWAP            1UL
#define INT_TAG             4UL
#define INT_TOUCH           2UL
#define INT_VGA             13UL
#define INVERT              5UL
#define KEEP                1UL
#define L1                  1UL
#define L2                  17UL
#define L4                  2UL
#define L8                  3UL
#define LEQUAL              2UL
#define LESS                1UL
#define LINEAR_SAMPLES      0UL
#define LINES               3UL
#define LINE_STRIP          4UL
#define NEAREST             0UL
#define NEVER               0UL
#define NOTEQUAL            6UL
#define ONE                 1UL
#define ONE_MINUS_DST_ALPHA 5UL
#define ONE_MINUS_SRC_ALPHA 4UL
#define OPT_CENTER          1536UL
#define OPT_CENTERX         512UL
#define OPT_CENTERY         1024UL
#define OPT_FLAT            256UL
#define OPT_FULLSCREEN      8UL
#define OPT_MEDIAFIFO       16UL
#define OPT_MONO            1UL
#define OPT_NOBACK          4096UL
#define OPT_NODL            2UL
#define OPT_NOHANDS         49152UL
#define OPT_NOHM            16384UL
#define OPT_NOPOINTER       16384UL
#define OPT_NOSECS          32768UL
#define OPT_NOTEAR          4UL
#define OPT_NOTICKS         8192UL
#define OPT_RIGHTX          2048UL
#define OPT_SIGNED          256UL
#define OPT_SOUND           32UL
#define PALETTED            8UL
#define PALETTED4444        15UL
#define PALETTED565         14UL
#define PALETTED8           16UL
#define FTPOINTS            2UL
#define RECTS               9UL

#define REPEAT               1UL
#define REPLACE              2UL
#define RGB332               4UL
#define RGB565               7UL
#define SRC_ALPHA            2UL
#define TEXT8X8              9UL
#define TEXTVGA              10UL
#define TOUCHMODE_CONTINUOUS 3UL
#define TOUCHMODE_FRAME      2UL
#define TOUCHMODE_OFF        0UL
#define TOUCHMODE_ONESHOT    1UL
#define ULAW_SAMPLES         1UL
#define ZERO                 0UL

#define FT_GPU_NUMCHAR_PERFONT (128)
#define FT_GPU_FONT_TABLE_SIZE (148)

#define CMD_APPEND           4294967070UL
#define CMD_BGCOLOR          4294967049UL
#define CMD_BITMAP_TRANSFORM 4294967073UL
#define CMD_BUTTON           4294967053UL
#define CMD_CALIBRATE        4294967061UL
#define CMD_CLOCK            4294967060UL
#define CMD_COLDSTART        4294967090UL
#define CMD_CRC              4294967043UL
#define CMD_CSKETCH          4294967093UL
#define CMD_DIAL             4294967085UL
#define CMD_DLSTART          4294967040UL
#define CMD_EXECUTE          4294967047UL
#define CMD_FGCOLOR          4294967050UL
#define CMD_GAUGE            4294967059UL
#define CMD_GETMATRIX        4294967091UL
#define CMD_GETPOINT         4294967048UL
#define CMD_GETPROPS         4294967077UL
#define CMD_GETPTR           4294967075UL
#define CMD_GRADCOLOR        4294967092UL
#define CMD_GRADIENT         4294967051UL
#define CMD_HAMMERAUX        4294967044UL
#define CMD_IDCT_DELETED     4294967046UL
#define CMD_INFLATE          4294967074UL
#define CMD_INTERRUPT        4294967042UL
#define CMD_INT_RAMSHARED    4294967101UL
#define CMD_INT_SWLOADIMAGE  4294967102UL
#define CMD_KEYS             4294967054UL
#define CMD_LOADIDENTITY     4294967078UL
#define CMD_LOADIMAGE        4294967076UL
#define CMD_LOGO             4294967089UL
#define CMD_MARCH            4294967045UL
#define CMD_MEDIAFIFO        4294967097UL
#define CMD_MEMCPY           4294967069UL
#define CMD_MEMCRC           4294967064UL
#define CMD_MEMSET           4294967067UL
#define CMD_MEMWRITE         4294967066UL
#define CMD_MEMZERO          4294967068UL
#define CMD_NUMBER           4294967086UL
#define CMD_PLAYVIDEO        4294967098UL
#define CMD_PROGRESS         4294967055UL
#define CMD_REGREAD          4294967065UL
#define CMD_ROMFONT          4294967103UL
#define CMD_ROTATE           4294967081UL
#define CMD_SCALE            4294967080UL
#define CMD_SCREENSAVER      4294967087UL
#define CMD_SCROLLBAR        4294967057UL
#define CMD_SETBASE          4294967096UL
#define CMD_SETBITMAP        4294967107UL
#define CMD_SETFONT          4294967083UL
#define CMD_SETFONT2         4294967099UL
#define CMD_SETMATRIX        4294967082UL
#define CMD_SETROTATE        4294967094UL
#define CMD_SETSCRATCH       4294967100UL
#define CMD_SKETCH           4294967088UL
#define CMD_SLIDER           4294967056UL
#define CMD_SNAPSHOT         4294967071UL
#define CMD_SNAPSHOT2        4294967095UL
#define CMD_SPINNER          4294967062UL
#define CMD_STOP             4294967063UL
#define CMD_SWAP             4294967041UL
#define CMD_SYNC             4294967106UL
#define CMD_TEXT             4294967052UL
#define CMD_TOGGLE           4294967058UL
#define CMD_TOUCH_TRANSFORM  4294967072UL
#define CMD_TRACK            4294967084UL
#define CMD_TRANSLATE        4294967079UL
#define CMD_VIDEOFRAME       4294967105UL
#define CMD_VIDEOSTART       4294967104UL

#define VERTEX2F(x, y) ((1UL << 30) | (((x)&32767UL) << 15) | (((y)&32767UL) << 0))
#define VERTEX2II(x, y, handle, cell)                                                              \
    ((2UL << 30) | (((x)&511UL) << 21) | (((y)&511UL) << 12) | (((handle)&31UL) << 7) |            \
     (((cell)&127UL) << 0))
#define BITMAP_SOURCE(addr) ((1UL << 24) | (((addr)&4194303UL) << 0))
#define CLEAR_COLOR_RGB(red, green, blue)                                                          \
    ((2UL << 24) | (((red)&255UL) << 16) | (((green)&255UL) << 8) | (((blue)&255UL) << 0))
#define TAG(s) ((3UL << 24) | (((s)&255UL) << 0))
#define COLOR_RGB(red, green, blue)                                                                \
    ((4UL << 24) | (((red)&255UL) << 16) | (((green)&255UL) << 8) | (((blue)&255UL) << 0))
#define BITMAP_HANDLE(handle) ((5UL << 24) | (((handle)&31UL) << 0))
#define CELL(cell) ((6UL << 24) | (((cell)&127UL) << 0))
#define BITMAP_LAYOUT(format, linestride, height)                                                  \
    ((7UL << 24) | (((format)&31UL) << 19) | (((linestride)&1023UL) << 9) | (((height)&511UL) << 0))
#define BITMAP_SIZE(filter, wrapx, wrapy, width, height)                                           \
    ((8UL << 24) | (((filter)&1UL) << 20) | (((wrapx)&1UL) << 19) | (((wrapy)&1UL) << 18) |        \
     (((width)&511UL) << 9) | (((height)&511UL) << 0))
#define ALPHA_FUNC(func, ref) ((9UL << 24) | (((func)&7UL) << 8) | (((ref)&255UL) << 0))
#define STENCIL_FUNC(func, ref, mask)                                                              \
    ((10UL << 24) | (((func)&7UL) << 16) | (((ref)&255UL) << 8) | (((mask)&255UL) << 0))
#define BLEND_FUNC(src, dst) ((11UL << 24) | (((src)&7UL) << 3) | (((dst)&7UL) << 0))
#define STENCIL_OP(sfail, spass) ((12UL << 24) | (((sfail)&7UL) << 3) | (((spass)&7UL) << 0))
#define POINT_SIZE(size) ((13UL << 24) | (((size)&8191UL) << 0))
#define LINE_WIDTH(width) ((14UL << 24) | (((width)&4095UL) << 0))
#define CLEAR_COLOR_A(alpha) ((15UL << 24) | (((alpha)&255UL) << 0))
#define COLOR_A(alpha) ((16UL << 24) | (((alpha)&255UL) << 0))
#define CLEAR_STENCIL(s) ((17UL << 24) | (((s)&255UL) << 0))
#define CLEAR_TAG(s) ((18UL << 24) | (((s)&255UL) << 0))
#define STENCIL_MASK(mask) ((19UL << 24) | (((mask)&255UL) << 0))
#define TAG_MASK(mask) ((20UL << 24) | (((mask)&1UL) << 0))
#define BITMAP_TRANSFORM_A(a) ((21UL << 24) | (((a)&131071UL) << 0))
#define BITMAP_TRANSFORM_B(b) ((22UL << 24) | (((b)&131071UL) << 0))
#define BITMAP_TRANSFORM_C(c) ((23UL << 24) | (((c)&16777215UL) << 0))
#define BITMAP_TRANSFORM_D(d) ((24UL << 24) | (((d)&131071UL) << 0))
#define BITMAP_TRANSFORM_E(e) ((25UL << 24) | (((e)&131071UL) << 0))
#define BITMAP_TRANSFORM_F(f) ((26UL << 24) | (((f)&16777215UL) << 0))
#define SCISSOR_XY(x, y) ((27UL << 24) | (((x)&2047UL) << 11) | (((y)&2047UL) << 0))
#define SCISSOR_SIZE(width, height)                                                                \
    ((28UL << 24) | (((width)&4095UL) << 12) | (((height)&4095UL) << 0))
#define CALL(dest) ((29UL << 24) | (((dest)&65535UL) << 0))
#define JUMP(dest) ((30UL << 24) | (((dest)&65535UL) << 0))
#define BEGIN(prim) ((31UL << 24) | (((prim)&15UL) << 0))
#define COLOR_MASK(r, g, b, a)                                                                     \
    ((32UL << 24) | (((r)&1UL) << 3) | (((g)&1UL) << 2) | (((b)&1UL) << 1) | (((a)&1UL) << 0))
#define CLEAR(c, s, t) ((38UL << 24) | (((c)&1UL) << 2) | (((s)&1UL) << 1) | (((t)&1UL) << 0))
#define VERTEX_FORMAT(frac) ((39UL << 24) | (((frac)&7UL) << 0))
#define BITMAP_LAYOUT_H(linestride, height)                                                        \
    ((40UL << 24) | (((linestride)&3UL) << 2) | (((height)&3UL) << 0))
#define BITMAP_SIZE_H(width, height) ((41UL << 24) | (((width)&3UL) << 2) | (((height)&3UL) << 0))
#define PALETTE_SOURCE(addr) ((42UL << 24) | (((addr)&4194303UL) << 0))
#define VERTEX_TRANSLATE_X(x) ((43UL << 24) | (((x)&131071UL) << 0))
#define VERTEX_TRANSLATE_Y(y) ((44UL << 24) | (((y)&131071UL) << 0))
#define NOP() ((45UL << 24))
#define END() ((33UL << 24))
#define SAVE_CONTEXT() ((34UL << 24))
#define RESTORE_CONTEXT() ((35UL << 24))
#define RETURN() ((36UL << 24))
#define MACRO(m) ((37UL << 24) | (((m)&1UL) << 0))
#define FT_DISPLAY() ((0UL << 24))
/* Nothing beyond this */

#endif /* FT8XX_H */
