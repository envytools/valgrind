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

#include "mmt_trace.h"
#include "mmt_nv_ioctl.h"

#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_vkiscnums.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcassert.h"

//#define MMT_PRINT_FILENAMES

static struct mmt_mmap_data mmt_mmaps[MMT_MAX_REGIONS];
static int mmt_last_region = -1;

static UInt mmt_current_item = 1;

int mmt_trace_opens = False;
struct mmt_trace_file mmt_trace_files[MMT_MAX_TRACE_FILES];

int mmt_trace_all_files = False;

static struct mmt_mmap_data *last_used_region;

struct negative_region {
	Addr start, end;
	int score;
};

#define NEG_REGS 10
static struct negative_region neg_regions[NEG_REGS];
static int neg_regions_number;

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define noinline	__attribute__((noinline))

static inline struct mmt_mmap_data *__mmt_bsearch(Addr addr, int *next)
{
	int start = 0, end = mmt_last_region, middle;
	struct mmt_mmap_data *tmp;

	while (start <= end)
	{
		middle = start + (end - start) / 2;
		tmp = &mmt_mmaps[middle];

		if (addr < tmp->start)
		{
			if (end == middle)
				break;
			end = middle;
		}
		else if (addr >= tmp->end)
			start = middle + 1;
		else
			return tmp;
	}
	*next = start;

	return NULL;
}

static void add_neg(Addr start, Addr end)
{
	if (neg_regions_number < NEG_REGS)
		neg_regions_number++;
	neg_regions[neg_regions_number - 1].start = start;
	neg_regions[neg_regions_number - 1].end = end;
	neg_regions[neg_regions_number - 1].score = 0;
}

/* finds region to which addr belongs to */
static noinline struct mmt_mmap_data *mmt_bsearch(Addr addr)
{
	struct mmt_mmap_data *region;
	int tmp;

	/* before first? */
	if (addr < mmt_mmaps[0].start)
	{
		add_neg(0, mmt_mmaps[0].start);
		return NULL;
	}

	/* after last? */
	if (addr >= mmt_mmaps[mmt_last_region].end)
	{
		add_neg(mmt_mmaps[mmt_last_region].end, (Addr)-1);
		return NULL;
	}

	region = __mmt_bsearch(addr, &tmp);
	if (region)
		return region;

	add_neg(mmt_mmaps[tmp - 1].end, mmt_mmaps[tmp].start);
	return NULL;
}

/* finds index of region which follows addr, assuming it does not belong to any */
static int mmt_bsearch_next(Addr addr)
{
	int index;
	tl_assert(__mmt_bsearch(addr, &index) == NULL);
	return index;
}

static noinline void score_higher(int i)
{
	struct negative_region tmp;
	struct negative_region *curr = &neg_regions[i];
	do
	{
		tmp = curr[-1];
		curr[-1] = *curr;
		*curr = tmp;

		i--;
		curr--;
	}
	while (i > 1 && curr->score > curr[-1].score);
}

static inline struct mmt_mmap_data *find_mmap(Addr addr)
{
	struct mmt_mmap_data *region;
	struct negative_region *neg = neg_regions;
	int i;

	if (likely(addr >= neg->start && addr < neg->end))
	{
		neg->score++;
		return NULL;
	}

	if (likely(last_used_region && addr >= last_used_region->start && addr < last_used_region->end))
		return last_used_region;

	/* if score of first negative entry grew too much - divide all entries;
	 * it prevents overflowing and monopoly at the top */
	if (unlikely(neg->score > 1 << 24))
		for (i = 0; i < neg_regions_number; ++i)
			neg_regions[i].score >>= 10;

	/* check all negative regions */
	for (i = 1; i < neg_regions_number; ++i)
	{
		neg++;
		if (likely(addr >= neg->start && addr < neg->end))
		{
			neg->score++;
			/* if current entry score is bigger than previous */
			if (unlikely(neg->score > neg[-1].score))
				score_higher(i); /* then swap them */
			return NULL;
		}
	}

	region = mmt_bsearch(addr);

	if (region)
		last_used_region = region;

	return region;
}

struct mmt_mmap_data *mmt_find_region_by_fd_offset(int fd, Off64T offset)
{
	int i;
	struct mmt_mmap_data *fd0_region = NULL;

	for (i = 0; i <= mmt_last_region; ++i)
	{
		struct mmt_mmap_data *region = &mmt_mmaps[i];
		if (region->offset == offset)
		{
			if (region->fd == fd)
				return region;
			if (region->fd == 0)
				fd0_region = region;
		}
	}

	return fd0_region;
}

struct mmt_mmap_data *mmt_find_region_by_fdset_offset(fd_set *fds, Off64T offset)
{
	int i;
	struct mmt_mmap_data *fd0_region = NULL;

