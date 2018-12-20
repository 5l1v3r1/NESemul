#include "common.h"
#include "ram.h"
#include "cpu.h"
#include "ppu.h"
#include "window.h"

int p_x, p_y;

struct ppu ppu;


const struct color ppu_palette[0x40] = {
#define CLR(idx, red, green, blue) [(idx)] = {	\
		.r = (red), .g = (green), .b = blue },

	CLR(0x00, 84, 84, 84)
	CLR(0x01, 0, 30, 116)
	CLR(0x02, 8, 16, 144)
	CLR(0x03, 48, 0, 136)
	CLR(0x04, 68, 0, 100)
	CLR(0x05, 92, 0, 48)
	CLR(0x06, 84, 4, 0)
	CLR(0x07, 60, 24, 0)
	CLR(0x08, 32, 42, 0)
	CLR(0x09, 8, 58, 0)
	CLR(0x0a, 0, 64, 0)
	CLR(0x0b, 0, 60, 0)
	CLR(0x0c, 0, 50, 60)
	CLR(0x0d, 0, 0, 0)
	CLR(0x0e, 0, 0, 0)
	CLR(0x0f, 0, 0, 0)

	CLR(0x10, 152, 150, 152)
	CLR(0x11, 8, 76, 196)
	CLR(0x12, 48, 50, 236)
	CLR(0x13, 92, 30, 228)
	CLR(0x14, 136, 20, 176)
	CLR(0x15, 160, 20, 100)
	CLR(0x16, 152, 34, 32)
	CLR(0x17, 120, 60, 0)
	CLR(0x18, 84, 90, 0)
	CLR(0x19, 40, 114, 0)
	CLR(0x1a, 8, 124, 0)
	CLR(0x1b, 0, 118, 40)
	CLR(0x1c, 0, 102, 120)
	CLR(0x1d, 0, 0, 0)
	CLR(0x1e, 0, 0, 0)
	CLR(0x1f, 0, 0, 0)

	CLR(0x20, 236, 238, 236)
	CLR(0x21, 76, 154, 236)
	CLR(0x22, 120, 124, 236)
	CLR(0x23, 176, 98, 236)
	CLR(0x24, 228, 84, 236)
	CLR(0x25, 236, 88, 180)
	CLR(0x26, 236, 106, 100)
	CLR(0x27, 212, 136, 32)
	CLR(0x28, 160, 170, 0)
	CLR(0x29, 116, 196, 0)
	CLR(0x2a, 76, 208, 32)
	CLR(0x2b, 56, 204, 108)
	CLR(0x2c, 56, 180, 204)
	CLR(0x2d, 60, 60, 60)
	CLR(0x2e, 0, 0, 0)
	CLR(0x2f, 0, 0, 0)

	CLR(0x30, 236, 238, 236)
	CLR(0x31, 76, 154, 236)
	CLR(0x32, 120, 124, 236)
	CLR(0x33, 176, 98, 236)
	CLR(0x34, 228, 84, 236)
	CLR(0x35, 236, 88, 180)
	CLR(0x36, 236, 106, 100)
	CLR(0x37, 212, 136, 32)
	CLR(0x38, 160, 170, 0)
	CLR(0x39, 116, 196, 0)
	CLR(0x3a, 76, 208, 32)
	CLR(0x3b, 56, 204, 108)
	CLR(0x3c, 56, 180, 204)
	CLR(0x3d, 60, 60, 60)
	CLR(0x3e, 60, 60, 60)
	CLR(0x3f, 60, 60, 60)
};
#undef CLR


int color_tbl[256][256][8];
byte ppu_sprite_hit_occured;

int bg_trans[256][256];
int bg_tiles[30][32];
int bg_pals[15][16];


byte
ppu_reg_get(word addr)
{
	byte ret;

	if (addr < 0x4000)
		addr = 0x2000 + (addr - 0x2000) % 8;

	printf("Accessing %04x register\n", addr);

	switch (addr) {
	case PPUSTATUS:
		ppu.scroll = ppu.addr = 0;
		ret = ppu.PPUSTATUS;

		/* In vblank = False */
		ppu.PPUSTATUS &= ~(1 << 7);
		/* In Sprite-0 hit = False */
		ppu.PPUSTATUS &= ~(1 << 6);

		return ret;
	case PPUDATA:
		ret = ppu_getb(ppu.PPUADDR);
		ppu.PPUADDR += (ppu.PPUCTRL & (1 << 2)) ? 32 : 1;

		return ret;
	case OAMDATA:
		return ppu.spr_ram[ppu.OAMADDR++];
	/* Joystick 1 */
	case 0x4016:
		return joy1_read_state();
	/* If some WO register or invalide register */
	default:
		//todo();
		return 0;
	}
}

