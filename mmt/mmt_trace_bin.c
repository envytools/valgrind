/*
   Copyright (C) 2006 Dave Airlie
   Copyright (C) 2007 Wladimir J. van der Laan
   Copyright (C) 2009, 2011, 2014 Marcin Slusarz <marcin.slusarz@gmail.com>

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
#include "pub_tool_vkiscnums.h"
#include "coregrind/pub_core_syscall.h"

#include "mmt_trace_bin.h"

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

#define print_begin(type) do { \
		mmt_bin_write_1(type); \
		mmt_bin_write_4(region->id); \
		mmt_bin_write_4((UInt)(addr - region->start)); \
	} while (0)

#define print_store_begin() \
		print_begin('w')

#define print_store_with_info_begin() \
		print_begin('x')

#define print_load_begin() \
		print_begin('r')

#define print_load_with_info_begin() \
		print_begin('s')

#define print_1(value) \
		mmt_bin_write_1(1); \
		mmt_bin_write_1(value);

#define print_2(value) \
		mmt_bin_write_1(2); \
		mmt_bin_write_2(value);

#define print_4(value) \
		mmt_bin_write_1(4); \
		mmt_bin_write_4(value);

#define print_4_4(value1, value2) \
		mmt_bin_write_1(8); \
		mmt_bin_write_4(value2); \
		mmt_bin_write_4(value1);

#define print_4_4_4_4(value1, value2, value3, value4) \
		mmt_bin_write_1(16); \
		mmt_bin_write_4(value4); \
		mmt_bin_write_4(value3); \
		mmt_bin_write_4(value2); \
		mmt_bin_write_4(value1);

#define print_8(value) \
		mmt_bin_write_1(8); \
		mmt_bin_write_8(value);

#define print_8_8(value1, value2) \
		mmt_bin_write_1(16); \
		mmt_bin_write_8(value2); \
		mmt_bin_write_8(value1);

#define print_8_8_8_8(value1, value2, value3, value4) \
		mmt_bin_write_1(32); \
		mmt_bin_write_8(value4); \
		mmt_bin_write_8(value3); \
		mmt_bin_write_8(value2); \
		mmt_bin_write_8(value1);

#define print_str(str) mmt_bin_write_str(str)

#define print_store_end() do { mmt_bin_end(); mmt_bin_sync(); } while (0)
#define print_load_end() do { mmt_bin_end(); mmt_bin_sync(); } while (0)

VG_REGPARM(2)
void mmt_trace_store_bin_1(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_1(value);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_1_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_1(value);
	print_str(namestr);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_2(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_2(value);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_2_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_2(value);
	print_str(namestr);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_4(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_4(value);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_4_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_4(value);
	print_str(namestr);
	print_store_end();
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_bin_8(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_8(value);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_8_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_8(value);
	print_str(namestr);
	print_store_end();
}
#endif

VG_REGPARM(2)
void mmt_trace_store_bin_4_4(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_4_4(value1, value2);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_4_4(value1, value2);
	print_str(namestr);
	print_store_end();
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_bin_8_8(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_8_8(value1, value2);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_8_8(value1, value2);
	print_str(namestr);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_8_8_8_8(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_8_8_8_8(value1, value2, value3, value4);
	print_store_end();
}

VG_REGPARM(2)
void mmt_trace_store_bin_8_8_8_8_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, UWord inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_8_8_8_8(value1, value2, value3, value4);
	print_str(namestr);
	print_store_end();
}
#endif

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store_bin_4_4_4_4(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_store_begin();
	print_4_4_4_4(value1, value2, value3, value4);
	print_store_end();
}
VG_REGPARM(2)
void mmt_trace_store_bin_4_4_4_4_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_store_with_info_begin();
	print_4_4_4_4(value1, value2, value3, value4);
	print_str(namestr);
	print_store_end();
}
#endif

VG_REGPARM(2)
void mmt_trace_load_bin_1(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_1(value);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_1_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_1(value);
	print_str(namestr);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_2(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_2(value);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_2_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_2(value);
	print_str(namestr);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_4(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_4(value);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_4_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_4(value);
	print_str(namestr);
	print_load_end();
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_bin_8(Addr addr, UWord value)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_8(value);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_8_ia(Addr addr, UWord value, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_8(value);
	print_str(namestr);
	print_load_end();
}
#endif

VG_REGPARM(2)
void mmt_trace_load_bin_4_4(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_4_4(value1, value2);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_4_4(value1, value2);
	print_str(namestr);
	print_load_end();
}

#ifdef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_bin_8_8(Addr addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_8_8(value1, value2);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_8_8(value1, value2);
	print_str(namestr);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_8_8_8_8(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_8_8_8_8(value1, value2, value3, value4);
	print_load_end();
}

VG_REGPARM(2)
void mmt_trace_load_bin_8_8_8_8_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_8_8_8_8(value1, value2, value3, value4);
	print_str(namestr);
	print_load_end();
}
#endif

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load_bin_4_4_4_4(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	print_load_begin();
	print_4_4_4_4(value1, value2, value3, value4);
	print_load_end();
}
VG_REGPARM(2)
void mmt_trace_load_bin_4_4_4_4_ia(Addr addr, UWord value1, UWord value2,
		UWord value3, UWord value4, Addr inst_addr)
{
	struct mmt_mmap_data *region;
	char namestr[256];

	region = find_mmap(addr);
	if (LIKELY(!region))
		return;

	mydescribe(inst_addr, namestr, 256);

	print_load_with_info_begin();
	print_4_4_4_4(value1, value2, value3, value4);
	print_str(namestr);
	print_load_end();
}
#endif

#define BUF_SIZE 64 * 1024
static char buffer[BUF_SIZE];
static int written = 0;

void mmt_bin_flush(void)
{
	VG_(write)(VG_(log_output_sink).fd, buffer, written);
	written = 0;
}

void mmt_bin_flush_and_sync(void)
{
	mmt_bin_flush();
	VG_(do_syscall1)(__NR_fdatasync, VG_(log_output_sink).fd);
}

void mmt_bin_write_1(UChar u8)
{
	if (BUF_SIZE - written < 1)
		mmt_bin_flush();
	VG_(memcpy)(buffer + written, &u8, 1);
	written++;
}
void mmt_bin_write_2(UShort u16)
{
	if (BUF_SIZE - written < 2)
		mmt_bin_flush();
	VG_(memcpy)(buffer + written, &u16, 2);
	written += 2;
}
void mmt_bin_write_4(UInt u32)
{
	if (BUF_SIZE - written < 4)
		mmt_bin_flush();
	VG_(memcpy)(buffer + written, &u32, 4);
	written += 4;
}
void mmt_bin_write_8(ULong u64)
{
	if (BUF_SIZE - written < 8)
		mmt_bin_flush();
	VG_(memcpy)(buffer + written, &u64, 8);
	written += 8;
}
#define MIN(a, b) ((a) < (b) ? (a) : (b))
void mmt_bin_write_str(const char *str)
{
	UInt len = VG_(strlen)(str) + 1;
	mmt_bin_write_4(len);

	do
	{
		if (BUF_SIZE - written < len)
			mmt_bin_flush();
		int cur = MIN(BUF_SIZE - written, len);
		VG_(memcpy)(buffer + written, str, cur);
		written += cur;
		len -= cur;
		str += cur;
	}
	while (len > 0);
}
void mmt_bin_write_buffer(UChar *buf, int len)
{
	mmt_bin_write_4(len);

	do
	{
		if (BUF_SIZE - written < len)
			mmt_bin_flush();
		int cur = MIN(BUF_SIZE - written, len);
		VG_(memcpy)(buffer + written, buf, cur);
		written += cur;
		len -= cur;
		buf += cur;
	}
	while (len > 0);
}