	for (i = 0; i <= mmt_last_region; ++i)
	{
		struct mmt_mmap_data *region = &mmt_mmaps[i];
		if (region->offset == offset)
		{
			if (FD_ISSET(region->fd, fds))
				return region;
			if (region->fd == 0)
				fd0_region = region;
		}
	}

	return fd0_region;
}

struct mmt_mmap_data *mmt_find_region_by_fdset_data(fd_set *fds, UWord data1, UWord data2)
{
	int i;
	struct mmt_mmap_data *fd0_region = NULL;

	for (i = 0; i <= mmt_last_region; ++i)
	{
		struct mmt_mmap_data *region = &mmt_mmaps[i];
		if (region->data1 == data1 && region->data2 == data2)
		{
			if (FD_ISSET(region->fd, fds))
				return region;
			if (region->fd == 0)
				fd0_region = region;
		}
	}

	return fd0_region;
}

static void remove_neg_region(int idx)
{
	VG_(memmove)(&neg_regions[idx], &neg_regions[idx + 1], (neg_regions_number - idx - 1) * sizeof(neg_regions[0]));
	neg_regions_number--;
	VG_(memset)(&neg_regions[neg_regions_number], 0, sizeof(neg_regions[0]));
}

void mmt_free_region(struct mmt_mmap_data *m)
{
	int idx = m - &mmt_mmaps[0];
	int i, found = -1;
	Addr start = m->start;
	Addr end = m->end;
	int joined = 0;

	/* are we freeing region adjacent to negative region?
	 * if yes, then extend negative region */
	for (i = 0; i < neg_regions_number; ++i)
	{
		struct negative_region *neg = &neg_regions[i];

		if (neg->end == start)
		{
			neg->end = end;
			found = i;
			break;
		}
		if (neg->start == end)
		{
			neg->start = start;
			found = i;
			break;
		}
	}

	if (found >= 0)
	{
		/* now that we extended negative region, maybe we can join two negative regions? */
		struct negative_region *found_reg = &neg_regions[found];
		int score = found_reg->score;
		start = found_reg->start;
		end = found_reg->end;

		for (i = 0; i < neg_regions_number; ++i)
		{
			struct negative_region *neg = &neg_regions[i];

			if (neg->end == start)
			{
				/* there is another negative region which ends where our starts */

				if (neg->score > score)
				{
					/* another is better */
					neg->end = end;

					remove_neg_region(found);
				}
				else
				{
					/* our is better */
					found_reg->start = neg->start;

					remove_neg_region(i);
				}

				joined = 1;
				break;
			}

			if (neg->start == end)
			{
				/* there is another negative region which starts where our ends */

				if (neg->score > score)
				{
					/* another is better */
					neg->start = start;

					remove_neg_region(found);
				}
				else
				{
					/* our is better */
					found_reg->end = neg->end;

					remove_neg_region(i);
				}

				joined = 1;
				break;
			}
		}
	}

	if (mmt_last_region != idx)
		VG_(memmove)(mmt_mmaps + idx, mmt_mmaps + idx + 1,
				(mmt_last_region - idx) * sizeof(struct mmt_mmap_data));
	VG_(memset)(&mmt_mmaps[mmt_last_region--], 0, sizeof(struct mmt_mmap_data));

	/* if we are releasing last used region, then zero cache */
	if (m == last_used_region)
		last_used_region = NULL;
	else if (last_used_region > m) /* if last used region was in area which just moved */
	{
		/* then move pointer by -1 */
		last_used_region--;
	}

	if (found >= 0)
	{
		/* we could not join multiple negative regions, maybe we can extend it? */
		if (!joined)
		{
			struct negative_region *found_reg = &neg_regions[found];
			int tmp = 0;
			struct mmt_mmap_data *region;

			if (start > 0)
			{
				region = __mmt_bsearch(start - 1, &tmp);
				if (region == NULL)
				{
					if (tmp >= 1)
						found_reg->start = mmt_mmaps[tmp - 1].end;
				}
			}
			if (end < (Addr)-1)
			{
				region = __mmt_bsearch(end, &tmp);
				if (region == NULL)
				{
					if (tmp <= mmt_last_region)
						found_reg->end = mmt_mmaps[tmp].start;
				}
			}
		}
	}
}

