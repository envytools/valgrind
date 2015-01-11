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

#include "mmt_trace.h"
#include "mmt_fglrx_ioctl.h"
#include "mmt_nv_ioctl.h"
#include "mmt_nouveau_ioctl.h"
#include "mmt_trace_bin.h"

#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_vkiscnums.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcassert.h"
/*
 * Binary format message types: (some of them are not used anymore, so they are reserved)
 *     = = text
 *     - = text
 *     d = dup syscall
 *     e = mremap syscall
 *     i = ioctl before
 *     j = ioctl after
 *     m = reserved (old mmap syscall)
 *     M = mmap syscall
 *     n = nvidia/nouveau messages (see mmt_nv_ioctl.c for list of subtypes)
 *     o = open syscall
 *     r = memory read
 *     R = memory read (full address)
 *     s = info for next read
 *     S = sync marker
 *     t = write syscall
 *     u = munmap syscall
 *     w = memory write
 *     W = memory write (full address)
 *     x = info for next write
 *     y = memory dump
 */
static struct mmt_mmap_data mmt_mmaps[MMT_MAX_REGIONS];
static int mmt_last_region = -1;

static UInt mmt_current_item = 1;

int mmt_trace_all_opens = False;
char *mmt_trace_files[MMT_MAX_TRACE_FILES];
fd_set trace_fds;

int mmt_trace_all_files = False;

int mmt_trace_stdout_stderr = False;

static struct mmt_mmap_data null_region;
struct mmt_mmap_data *last_used_region = &null_region;

#define NEG_REGS 10
struct negative_region neg_regions[NEG_REGS];
static int neg_regions_number;

#define noinline	__attribute__((noinline))

int all_mem = 0;