void
ppu_load_spr_ram(word addr)
{
	int i;

	for (i = 0; i < 0x100; i++)
		ppu.spr_ram[i] = ram_getb(addr + i);
}

word
ppu_get_addr(word a)
{
	a %= 0x4000;

	/* Mirroring */
	if (0x3f20 <= a)
		a = 0x3f00 + a % 0x20;

	if (a == 0x3f10 || a == 0x3f14 || a == 0x3f18 || a == 0x3f1c)
		a -= 0x10;

	/* Mirroring */
	if (0x3000 <= a && a < 0x3f00)
		a = 0x2000 + a % 0x1000;

	/* If not a nametable */
	if (!(0x2000 <= a && a < 0x3000))
		return a;

	/* Some nametable mirroring */
	if (ppu.vmap) {
		a = 0x2000 + a % 0x800;

	} else {
		if (0x2000 <= a && a < 0x2800)
			a = 0x2000 + a % 0x400;
		else
			a = 0x2800 + a % 0x400;
	}

	return a;
}

void
ppu_setb(word a, byte b)
{
	a = ppu_get_addr(a);

	printf("Real addr equals to %04x!\n", a);

	if (a < 0x2000 && chr_rom.n)
		chr_rom.bank[chr_rom.cur][a] = b;
	else
		ppu.ram[a] = b;
}

byte
ppu_getb(word a)
{
	a = ppu_get_addr(a);

	if (a < 0x2000 && chr_rom.n)
		return chr_rom.bank[chr_rom.cur][a];
	else
		return ppu.ram[a];
}

void
ppu_reg_set(word addr, byte b)
{
	/* Some APU registers. Ignored for now */
	if (addr >= 0x4000 && addr != OAMDMA && addr != 0x4016)
		return;

	/* Some mirroring */
	if (0x2000 <= addr && addr < 0x4000)
		addr = 0x2000 + addr % 8;

	printf("Setting reg %04x to %02x\n", addr, b);

	switch (addr) {
	case OAMDMA:
		ppu_load_spr_ram(b * 0x100);
		break;
	case PPUSCROLL:
		switch (ppu.scroll) {
		case 0:  p_x = ppu.PPUSCROLL_X; ppu.PPUSCROLL_X = b; break;
		case 1:  p_y = ppu.PPUSCROLL_Y; ppu.PPUSCROLL_Y = b; break;
		default: break;
		}

		ppu.scroll++;
		ppu.scroll %= 2;
		break;
	case PPUCTRL:
		if (ppu.ready)
			ppu.PPUCTRL = b;
		break;
	case PPUMASK:
		if (ppu.ready)
			ppu.PPUMASK = b;
		break;
	case PPUADDR:
		if (!ppu.ready)
			break;

		ppu.PPUADDR <<= 8;
		ppu.PPUADDR |= b;

		ppu.addr++;
		ppu.addr %= 2;
		break;
	case PPUDATA:
		ppu_setb(ppu.PPUADDR, b);
		ppu.PPUADDR += (ppu.PPUCTRL & (1 << 2)) ? 32 : 1;
		break;
	case OAMADDR:
		ppu.OAMADDR = b;
		break;
	case OAMDATA:
		ppu.spr_ram[ppu.OAMADDR++] = b;
		break;
	/* Joystick 1 */
	case 0x4016:
		joy1_write(b);
		break;
	default:
		todo();
	}
}

byte
ppu_is_reg_r(word addr)
{
	/* Some mirroring */
	if (0x2000 <= addr && addr < 0x4000)
		addr = 0x2000 + addr % 8;

	switch (addr) {
	case PPUSTATUS:
	case OAMDATA:
	case PPUDATA:
		return 1;
	default:
		return 0;
	}
}

word
ppu_bg_patt_tbl()
{
	return (ppu.PPUCTRL & (1 << 4)) ? 0x1000 : 0x0000;
}

word
ppu_spr_patt_tbl()
{
	return (ppu.PPUCTRL & (1 << 3)) ? 0x1000 : 0x0000;
}

word
ppu_get_name_tbl_left(int x, int y)
{
	int idx;

	y %= 460;
	x %= 512;

	idx = ppu.PPUCTRL & 3;

	if (y < 240 && x < 256)
		return 0x2000 + (idx ^ 0) * 0x400 % 0x1000;
	else if (y <  240 && x >= 256)
		return 0x2000 + (idx ^ 1) * 0x400 % 0x1000;
	else if (y >= 240 && x < 256)
		return 0x2000 + (idx ^ 2) * 0x400 % 0x1000;
	else//if (y >= 240 & x >= 256)
		return 0x2000 + (idx ^ 3) * 0x400 % 0x1000;
}

