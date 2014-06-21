/*
   Copyright (C) 2006 Dave Airlie
   Copyright (C) 2007 Wladimir J. van der Laan
   Copyright (C) 2009, 2011 Marcin Slusarz <marcin.slusarz@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_debuginfo.h"
#include "pub_tool_libcprint.h"

#include "mmt_trace_text.h"

static void mydescribe(Addr inst_addr, char *namestr, int len)
{
	char filename[100];
	UInt line = 0;

	if (VG_(get_filename)(inst_addr, filename, 100))
	{
		VG_(get_linenum)(inst_addr, &line);
		VG_(snprintf) (namestr, len, "@%08lx (%s:%d)", inst_addr, filename, line);
	}
	else
		VG_(snprintf) (namestr, len, "@%08lx", inst_addr);
}

#define print_store(fmt, ...) \
	VG_(message)(Vg_DebugMsg, "w %d:0x%04x, " fmt "\n", \
				region->id, (unsigned int)(addr - region->start), __VA_ARGS__)
#define print_load(fmt, ...) \
	VG_(message)(Vg_DebugMsg, "r %d:0x%04x, " fmt "\n", \
				region->id, (unsigned int)(addr - region->start), __VA_ARGS__)

VG_REGPARM(2)
void mmt_trace_store_1(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%02lx ", value);
}

VG_REGPARM(2)
void mmt_trace_store_1_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%02lx %s", value, namestr);
}

VG_REGPARM(2)
void mmt_trace_store_2(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%04lx ", value);
}

VG_REGPARM(2)
void mmt_trace_store_2_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%04lx %s", value, namestr);
}

VG_REGPARM(2)
void mmt_trace_store_4(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx ", value);
}

VG_REGPARM(2)
void mmt_trace_store_4_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx %s", value, namestr);
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_8(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx,0x%08lx ", value >> 32, value & 0xffffffff);
}
VG_REGPARM(2)
void mmt_trace_store_8_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx,0x%08lx %s", value >> 32, value & 0xffffffff, namestr);
}
#endif

VG_REGPARM(2)
void mmt_trace_store_4_4(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx,0x%08lx ", value1, value2);
}

VG_REGPARM(2)
void mmt_trace_store_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx,0x%08lx %s", value1, value2, namestr);
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_8_8(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff);
}

VG_REGPARM(2)
void mmt_trace_store_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff, namestr);
}

VG_REGPARM(2)
void mmt_trace_store_8_8_8_8(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value3 >> 32,
			value3 & 0xffffffff, value4 >> 32, value4 & 0xffffffff);
	addr += 0x10;
	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff);
}

VG_REGPARM(2)
void mmt_trace_store_8_8_8_8_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, UWord inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value3 >> 32,
			value3 & 0xffffffff, value4 >> 32, value4 & 0xffffffff, namestr);
	addr += 0x10;
	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff, namestr);
}
#endif

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_4_4_4_4(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1, value2,
			value3, value4);
}
VG_REGPARM(2)
void mmt_trace_store_4_4_4_4_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1, value2,
			value3, value4, namestr);
}
#endif

VG_REGPARM(2)
void mmt_trace_load_1(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%02lx ", value);
}

VG_REGPARM(2)
void mmt_trace_load_1_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%02lx %s", value, namestr);
}

VG_REGPARM(2)
void mmt_trace_load_2(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%04lx ", value);
}

VG_REGPARM(2)
void mmt_trace_load_2_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%04lx %s", value, namestr);
}

VG_REGPARM(2)
void mmt_trace_load_4(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx ", value);
}

VG_REGPARM(2)
void mmt_trace_load_4_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx %s", value, namestr);
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_8(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx,0x%08lx ", value >> 32, value & 0xffffffff);
}
VG_REGPARM(2)
void mmt_trace_load_8_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx,0x%08lx %s", value >> 32, value & 0xffffffff, namestr);
}
#endif

VG_REGPARM(2)
void mmt_trace_load_4_4(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx,0x%08lx ", value1, value2);
}

VG_REGPARM(2)
void mmt_trace_load_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx,0x%08lx %s", value1, value2, namestr);
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_8_8(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff);
}

VG_REGPARM(2)
void mmt_trace_load_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff, namestr);
}

VG_REGPARM(2)
void mmt_trace_load_8_8_8_8(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value3 >> 32,
			value3 & 0xffffffff, value4 >> 32, value4 & 0xffffffff);
	addr += 0x10;
	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff);
}

VG_REGPARM(2)
void mmt_trace_load_8_8_8_8_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value3 >> 32,
			value3 & 0xffffffff, value4 >> 32, value4 & 0xffffffff);
	addr += 0x10;
	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1 >> 32,
			value1 & 0xffffffff, value2 >> 32, value2 & 0xffffffff, namestr);
}
#endif

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_4_4_4_4(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx ", value1, value2,
			value3, value4);
}
VG_REGPARM(2)
void mmt_trace_load_4_4_4_4_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load("0x%08lx,0x%08lx,0x%08lx,0x%08lx %s", value1, value2,
			value3, value4, namestr);
}
#endif
