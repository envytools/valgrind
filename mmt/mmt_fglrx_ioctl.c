/*
   Copyright (C) 2015 Marcin Ślusarz <marcin.slusarz@gmail.com>

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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <inttypes.h>

#include "pub_tool_libcassert.h"
#include "pub_tool_vkiscnums.h"
#include "coregrind/pub_core_syscall.h"
#include "coregrind/pub_core_aspacemgr.h"

#include "fglrx_ioctl.h"
#include "mmt_fglrx_ioctl.h"
#include "mmt_trace_bin.h"

static const int dump_maps = 0;
static const int fuzzer_enabled = 0;

int mmt_trace_fglrx_ioctls;

#define noinline	__attribute__((noinline))

static noinline void dumpmem(Addr addr, UInt size)
{
	if (!addr)
		return;

	mmt_bin_write_1('y');
	mmt_bin_write_8(addr);
	mmt_bin_write_buffer((const UChar *)addr, size);
	mmt_bin_end();
}

static void *test_page = NULL;
static void *inaccessible_page = NULL;

#define MMT_ALLOC_SIZE 4096

#ifdef MMT_64BIT
#define MMT_INITIAL_OFFSET 0x100000000
#else
#define MMT_INITIAL_OFFSET 0x10000000
#endif

static inline void *u64_to_ptr(uint64_t u)
{
	return (void *)(unsigned long)u;
}

static inline uint64_t ptr_to_u64(void *ptr)
{
	return (uint64_t)(unsigned long)ptr;
}

static int test_page_available(void)
{
	if (test_page)
		return 1;

	SysRes res, res2;
	int offset = 0;

	do
	{
		res = VG_(am_do_mmap_NO_NOTIFY)(MMT_INITIAL_OFFSET + offset, MMT_ALLOC_SIZE,
				PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (sr_isError(res))
		{
			offset += MMT_ALLOC_SIZE;
			continue;
		}
		res2 = VG_(am_do_mmap_NO_NOTIFY)(MMT_INITIAL_OFFSET + offset + MMT_ALLOC_SIZE,
				MMT_ALLOC_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (sr_isError(res2))
		{
			VG_(do_syscall2)(__NR_munmap, sr_Res(res), MMT_ALLOC_SIZE);
			offset += MMT_ALLOC_SIZE;
			continue;
		}
	}
	while (sr_isError(res) || sr_isError(res2));

	test_page = (void *)sr_Res(res);
	inaccessible_page = (void *)sr_Res(res2);
	return 1;
}

static int in_fuzzer_mode = 0;

static void dump_ioctl_data(UInt id, void *data)
{
	if (fuzzer_enabled)
		return;

	switch (id)
	{
		case FGLRX_IOCTL_DRV_INFO:
		{
			struct fglrx_ioctl_drv_info *f = data;
			if (f->drv_id)
				dumpmem((Addr)f->drv_id, f->drv_id_len);
			if (f->drv_date)
				dumpmem((Addr)f->drv_date, f->drv_date_len);
			if (f->drv_name)
				dumpmem((Addr)f->drv_name, f->drv_name_len);
			break;
		}
		case FGLRX_IOCTL_GET_BUS_ID:
		{
			struct fglrx_ioctl_get_bus_id *f = data;
			if (f->bus_id)
				dumpmem((Addr)f->bus_id, f->bus_id_len);
			break;
		}
		case FGLRX_IOCTL_4F:
		{
			struct fglrx_ioctl_4f *f = data;
			if (f->ptr1)
				;//dumpmem((Addr)f->ptr1, 0);//TODO
			break;
		}
		case FGLRX_IOCTL_50:
		{
			struct fglrx_ioctl_50 *f = data;
			if (f->kernel_ver)
				dumpmem((Addr)f->kernel_ver, f->kernel_ver_len);
			break;
		}
		case FGLRX_IOCTL_64:
		{
			struct fglrx_ioctl_64 *f = data;
			if (f->ptr1)
				;//dumpmem((Addr)f->ptr1, 0);//TODO
			break;
		}
		case FGLRX_IOCTL_68:
		{
			struct fglrx_ioctl_68 *f = data;
			if (f->ptr1)
				;//dumpmem((Addr)f->ptr1, 0);//TODO
			if (f->ptr2)
				;//dumpmem((Addr)f->ptr2, 0);//TODO
			break;
		}
		case FGLRX_IOCTL_CONFIG:
		{
			struct fglrx_ioctl_config *f = data;
			if (f->username)
				dumpmem((Addr)f->username, f->username_len);
			if (f->namespace)
				dumpmem((Addr)f->namespace, f->namespace_len);
			if (f->prop_name)
				dumpmem((Addr)f->prop_name, f->prop_name_len);
			if (f->prop_value)
				dumpmem((Addr)f->prop_value, f->prop_value_len);
			break;
		}
		case FGLRX_IOCTL_A6:
		{
			struct fglrx_ioctl_a6 *f = data;
			if (f->ptr1)
				dumpmem((Addr)f->ptr1, f->len1);
			if (f->ptr2)
				dumpmem((Addr)f->ptr2, f->len2);
			break;
		}

		default:
			break;
	}
}

static void *read_process_maps(int *len)
{
#define MAX_MAPS_LEN (1<<16)
	static char data[MAX_MAPS_LEN];
	int fd = VG_(fd_open)("/proc/self/maps", VKI_O_RDONLY, 0);
	if (fd == -1)
	{
		*len = 0;
		return NULL;
	}

	int curlen = 0;
	int r;
	do
	{
		r = VG_(read)(fd, data + curlen, MAX_MAPS_LEN - 1 - curlen);
		if (r > 0)
			curlen += r;
	}
	while (r != 0);

	VG_(close)(fd);

	data[curlen] = 0;

	*len = curlen;
	return data;
}

static void dump_process_maps(void)
{
	int len;
	const char *data = read_process_maps(&len);
	if (len == 0)
		return;

	mmt_bin_write_1('y');
	mmt_bin_write_8(1);
	mmt_bin_write_buffer((const UChar *)data, len);
	mmt_bin_end();
}

struct ati_card0_map
{
	Addr start;
	Addr end;
	Off64T offset;
	int prot;
	int flags;
	struct mmt_mmap_data *region;
};

#define MAX_MAPS 1000
static int ati_card0_map_len;
static struct ati_card0_map ati_card0_maps[MAX_MAPS];

static void update_maps(int fd)
{
	int i, j, len;
	char *data = read_process_maps(&len);
	if (len == 0)
		return;

	int num = 0;
	static struct ati_card0_map maps[MAX_MAPS];

	HChar *line = VG_(strtok)(data, "\n");
	HChar *end;

	while (line && num < MAX_MAPS)
	{
		if (VG_(strstr)(line, "/dev/ati/card0"))
		{
			maps[num].start = VG_(strtoull16)(line, &end);
			line = end + 1; // "-"
			maps[num].end = VG_(strtoull16)(line, &end);
			line = end + 1; // " "
			int prot = PROT_NONE;
			int flags = 0;
			if (*line++ == 'r')
				prot |= PROT_READ;
			if (*line++ == 'w')
				prot |= PROT_WRITE;
			if (*line++ == 'x')
				prot |= PROT_EXEC;
			maps[num].prot = prot;

			if (*line == 's')
				flags |= MAP_SHARED;
			else if (*line == 'p')
				flags |= MAP_PRIVATE;
			line++;
			maps[num].flags = flags;

			line++; // " "
			maps[num].offset = VG_(strtoull16)(line, &end);

			num++;
		}
		tl_assert(num <= MAX_MAPS);

		line = VG_(strtok)(NULL, "\n");
	}

	for (i = 0; i < ati_card0_map_len; ++i)
	{
		int found = 0;
		for (j = 0; j < num && !found; ++j)
			found = ati_card0_maps[i].start == maps[j].start &&
					ati_card0_maps[i].end == maps[j].end &&
					ati_card0_maps[i].offset == maps[j].offset &&
					ati_card0_maps[i].prot == maps[j].prot &&
					ati_card0_maps[i].flags == maps[j].flags;

		if (!found)
		{
			mmt_bin_flush();
			VG_(message)(Vg_UserMsg, "ioctl munmap\n");

			mmt_unmap_region(ati_card0_maps[i].region);

			VG_(memmove)(&ati_card0_maps[i], &ati_card0_maps[i + 1],
					sizeof(ati_card0_maps[i]) * (ati_card0_map_len - i - 1));
			ati_card0_map_len--;
			i--;
		}
	}

	for (j = 0; j < num; ++j)
	{
		int found = find_mmap(maps[j].start) != NULL;
		for (i = 0; i < ati_card0_map_len && !found; ++i)
			found = ati_card0_maps[i].start == maps[j].start &&
					ati_card0_maps[i].end == maps[j].end &&
					ati_card0_maps[i].offset == maps[j].offset &&
					ati_card0_maps[i].prot == maps[j].prot &&
					ati_card0_maps[i].flags == maps[j].flags;

		if (!found)
		{
			mmt_bin_flush();
			VG_(message)(Vg_UserMsg, "ioctl mmap\n");

			ati_card0_maps[ati_card0_map_len] = maps[j];
			ati_card0_maps[ati_card0_map_len].region = mmt_map_region(fd,
					maps[j].start, maps[j].end, maps[j].offset, maps[j].prot,
					maps[j].flags);
			ati_card0_map_len++;
		}
	}
}

int mmt_fglrx_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;

	if (!mmt_trace_fglrx_ioctls)
		return 0;

	if ((id & 0x0000FF00) != 0x6400)
		return 0;

	size = (id & 0x3FFF0000) >> 16;

	mmt_bin_write_1('i');
	mmt_bin_write_4(fd);
	mmt_bin_write_4(id);
	mmt_bin_write_buffer((const UChar *)data, size);
	mmt_bin_end();

	if (dump_maps)
		dump_process_maps();

#define FFIELD ptr1
#define FIOCTL FGLRX_IOCTL_4F
#define FSTRUCT struct fglrx_ioctl_4f
	if (fuzzer_enabled && id == FIOCTL && !in_fuzzer_mode && test_page_available())
	{
		FSTRUCT *s = (void *)data;
		FSTRUCT ioc;
		int test_size = -1;
		SysRes res;
		int err;

		mmt_bin_flush();

		if (s->FFIELD)
		{
			uint64_t addr;
			mmt_bin_flush();
			VG_(message)(Vg_UserMsg, "start\n");
			do
			{
				VG_(memcpy)(&ioc, s, sizeof(ioc));

				test_size++;
				addr = ioc.FFIELD = ptr_to_u64(inaccessible_page) - test_size;
				VG_(memcpy)(u64_to_ptr(ioc.FFIELD), u64_to_ptr(s->FFIELD), test_size);

				UWord new_args[3] = { fd, (UWord)FIOCTL, (UWord)&ioc };

				in_fuzzer_mode = 1;
				mmt_fglrx_ioctl_pre(new_args);
				res = VG_(do_syscall3)(__NR_ioctl, new_args[0], new_args[1], new_args[2]);
				mmt_fglrx_ioctl_post(new_args, res);
				in_fuzzer_mode = 0;
				err = sr_isError(res) ? 1 : 0;
			}
			while (err && test_size < 4000);

			if (!err)
			{
				dumpmem(addr, test_size);
				mmt_bin_flush();
				VG_(message)(Vg_UserMsg,
						"minimal argument size: %d (0x%x) bytes (%d words + %d bytes)\n", test_size, test_size, test_size / 4, test_size & 3);
			}
			else
			{
				mmt_bin_flush();
				VG_(message)(Vg_UserMsg,
						"could not detect minimal argument size\n");
			}
		}
	}
#undef FFIELD
#undef FIOCTL
#undef FSTRUCT

	dump_ioctl_data(id, data);

	mmt_bin_sync();

	return 1;

}

int mmt_fglrx_ioctl_post(UWord *args, SysRes res)
{
	int fd = args[0];
	UInt id = args[1];
	void *data = (void *) args[2];
	UInt size;

	if (!mmt_trace_fglrx_ioctls)
		return 0;

	if ((id & 0x0000FF00) != 0x6400)
		return 0;

	size = (id & 0x3FFF0000) >> 16;

	update_maps(fd);

	mmt_bin_write_1('j');
	mmt_bin_write_4(fd);
	mmt_bin_write_4(id);
	mmt_bin_write_8(sr_Res(res));
	mmt_bin_write_8(sr_Err(res));
	mmt_bin_write_buffer((const UChar *)data, size);
	mmt_bin_end();

	if (dump_maps)
		dump_process_maps();

	dump_ioctl_data(id, data);

	mmt_bin_sync();

	return 1;
}