struct mmt_mmap_data *mmt_add_region(int fd, Addr start, Addr end,
		Off64T offset, UInt id, UWord data1, UWord data2)
{
	struct mmt_mmap_data *region;
	int i;

	tl_assert2(mmt_last_region + 1 < MMT_MAX_REGIONS, "not enough space for new mmap!");

	if (start >= mmt_mmaps[mmt_last_region].end)
		region = &mmt_mmaps[++mmt_last_region];
	else
	{
		i = mmt_bsearch_next(start);

		region = &mmt_mmaps[i];
		if (i != mmt_last_region + 1)
		{
			VG_(memmove)(&mmt_mmaps[i+1], &mmt_mmaps[i],
					(mmt_last_region - i  + 1) * sizeof(mmt_mmaps[0]));

			if (last_used_region >= region)
				last_used_region++;
		}
		mmt_last_region++;
	}

	/* look for negative regions overlapping new region */
	for (i = 0; i < neg_regions_number; ++i)
		if ((start >= neg_regions[i].start && start < neg_regions[i].end) ||
			(end   >= neg_regions[i].start && end   < neg_regions[i].end))
		{
			/* just remove negative region */
			remove_neg_region(i);
			i--;
		}

	region->fd = fd;
	if (id == 0)
		region->id = mmt_current_item++;
	else
		region->id = id;
	region->start = start;
	region->end = end;
	region->offset = offset;
	region->data1 = data1;
	region->data2 = data2;

	return region;
}

#ifdef MMT_PRINT_FILENAMES
#define MMT_NAMESTR_LEN 256
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
#define MMT_NAMESTR_LEN 1
static inline void mydescribe(Addr inst_addr, char *namestr, int len)
{
	namestr[0] = 0;
}
#endif

VG_REGPARM(2)
void mmt_trace_store(Addr addr, SizeT size, Addr inst_addr, UWord value)
{
	struct mmt_mmap_data *region;
	char valstr[22];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
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
	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

VG_REGPARM(2)
void mmt_trace_store2(Addr addr, SizeT size, Addr inst_addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;
	char valstr[44];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
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

	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store4(Addr addr, Addr inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;
	char valstr[44];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
		return;

	VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx", value1, value2, value3, value4);
	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "w %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}
#endif

VG_REGPARM(2)
void mmt_trace_load(Addr addr, SizeT size, UInt inst_addr, UWord value)
{
	struct mmt_mmap_data *region;
	char valstr[22];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
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
	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

VG_REGPARM(2)
void mmt_trace_load2(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2)
{
	struct mmt_mmap_data *region;
	char valstr[44];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
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
	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load4(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
{
	struct mmt_mmap_data *region;
	char valstr[44];
	char namestr[MMT_NAMESTR_LEN];

	region = find_mmap(addr);
	if (likely(!region))
		return;

	VG_(sprintf) (valstr, "0x%08lx,0x%08lx,0x%08lx,0x%08lx", value1, value2, value3, value4);
	mydescribe(inst_addr, namestr, MMT_NAMESTR_LEN);

	VG_(message) (Vg_DebugMsg, "r %d:0x%04x, %s %s\n", region->id, (unsigned int)(addr - region->start), valstr, namestr);
}
#endif

void mmt_pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs)
{
	if (syscallno == __NR_ioctl)
		mmt_nv_ioctl_pre(args);
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
	Addr start = args[0];
	unsigned long len = args[1];
//	unsigned long prot = args[2];
//	unsigned long flags = args[3];
	unsigned long fd = args[4];
	unsigned long offset = args[5];
	int i;
	struct mmt_mmap_data *region;

	if (res._isError || (int)fd == -1)
		return;

	start = res._val;

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

	region = mmt_add_region(fd, start, start + len, offset * offset_unit, 0, 0, 0);

	VG_(message) (Vg_DebugMsg,
			"got new mmap at %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
			(void *)region->start, len, region->offset, region->id);
}

static void post_munmap(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	Addr start = args[0];
//	unsigned long len = args[1];
	struct mmt_mmap_data *region;
	int tmpi;

	if (res._isError)
		return;

	region = __mmt_bsearch(start, &tmpi);
	if (!region)
		return;

	VG_(message) (Vg_DebugMsg,
			"removed mmap 0x%lx:0x%lx for: %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
			region->data1, region->data2, (void *)region->start,
			region->end - region->start, region->offset, region->id);

	mmt_free_region(region);
}

static void post_mremap(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	Addr start = args[0];
	unsigned long old_len = args[1];
	unsigned long new_len = args[2];
//	unsigned long flags = args[3];
	struct mmt_mmap_data *region, tmp;
	int tmpi;

	if (res._isError)
		return;

	region = __mmt_bsearch(start, &tmpi);
	if (!region)
		return;

	tmp = *region;
	mmt_free_region(region);
	region = mmt_add_region(tmp.fd, res._val, res._val + new_len, tmp.offset, tmp.id, tmp.data1, tmp.data2);

	VG_(message) (Vg_DebugMsg,
			"changed mmap 0x%lx:0x%lx from: (address: %p, len: 0x%08lx), to: (address: %p, len: 0x%08lx), offset 0x%llx, serial %d\n",
			region->data1, region->data2,
			(void *)start, old_len,
			(void *)region->start, region->end - region->start,
			region->offset, region->id);
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