word
ppu_get_name_tbl_right(int x, int y)
{
	int idx;

	y %= 460;
	x %= 512;

	idx = ppu.PPUCTRL & 3;

	if (y < 240 && x < 256)
		return 0x2000 + (idx ^ 1) * 0x400 % 0x1000;
	else if (y <  240 && x >= 256)
		return 0x2000 + (idx ^ 2) * 0x400 % 0x1000;
	else if (y >= 240 && x < 256)
		return 0x2000 + (idx ^ 3) * 0x400 % 0x1000;
	else//if (y >= 240 & x >= 256)
		return 0x2000 + (idx ^ 0) * 0x400 % 0x1000;
}

void
ppu_draw_tile_line(struct window_layer *lp, int spr_idx, int tile,
				   int scr_x, int scr_y, int ny, int pal, int hflip)
{
	byte low, high;
	int i, clr;

	low  = ppu_getb(tile + ny % 8);
	high = ppu_getb(tile + ny % 8 + 8);

	for (i = 0; i < 8; i++) {
		if (scr_x + i < 0 || scr_x + i > 255)
			continue;

		clr = color_tbl[low][high][(hflip ? 7 - i : i)];

		if (spr_idx != -1 && clr == 0)
			continue;

		lp->arr[scr_y][scr_x + i] = ppu_getb(pal + clr);

		if (spr_idx == -1) {
			bg_trans[scr_y][scr_x + i] = !clr;

		} else if (spr_idx == 0 && !bg_trans[scr_y][scr_x + i] &&
					!ppu_sprite_hit_occured) {

			/* Setting sprite 0 hit */
			ppu.PPUSTATUS |= (1 << 6);
			ppu_sprite_hit_occured = TRUE;
		}
	}
}

void
ppu_draw_bg_line()
{
	int x, y, nx, ny;
	int scr_x, scr_y;
	word patt, name_left, name_right, att_left, att_right;

	scr_y = ppu.scanline;
	y = ppu.PPUSCROLL_Y + ppu.scanline;
	ny = y % 240;

	patt = ppu_bg_patt_tbl();

	name_left  = ppu_get_name_tbl_left(ppu.PPUSCROLL_X, y);
	name_right = ppu_get_name_tbl_right(ppu.PPUSCROLL_X, y);

	att_left  = name_left  + 0x3C0;
	att_right = name_right + 0x3C0;

	for (scr_x = -ppu.PPUSCROLL_X % 8; scr_x < 256; scr_x += 8) {
		word name, att, pal;
		int tile, tile_idx;

		x = scr_x + ppu.PPUSCROLL_X;
		nx = x % 256;

		if (x < 0)
			continue;

		if (x < 256) {
			name = name_left;
			att = att_left;
		} else {
			name = name_right;
			att = att_right;
		}

		tile_idx = ppu_getb(name + (ny >> 3) * 32 + (nx >> 3));
		tile = patt + tile_idx * 16;

		pal = att + (ny >> 5) * 8 + (nx >> 5);
		pal = ppu_getb(pal);

		// 33221100 = 2 bits per attribute area
		// 0 1		= their relative position on the screen
		// 2 3
		// each box is 16x16 pixels

		if (ny % 32 >= 16)
			pal >>= 4;
		if (nx % 32 >= 16)
			pal >>= 2;

		pal = 0x3f00 + (pal % 4) * 4;
//		bg_tiles[scr_y/8][scr_x/8] = tile_idx;
//		bg_pals[scr_y/16][scr_x/16] = pal;
		ppu_draw_tile_line(&bg, -1, tile, scr_x, scr_y, ny, pal, 0);
	}
}

