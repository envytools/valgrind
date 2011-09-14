/*
   Copyright (C) 2006 Dave Airlie
   Copyright (C) 2007 Wladimir J. van der Laan
   Copyright (C) 2009 Marcin Slusarz <marcin.slusarz@gmail.com>

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

#include "mmt_trace.h"
#include "mmt_nv_ioctl.h"

#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_vkiscnums.h"
#include "pub_tool_debuginfo.h"

//#define MMT_PRINT_FILENAMES

struct mmt_mmap_data mmt_mmaps[MMT_MAX_REGIONS];
int mmt_last_region = -1;

UInt mmt_current_item = 1;

int mmt_trace_opens = False;
struct mmt_trace_file mmt_trace_files[MMT_MAX_TRACE_FILES];

int mmt_trace_all_files = False;

static struct mmt_mmap_data *find_mmap(Addr addr)
{
	struct mmt_mmap_data *region = NULL;
	int i;

	for (i = 0; i <= mmt_last_region; i++)
	{
		region = &mmt_mmaps[i];
		if (addr >= region->start && addr < region->end)
			return region;
	}

	return NULL;
}

#ifdef MMT_PRINT_FILENAMES
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
#else
static inline void mydescribe(Addr inst_addr, char *namestr, int len)
{
	namestr[0] = 0;
}
#endif

VG_REGPARM(2)
void mmt_trace_store(Addr addr, SizeT size, Addr inst_addr, UWord value)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	switch (size)
	{
		case 1:
			VG_(sprintf) (valstr, "0x%02lx", value);
			break;
		case 2:
			VG_(sprintf) (valstr, "0x%04lx", value);
			break;
		case 4:
			VG_(sprintf) (valstr, "0x%08lx", value);
			break;
#ifdef MMT_64BIT
		case 8:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx", value >> 32, value & 0xffffffff);
			break;
#endif
		default:
			return;
	}
	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

VG_REGPARM(2)
void mmt_trace_store2(Addr addr, SizeT size, Addr inst_addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	switch (size)
	{
		case 4:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx", value1, value2);
			break;
#ifdef MMT_64BIT
		case 8:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx",
					value1 >> 32, value1 & 0xffffffff,
					value2 >> 32, value2 & 0xffffffff);
			break;
#endif
		default:
			return;
	}

	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store4(Addr addr, Addr inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx", value1, value2, value3, value4);
	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}
#endif

VG_REGPARM(2)
void mmt_trace_load(Addr addr, SizeT size, UInt inst_addr, UWord value)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	switch (size)
	{
		case 1:
			VG_(sprintf) (valstr, "0x%02lx", value);
			break;
		case 2:
			VG_(sprintf) (valstr, "0x%04lx", value);
			break;
		case 4:
			VG_(sprintf) (valstr, "0x%08lx", value);
			break;
#ifdef MMT_64BIT
		case 8:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx", value >> 32, value & 0xffffffff);
			break;
#endif
		default:
			return;
	}
	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

VG_REGPARM(2)
void mmt_trace_load2(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	switch (size)
	{
		case 4:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx", value1, value2);
			break;
#ifdef MMT_64BIT
		case 8:
			VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx",
					value1 >> 32, value1 & 0xffffffff,
					value2 >> 32, value2 & 0xffffffff);
			break;
#endif
		default:
			return;
	}
	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load4(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;
	char valstr[64];
	char namestr[256];

	region = find_mmap(addr);
	if (!region)
		return;

	VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx", value1, value2, value3, value4);
	mydescribe(inst_addr, namestr, 256);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}
#endif

void mmt_pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs)
{
	if (syscallno == __NR_ioctl)
		mmt_nv_ioctl_pre(args);
}

void mmt_free_region(int idx)
{
	if (mmt_last_region != idx)
		VG_(memmove)(mmt_mmaps + idx, mmt_mmaps + idx + 1,
				(mmt_last_region - idx) * sizeof(struct mmt_mmap_data));
	VG_(memset)(&mmt_mmaps[mmt_last_region--], 0, sizeof(struct mmt_mmap_data));
}

static void post_open(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	const char *path = (const char *)args[0];
	int i;

	if (mmt_trace_opens)
	{
		int flags = (int)args[1];
		int mode = (int)args[2];
		VG_(message)(Vg_DebugMsg, "sys_open: %s, flags: 0x%x, mode: 0x%x, ret: %ld\n", path, flags, mode, res._val);
	}
	if (res._isError)
		return;

	if (!mmt_trace_all_files)
	{
		for (i = 0; i < MMT_MAX_TRACE_FILES; ++i)
		{
			const char *path2 = mmt_trace_files[i].path;
			if (path2 != NULL && VG_(strcmp)(path, path2) == 0)
			{
				FD_SET(res._val, &mmt_trace_files[i].fds);
//				VG_(message)(Vg_DebugMsg, "fd %ld connected to %s\n", res._val, path);
				break;
			}
		}
	}

	mmt_nv_ioctl_post_open(args, res);
}

static void post_close(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	int fd = (int)args[0];
	int i;

	if (!mmt_trace_all_files)
		for(i = 0; i < MMT_MAX_TRACE_FILES; ++i)
		{
			if (mmt_trace_files[i].path != NULL && FD_ISSET(fd, &mmt_trace_files[i].fds))
			{
				FD_CLR(fd, &mmt_trace_files[i].fds);
				break;
			}
		}

	mmt_nv_ioctl_post_close(args);
}

static void post_mmap(ThreadId tid, UWord *args, UInt nArgs, SysRes res, int offset_unit)
{
	void *start = (void *)args[0];
	unsigned long len = args[1];
//	unsigned long prot = args[2];
//	unsigned long flags = args[3];
	unsigned long fd = args[4];
	unsigned long offset = args[5];
	int i;
	struct mmt_mmap_data *region;

	if (res._isError || (int)fd == -1)
		return;

	start = (void *)res._val;

	if (!mmt_trace_all_files)
	{
		for(i = 0; i < MMT_MAX_TRACE_FILES; ++i)
		{
			if (FD_ISSET(fd, &mmt_trace_files[i].fds))
				break;
		}
		if (i == MMT_MAX_TRACE_FILES)
		{
//			VG_(message)(Vg_DebugMsg, "fd %ld not found\n", fd);
			return;
		}
	}

	if (mmt_nv_ioctl_post_mmap(args, res, offset_unit))
		return;

	if (mmt_last_region + 1 >= MMT_MAX_REGIONS)
	{
		VG_(message)(Vg_UserMsg, "not enough space for new mmap!\n");
		return;
	}

	region = &mmt_mmaps[++mmt_last_region];

	region->fd = fd;
	region->id = mmt_current_item++;
	region->start = (Addr)start;
	region->end = (Addr)(((char *)start) + len);
	region->offset = offset * offset_unit;

	VG_(message) (Vg_DebugMsg,
			"got new mmap at %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
			(void *)region->start, len, region->offset, region->id);
}

static void post_munmap(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	void *start = (void *)args[0];
//	unsigned long len = args[1];
	int i;
	struct mmt_mmap_data *region;

	if (res._isError)
		return;

	for (i = 0; i <= mmt_last_region; ++i)
	{
		region = &mmt_mmaps[i];
		if (region->start == (Addr)start)
		{
			VG_(message) (Vg_DebugMsg,
					"removed mmap 0x%lx:0x%lx for: %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
					region->data1, region->data2, (void *)region->start,
					region->end - region->start, region->offset, region->id);
			mmt_free_region(i);
			return;
		}
	}
}

static void post_mremap(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	void *start = (void *)args[0];
	unsigned long old_len = args[1];
	unsigned long new_len = args[2];
//	unsigned long flags = args[3];
	int i;
	struct mmt_mmap_data *region;

	if (res._isError)
		return;

	for (i = 0; i <= mmt_last_region; ++i)
	{
		region = &mmt_mmaps[i];
		if (region->start == (Addr)start)
		{
			region->start = (Addr) res._val;
			region->end = region->start + new_len;
			VG_(message) (Vg_DebugMsg,
					"changed mmap 0x%lx:0x%lx from: (address: %p, len: 0x%08lx), to: (address: %p, len: 0x%08lx), offset 0x%llx, serial %d\n",
					region->data1, region->data2,
					start, old_len,
					(void *)region->start, region->end - region->start,
					region->offset, region->id);
			return;
		}
	}
}

void mmt_post_syscall(ThreadId tid, UInt syscallno, UWord *args,
			UInt nArgs, SysRes res)
{
	if (syscallno == __NR_ioctl)
		mmt_nv_ioctl_post(args);
	else if (syscallno == __NR_open)
		post_open(tid, args, nArgs, res);
	else if (syscallno == __NR_close)
		post_close(tid, args, nArgs, res);
	else if (syscallno == __NR_mmap)
		post_mmap(tid, args, nArgs, res, 1);
#ifndef MMT_64BIT
	else if (syscallno == __NR_mmap2)
		post_mmap(tid, args, nArgs, res, 4096);
#endif
	else if (syscallno == __NR_munmap)
		post_munmap(tid, args, nArgs, res);
	else if (syscallno == __NR_mremap)
		post_mremap(tid, args, nArgs, res);
}