static maybe_unused void dump_state(void)
{
	int i;

	mmt_bin_flush();

	if (neg_regions_number == 0)
		VG_(printf)("NEGative cache empty\n");
	else
	{
		VG_(printf)("NEG vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
		struct negative_region *reg = neg_regions;
		for (i = 0; i < neg_regions_number; ++i, ++reg)
			VG_(printf)("NEG <0x%016lx 0x%016lx> %16d\n", reg->start, reg->end, reg->score);
		VG_(printf)("NEG ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	}

	if (mmt_last_region < 0)
		VG_(printf)("POS mmap list empty\n");
	else
	{
		VG_(printf)("POS vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n");
		struct mmt_mmap_data *region = mmt_mmaps;
		for (i = 0; i <= mmt_last_region; ++i, ++region)
			VG_(printf)("POS %05d, id: %05d, start: 0x%016lx, end: 0x%016lx\n", i, region->id, region->start, region->end);
		VG_(printf)("POS ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
	}
}

#define mmt_assert(expr)                                                \
  ((void) (LIKELY(expr) ? 0 :                                           \
           (dump_state(),                                               \
            VG_(assert_fail) (False, #expr,                             \
                              __FILE__, __LINE__,                       \
                              __PRETTY_FUNCTION__,                      \
                              ""),                                      \
                              0)))

#define mmt_assert2(expr, format, args...)                              \
  ((void) (LIKELY(expr) ? 0 :                                           \
           (dump_state(),                                               \
            VG_(assert_fail) (False, #expr,                             \
                              __FILE__, __LINE__,                       \
                              __PRETTY_FUNCTION__,                      \
                              format, ##args),                          \
                              0)))

static maybe_unused void __verify_state(void)
{
	int i, j;
	struct negative_region *neg1 = neg_regions;
	struct negative_region *neg2;
	struct mmt_mmap_data *pos1, *pos2;

#ifdef MMT_DEBUG_VERBOSE
	dump_state();
#endif

	for (i = 0; i < neg_regions_number; ++i, ++neg1)
	{
		mmt_assert2(neg1->start <= neg1->end, "%p %p", (void *)neg1->start, (void *)neg1->end);

		mmt_assert2(neg1->score >= 0, "score: %d", neg1->score);

		/* score order */
		if (i > 0)
			mmt_assert2(neg1->score <= neg1[-1].score, "score: %d, prev score: %d", neg1->score, neg1[-1].score);

		neg2 = neg_regions;
		for (j = 0; j < neg_regions_number; ++j, ++neg2)
		{
			if (i == j)
				continue;

			/* negative regions should not be adjacent */
			mmt_assert2(neg1->start != neg2->end, "<%p, %p> <%p, %p>",
					(void *)neg1->start, (void *)neg1->end,
					(void *)neg2->start, (void *)neg2->end);
			mmt_assert2(neg1->end   != neg2->start, "<%p, %p> <%p, %p>",
					(void *)neg1->start, (void *)neg1->end,
					(void *)neg2->start, (void *)neg2->end);

			/* start or end are not within other region */
			mmt_assert2(neg1->start < neg2->start || neg1->start >= neg2->end,
					"<%p, %p> <%p, %p>",
					(void *)neg1->start, (void *)neg1->end,
					(void *)neg2->start, (void *)neg2->end);
			mmt_assert2(neg1->end   < neg2->start || neg1->end   >= neg2->end,
					"<%p, %p> <%p, %p>",
					(void *)neg1->start, (void *)neg1->end,
					(void *)neg2->start, (void *)neg2->end);
		}
	}

	/* negative regions after last should be empty */
	for (i = neg_regions_number; i < NEG_REGS; ++i, ++neg1)
		mmt_assert(neg1->start == 0 && neg1->end == 0 && neg1->score == 0);

	pos1 = mmt_mmaps;
	for (i = 0; i <= mmt_last_region; ++i, ++pos1)
	{
		/* order */
		mmt_assert(pos1->start <= pos1->end);

		/* all regions must have an id */
		mmt_assert(pos1->id > 0);

		/* if it's a real region, it must have a file descriptor */
		if (pos1->end > 0)
			mmt_assert(pos1->fd > 0);

		pos2 = mmt_mmaps;
		for (j = 0; j <= mmt_last_region; ++j, ++pos2)
		{
			if (i == j)
				continue;

			/* start or end are not within other region */
			mmt_assert2(pos1->start < pos2->start || pos1->start >= pos2->end,
					"<%p, %p> <%p, %p>",
					(void *)pos1->start, (void *)pos1->end,
					(void *)pos2->start, (void *)pos2->end);
			mmt_assert2(pos1->end  <= pos2->start || pos1->end   >= pos2->end,
					"<%p, %p> <%p, %p>",
					(void *)pos1->start, (void *)pos1->end,
					(void *)pos2->start, (void *)pos2->end);
		}
	}

	/* positive regions after last should be empty */
	for (i = mmt_last_region + 1; i < MMT_MAX_REGIONS; ++i, ++pos1)
	{
		mmt_assert2(pos1->start == 0, "%p", (void *)pos1->start);
		mmt_assert2(pos1->end == 0, "%p", (void *)pos1->end);
		mmt_assert2(pos1->fd == 0, "%d", pos1->fd);
		mmt_assert2(pos1->id == 0, "%u", pos1->id);
		mmt_assert2(pos1->offset == 0, "%lld", pos1->offset);
	}

	if (last_used_region && last_used_region != &null_region)
	{
		/* there must be at least one positive region, we are pointing at it! */
		mmt_assert(mmt_last_region >= 0);

		/* within possible range */
		mmt_assert(last_used_region >= &mmt_mmaps[0]);
		mmt_assert(last_used_region <= &mmt_mmaps[mmt_last_region]);

		/* it must be real region */
		mmt_assert(last_used_region->start < last_used_region->end);
	}
}
static void verify_state(void)
{
#ifdef MMT_DEBUG
	__verify_state();
#endif
}

static force_inline struct mmt_mmap_data *__mmt_bsearch(Addr addr, int *next)
{
	int start = 0, end = mmt_last_region, middle;
	struct mmt_mmap_data *tmp;

	while (start <= end)
	{
		middle = start + (end - start) / 2;
		tmp = &mmt_mmaps[middle];

#ifdef MMT_DEBUG
		mmt_assert2(start >= 0 && start <= mmt_last_region,
				"%d %d", start, mmt_last_region);
		mmt_assert2(middle >= 0 && middle <= mmt_last_region,
				"%d %d", middle, mmt_last_region);
		mmt_assert2(end >= 0 && end <= mmt_last_region,
				"%d %d", end, mmt_last_region);
#endif

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

#ifdef MMT_DEBUG
	for(start = 0; start <= mmt_last_region; ++start)
		mmt_assert2(addr < mmt_mmaps[start].start || addr >= mmt_mmaps[start].end,
				"%p in %d<%p, %p>", (void *)addr, start, (void *)mmt_mmaps[start].start,
				(void *)mmt_mmaps[start].end);
	mmt_assert2(*next <= mmt_last_region + 1, "*prev: %d, mmt_last_region: %d, addr: %p",
			*next, mmt_last_region, (void *)addr);
#endif

	return NULL;
}

static void add_neg(Addr start, Addr end)
{
#ifdef MMT_DEBUG_VERBOSE
	mmt_bin_flush();
	VG_(printf)("adding negative entry: <%p, %p>\n", (void *)start, (void *)end);
#endif

	if (neg_regions_number < NEG_REGS)
		neg_regions_number++;
	neg_regions[neg_regions_number - 1].start = start;
	neg_regions[neg_regions_number - 1].end = end;
	neg_regions[neg_regions_number - 1].score = 0;

	verify_state();
}

/* finds region to which addr belongs to */
static noinline struct mmt_mmap_data *mmt_bsearch(Addr addr)
{
	struct mmt_mmap_data *region;
	int tmp;

	if (UNLIKELY(mmt_last_region < 0))
	{
		add_neg(0, (Addr)-1);
		return NULL;
	}

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
	mmt_assert(__mmt_bsearch(addr, &index) == NULL);
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

struct mmt_mmap_data *__find_mmap_slow(Addr addr)
{
	struct mmt_mmap_data *region;
	struct negative_region *neg = neg_regions;
	int i;

	/* if score of first negative entry grew too much - divide all entries;
	 * it prevents overflowing and monopoly at the top */
	if (UNLIKELY(neg->score > 1 << 24))
		for (i = 0; i < neg_regions_number; ++i)
			neg_regions[i].score >>= 10;

	/* check all negative regions */
	for (i = 1; i < neg_regions_number; ++i)
	{
		neg++;
		if (LIKELY(addr >= neg->start && addr < neg->end))
		{
			neg->score++;
			/* if current entry score is bigger than previous */
			if (UNLIKELY(neg->score > neg[-1].score))
				score_higher(i); /* then swap them */
			return NULL;
		}
	}

	region = mmt_bsearch(addr);

	if (region)
		last_used_region = region;

	return region;
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

#ifdef MMT_DEBUG_VERBOSE
	mmt_bin_flush();
	VG_(printf)("freeing region: <%p, %p>\n", (void *)start, (void *)end);
#endif

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
		last_used_region = &null_region;
	else if (last_used_region > m && last_used_region != &null_region) /* if last used region was in area which just moved */
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

	verify_state();
}

struct mmt_mmap_data *mmt_add_region(int fd, Addr start, Addr end,
		Off64T offset, UInt id)
{
	struct mmt_mmap_data *region;
	int i;
	end = (end + VKI_PAGE_SIZE - 1) & ~(VKI_PAGE_SIZE - 1);

#ifdef MMT_DEBUG_VERBOSE
	mmt_bin_flush();
	VG_(printf)("adding region: <%p, %p>\n", (void *)start, (void *)end);
#endif

	mmt_assert2(mmt_last_region + 1 < MMT_MAX_REGIONS, "not enough space for new mmap!");

	if (mmt_last_region < 0 || start >= mmt_mmaps[mmt_last_region].end)
		region = &mmt_mmaps[++mmt_last_region];
	else
	{
		i = mmt_bsearch_next(start);

		region = &mmt_mmaps[i];
		if (i != mmt_last_region + 1)
		{
			VG_(memmove)(&mmt_mmaps[i+1], &mmt_mmaps[i],
					(mmt_last_region - i  + 1) * sizeof(mmt_mmaps[0]));

			if (last_used_region >= region && last_used_region != &null_region)
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

	verify_state();

	return region;
}

static void mmt_pre_write(UWord *args)
{
	if (!mmt_trace_stdout_stderr)
		return;

	int fd = args[0];
	if (fd != 1 && fd != 2)
		return;

	void *buf = (void *)args[1];
	UInt count = args[2];

	mmt_bin_write_1('t');
	mmt_bin_write_4(fd);
	mmt_bin_write_buffer(buf, count);
	mmt_bin_end();
	mmt_bin_sync();
}

void mmt_pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs)
{
	if (syscallno == __NR_ioctl)
	{
		int fd = args[0];
		if (FD_ISSET(fd, &trace_fds) && (mmt_trace_nvidia_ioctls ||
				mmt_trace_nouveau_ioctls || mmt_trace_fglrx_ioctls))
			if (mmt_nv_ioctl_pre(args) == 0 &&
					mmt_nouveau_ioctl_pre(args) == 0 &&
					mmt_fglrx_ioctl_pre(args) == 0)
			{
				UInt id = args[1];

				mmt_bin_write_1('i');
				mmt_bin_write_4(fd);
				mmt_bin_write_4(id);
				mmt_bin_write_buffer(NULL, 0);
				mmt_bin_end();
				mmt_bin_sync();
			}
	}
	else if (syscallno == __NR_exit_group || syscallno == __NR_exit)
		mmt_bin_flush();
	else if (syscallno == __NR_write)
		mmt_pre_write(args);
}

void mmt_dump_open(UWord *args, SysRes res)
{
	const char *path = (const char *)args[0];
	int flags = (int)args[1];
	int mode = (int)args[2];

	mmt_bin_write_1('o');
	mmt_bin_write_4(flags);
	mmt_bin_write_4(mode);
	mmt_bin_write_4(res._val);
	mmt_bin_write_str(path);
	mmt_bin_end();
	mmt_bin_sync();
}

static void post_open(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	const char *path = (const char *)args[0];
	int i;

	if (res._isError)
		return;

	if (mmt_trace_all_files)
		FD_SET(res._val, &trace_fds);
	else
	{
		for (i = 0; i < MMT_MAX_TRACE_FILES; ++i)
		{
			const char *path2 = mmt_trace_files[i];
			if (path2 != NULL && VG_(strcmp)(path, path2) == 0)
			{
				FD_SET(res._val, &trace_fds);
				break;
			}
		}
	}

	if (mmt_trace_all_opens || FD_ISSET(res._val, &trace_fds))
		mmt_dump_open(args, res);
}

static void post_close(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	int fd = (int)args[0];

	if (FD_ISSET(fd, &trace_fds))
		FD_CLR(fd, &trace_fds);
}

struct mmt_mmap_data *mmt_map_region(int fd, Addr start, Addr end, Off64T offset, int prot, int flags)
{
	struct mmt_mmap_data *region = mmt_add_region(fd, start, end, offset, 0);

	mmt_bin_write_1('M');
	mmt_bin_write_8(region->offset);
	mmt_bin_write_4(prot);
	mmt_bin_write_4(flags);
	mmt_bin_write_4(fd);
	mmt_bin_write_4(region->id);
	mmt_bin_write_8(region->start);
	mmt_bin_write_8(end - start);
	mmt_bin_end();
	mmt_bin_sync();

	return region;
}

static void post_mmap(ThreadId tid, UWord *args, UInt nArgs, SysRes res, int offset_unit)
{
	Addr start = args[0];
	unsigned long len = args[1];
	unsigned long prot = args[2];
	unsigned long flags = args[3];
	unsigned long fd = args[4];
	unsigned long offset = args[5];

	if (res._isError || (int)fd == -1)
		return;

	start = res._val;

	if (!FD_ISSET(fd, &trace_fds))
		return;

	mmt_map_region(fd, start, start + len, offset * offset_unit, prot, flags);
}

void mmt_unmap_region(struct mmt_mmap_data *region)
{
	mmt_bin_write_1('u');
	mmt_bin_write_8(region->offset);
	mmt_bin_write_4(region->id);
	mmt_bin_write_8(region->start);
	mmt_bin_write_8(region->end - region->start);
	mmt_bin_write_8(0);
	mmt_bin_write_8(0);
	mmt_bin_end();
	mmt_bin_sync();

	mmt_free_region(region);
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

	mmt_unmap_region(region);
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
	region = mmt_add_region(tmp.fd, res._val, res._val + new_len, tmp.offset, tmp.id);

	mmt_bin_write_1('e');
	mmt_bin_write_8(region->offset);
	mmt_bin_write_4(region->id);
	mmt_bin_write_8(start);
	mmt_bin_write_8(old_len);
	mmt_bin_write_8(0);
	mmt_bin_write_8(0);
	mmt_bin_write_8(region->start);
	mmt_bin_write_8(region->end - region->start);
	mmt_bin_end();
	mmt_bin_sync();
}

void mmt_post_syscall(ThreadId tid, UInt syscallno, UWord *args,
			UInt nArgs, SysRes res)
{
	if (syscallno == __NR_ioctl)
	{
		int fd = args[0];
		if (FD_ISSET(fd, &trace_fds) && (mmt_trace_nvidia_ioctls ||
				mmt_trace_nouveau_ioctls || mmt_trace_fglrx_ioctls))
			if (mmt_nv_ioctl_post(args, res) == 0 &&
					mmt_nouveau_ioctl_post(args, res) == 0 &&
					mmt_fglrx_ioctl_post(args, res) == 0)
			{
				UInt id = args[1];

				mmt_bin_write_1('j');
				mmt_bin_write_4(fd);
				mmt_bin_write_4(id);
				mmt_bin_write_8(sr_Res(res));
				mmt_bin_write_8(sr_Err(res));
				mmt_bin_write_buffer(NULL, 0);
				mmt_bin_end();
				mmt_bin_sync();
			}

		mmt_bin_flush();
	}
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
	else if (syscallno == __NR_dup)
	{
		int fd = args[0];
		if (!sr_isError(res) && FD_ISSET(fd, &trace_fds))
		{
			mmt_bin_write_1('d');
			mmt_bin_write_4(fd);
			mmt_bin_write_4((int)sr_Res(res));
			mmt_bin_end();
			mmt_bin_sync();

			FD_SET(sr_Res(res), &trace_fds);
		}
	}
}

int mmt_sync_fd = -1;
static int sync_id = 0;

void mmt_emit_sync_and_wait(void)
{
	char buf[4];
	mmt_bin_write_1('S');
	mmt_bin_write_4(++sync_id);
	mmt_bin_end();
	mmt_bin_flush_and_sync();
	int cnt = 4;
	while (cnt)
	{
		int r = VG_(read)(mmt_sync_fd, buf + 4 - cnt, cnt);
		mmt_assert2(r > 0, "sync failed: %d", r);

		cnt -= r;
	}
	int ret_sync = *(int *)((void *)buf);
	mmt_assert2(ret_sync == sync_id, "%d %d", ret_sync, sync_id);
}