void
ppu_draw_sprites_line()
{
	int spr_idx;
	int scr_y, tile_idx;
	byte x, y;
	byte sprh, hflip, vflip, prior;
	word patt, tile, pal;

	scr_y = ppu.scanline;
	sprh = (ppu.PPUCTRL & (1 << 5)) ? 16 : 8;
	patt = ppu_spr_patt_tbl();

	for (spr_idx = 0; spr_idx < 256; spr_idx += 4) {
		y = ppu.spr_ram[spr_idx];

		if (!(y <= scr_y && scr_y < y + sprh))
			continue;

		tile_idx = ppu.spr_ram[spr_idx + 1];
		pal = 0x3f10 + (ppu.spr_ram[spr_idx + 2] & 3) * 4;
		hflip = ppu.spr_ram[spr_idx + 2] & (1 << 6);
		vflip = ppu.spr_ram[spr_idx + 2] & (1 << 7);
		prior = ppu.spr_ram[spr_idx + 2] & (1 << 7);
		x = ppu.spr_ram[spr_idx + 3];

		if (sprh == 16) {
			patt = (tile_idx & 1) * 0x1000;
			/* Clearing first bit */
			tile_idx &= ~1;
		}

		if ((scr_y - y >= 8 && vflip == 0) ||
							(scr_y - y < 8 && vflip)) {
			if (vflip == 0)
				y += 8;
			tile_idx += 1;
		}

		tile = patt + 16 * tile_idx;

		int ny;
		struct window_layer *lp;

		ny = (vflip ? (7 - scr_y + y) : (scr_y - y));
		lp = (prior ? &spr0 : &spr1);

		ppu_draw_tile_line(lp, spr_idx, tile, x, scr_y + 1, ny, pal, hflip);
	}
}

void
ppu_draw_screen()
{
	int i, j;

	for (i = 0; i < 240; i++) {
		for (j = 0; j < 256; j++) {
			if (spr1.arr[i][j] != 0xff)
				spr0.arr[i][j] = spr1.arr[i][j];
			else if (bg.arr[i][j] != 0xff)
				spr0.arr[i][j] = bg.arr[i][j];
		}
	}

	window_set_to_layer(&spr0);
	window_flush();

	window_layer_clear(&bg, 0xff);
	window_layer_clear(&spr0, 0xff);
	window_layer_clear(&spr1, 0xff);
}

void
ppu_print_data()
{
	int i, j;

	printf("\n\nPPU memory\n");
	for (i = 0x2000; i < 0x2400; i += 0x10) {
		printf("%04x:\t", i);
		for (j = 0; j < 0x10; j++)
			printf("%02x ", ppu_getb(i + j));
		printf("\n");
	}
}

void
ppu_print_bg_tiles()
{
	int x, y;

	fprintf(stderr, "\nTiles:\n");

	for (y = 0; y < 30; y++) {
		for (x = 0; x < 32; x++)
			fprintf(stderr, "[%03x] = %02x (%x %x)\n", y * 32 + x, bg_tiles[y][x], y, x);
	}
	fprintf(stderr, "\n");
}

void
ppu_run_cycle()
{
	printf("Start of scanline %d\n", ppu.scanline);

	if (!ppu.ready && cpu_cycles > 29658)
		ppu.ready = 1;

	if (ppu.scanline < 240) {
		/* Rendering! */

		/* If background */
		if (ppu.PPUMASK & (1 << 3))
			ppu_draw_bg_line();

		/* If sprite */
		if (ppu.PPUMASK & (1 << 4))
			ppu_draw_sprites_line();
	}
	else if (ppu.scanline == 240) {
		/* Dummy scanline */
	}
	else if (ppu.scanline == 241) {
		/* Start of Vblank! */

		/* In vblank = True */
		ppu.PPUSTATUS |=  (1 << 7);
		/* In Sprite-0 hit = False */
		ppu.PPUSTATUS &= ~(1 << 6);

		/* If should raise nmi */
		if (ppu.PPUCTRL & (1 << 7))
			cpu_nmi();
	}
	else if (ppu.scanline < 261) {
		/* VBLANK! */
	}
	else if (ppu.scanline == 261) {
		/* Dummy scanline */

		/* In vblank = False */
		ppu.PPUSTATUS &= ~(1 << 7);
//		printf("Drawing screen\n");
		ppu_draw_screen();
//		ppu_print_data();
//		ppu_print_bg_tiles();
		ppu.scanline = -1;
		ppu_sprite_hit_occured = FALSE;
	}

	ppu.scanline++;
}

void
ppu_run_cycles(int n)
{
	while (n--)
		ppu_run_cycle();
}

void
ppu_init()
{
	ppu.PPUCTRL = 0;
	ppu.PPUMASK = 0;
	ppu.PPUSCROLL_X = 0;
	ppu.PPUSCROLL_Y = 0;
	ppu.PPUADDR = 0;
	ppu.PPUSTATUS = (1 << 7) | (1 << 5);

	ppu.ready = 0;
	ppu.scanline = 0;

	int low, high, x;

	for (low = 0; low < 256; low++) {
		for (high = 0; high < 256; high++) {
			for (x = 0; x < 8; x++) {
				int clr0, clr1;

				clr0 = (high >> (7 - x)) & 1;
				clr1 = (low  >> (7 - x)) & 1;

				color_tbl[low][high][x] = (clr0 << 1) | clr1;
			}
		}
	}
}

