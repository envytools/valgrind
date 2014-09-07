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
#include "mmt_nv_ioctl.h"
#include "mmt_trace_bin.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_vkiscnums.h"
#include "coregrind/pub_core_syscall.h"
#include "nvrm_ioctl.h"
#include "nvrm_mthd.h"

#include <sys/select.h>

#define NVRM_CLASS_SUBDEVICE_0 0x2080

static fd_set nvidiactl_fds;
static fd_set nvidia0_fds;

int mmt_trace_nvidia_ioctls = False;
int mmt_trace_marks = False;
static int trace_mark_fd;
static int trace_mark_cnt = 0;

/*
 * Binary format message subtypes: (some of them are not used anymore, so they are reserved)
 *     a = reserved (allocate map)
 *     b = reserved (bind)
 *     c = reserved (create object)
 *     d = reserved (destroy object)
 *     e = reserved (deallocate map)
 *     g = reserved (gpu map)
 *     h = reserved (gpu unmap)
 *     i = ioctl before
 *     j = ioctl after
 *     k = mark (mmiotrace)
 *     l = reserved (call method)
 *     m = mmap
 *     o = memory dump
 *     p = reserved (create mapped object)
 *     P = nouveau's GEM_PUSHBUF data
 *     r = reserved (create driver object)
 *     t = reserved (create dma object)
 *     v = reserved (create device object)
 *     x = reserved (create context object)
 *     1 = reserved (call method data)
 *     4 = ioctl 4d
 *
 */

void mmt_nv_ioctl_fini()
{
	if (mmt_trace_marks)
		VG_(close)(trace_mark_fd);
}

void mmt_nv_ioctl_post_clo_init(void)
{
	if (mmt_trace_marks)
	{
		SysRes ff;
		ff = VG_(open)("/sys/kernel/debug/tracing/trace_marker", VKI_O_WRONLY, 0777);
		if (ff._isError)
		{
			VG_(message) (Vg_UserMsg, "Cannot open marker file!\n");
			mmt_trace_marks = 0;
		}
		trace_mark_fd = ff._val;
	}
}

static struct mmt_mmap_data *get_nvidia_mapping(Off64T offset)
{
	struct mmt_mmap_data *region;

	region = mmt_find_region_by_fdset_offset(&nvidia0_fds, offset);
	if (region)
		return region;

	return mmt_add_region(0, 0, 0, offset, 0, 0, 0);
}

static Addr release_nvidia_mapping(Off64T offset)
{
	struct mmt_mmap_data *region;
	Addr start;

	region = mmt_find_region_by_fdset_offset(&nvidia0_fds, offset);
	if (!region)
		return 0;

	start = region->start;
	mmt_free_region(region);

	return start;
}

static Addr release_nvidia_mapping2(UWord data1, UWord data2)
{
	struct mmt_mmap_data *region;
	Addr start;

	region = mmt_find_region_by_fdset_data(&nvidia0_fds, data1, data2);
	if (!region)
		return 0;

	start = region->start;
	mmt_free_region(region);

	return start;
}

static void dumpmem(const char *s, Addr addr, UInt size)
{
	if (!addr || !size)
		return;

	mmt_bin_write_1('n');
	mmt_bin_write_1('o');
	mmt_bin_write_8(addr);

	if ((addr & 0xffff0000) == 0xbeef0000)
	{
		mmt_bin_write_str("");
		mmt_bin_write_buffer((UChar *)"", 0);
		mmt_bin_end();

		return;
	}

	mmt_bin_write_str(s);
	mmt_bin_write_buffer((UChar *)addr, size);
	mmt_bin_end();
}

void mmt_nv_ioctl_post_open(UWord *args, SysRes res)
{
	const char *path = (const char *)args[0];

	if (mmt_trace_nvidia_ioctls)
	{
		if (VG_(strcmp)(path, "/dev/nvidiactl") == 0)
			FD_SET(res._val, &nvidiactl_fds);
		else if (VG_(strncmp)(path, "/dev/nvidia", 11) == 0)
			FD_SET(res._val, &nvidia0_fds);
	}
}

void mmt_nv_ioctl_post_close(UWord *args)
{
	int fd = (int)args[0];

	if (mmt_trace_nvidia_ioctls)
	{
		FD_CLR(fd, &nvidiactl_fds);
		FD_CLR(fd, &nvidia0_fds);
	}
}

int mmt_nv_ioctl_post_mmap(UWord *args, SysRes res, int offset_unit)
{
	Addr start = args[0];
	unsigned long len = args[1];
//	unsigned long prot = args[2];
//	unsigned long flags = args[3];
	unsigned long fd = args[4];
	unsigned long offset = args[5];
	struct mmt_mmap_data *region;
	struct mmt_mmap_data tmp;

	if (!mmt_trace_nvidia_ioctls)
		return 0;
	if (!FD_ISSET(fd, &nvidia0_fds))
		return 0;

	region = mmt_find_region_by_fd_offset(fd, offset * offset_unit);
	if (!region)
		return 0;

	tmp = *region;

	mmt_free_region(region);

	start = res._val;
	region = mmt_add_region(fd, start, start + len, tmp.offset, tmp.id, tmp.data1, tmp.data2);

	mmt_bin_write_1('n');
	mmt_bin_write_1('m');
	mmt_bin_write_8(region->offset);
	mmt_bin_write_4(region->id);
	mmt_bin_write_8(region->start);
	mmt_bin_write_8(len);
	mmt_bin_write_8(region->data1);
	mmt_bin_write_8(region->data2);
	mmt_bin_end();

	return 1;
}

static struct object_type {
	UInt id;		// type id
	UInt cargs;		// number of constructor args (uint32)
} object_types[] =
{
	{0x0000, 1},
	{0x0005, 6},
	{0x0019, 4}, // +NULL
	{0x0039, 4}, // +NULL
	{0x0041, 1},
	{0x0043, 4}, // +NULL
	{0x0044, 4}, // +NULL
	{0x004a, 4}, // +NULL
	{0x3062, 4}, // +NULL
	{0x3066, 4}, // +NULL
	{0x406e, 8},
	{0x0072, 4}, // +NULL
	{0x0073, 0},
	{0x0079, 6},
	{0x307b, 4}, // +NULL
	{0x357c, 3}, // +NULL
	{0x0080, 8},
	{0x2080, 1},
	{0x3089, 4}, // +NULL
	{0x308a, 4}, // +NULL
	{0x9096, 0},
	{0x4097, 4}, // +NULL
	{0x309e, 4}, // +NULL
	{0x009f, 4}, // +NULL
};

static struct object_type *find_objtype(UInt id)
{
	int i;
	int n = sizeof(object_types) / sizeof(struct object_type);

	for (i = 0; i < n; ++i)
		if (object_types[i].id == id)
			return &object_types[i];

	return NULL;
}

static void handle_nvrm_ioctl_call(struct nvrm_ioctl_call *s, int in)
{
	void *ptr = (void *)(unsigned long)s->ptr;
	const char *str;
	if (in)
		str = "in";
	else
		str = "out";

	dumpmem(str, s->ptr, s->size);

	if (s->mthd == 0x10000002)
	{
		UInt *addr2 = ptr;
		dumpmem(str, addr2[2], 0x3c);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_BAR0)
	{
		struct nvrm_mthd_subdevice_bar0 *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4 * 8);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_GET_CLASSES)
	{
		struct nvrm_mthd_device_get_classes *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_FB_GET_PARAMS)
	{
		struct nvrm_mthd_subdevice_fb_get_params *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4 * 2);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_BUS_GET_PARAMS)
	{
		struct nvrm_mthd_subdevice_bus_get_params *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4 * 2);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1401)
	{
		struct nvrm_mthd_device_unk1401 *m = ptr;
		dumpmem(str, m->ptr, m->cnt);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_GET_FIFO_ENGINES)
	{
		struct nvrm_mthd_subdevice_get_fifo_engines *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1102)
	{
		struct nvrm_mthd_device_unk1102 *m = ptr;
		dumpmem(str, m->ptr, m->cnt);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1701)
	{
		struct nvrm_mthd_device_unk1701 *m = ptr;
		dumpmem(str, m->ptr, m->cnt);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK1201)
	{
		struct nvrm_mthd_subdevice_unk1201 *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4 * 2);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0101)
	{
		struct nvrm_mthd_subdevice_unk0101 *m = ptr;
		dumpmem(str, m->ptr, m->cnt * 4 * 2);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0522)
	{
		struct nvrm_mthd_subdevice_unk0522 *m = ptr;
		dumpmem(str, m->ptr, m->size);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0512)
	{
		struct nvrm_mthd_subdevice_unk0512 *m = ptr;
		dumpmem(str, m->ptr, m->size);
	}
}

void mmt_nv_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;

	if (!FD_ISSET(fd, &nvidiactl_fds) && !FD_ISSET(fd, &nvidia0_fds))
		return;
	if (!data)
		return;

	if (mmt_trace_marks)
	{
		char buf[50];
		VG_(snprintf)(buf, 50, "VG-%d-%d-PRE\n", VG_(getpid)(), trace_mark_cnt);
		VG_(write)(trace_mark_fd, buf, VG_(strlen)(buf));

		mmt_bin_write_1('n');
		mmt_bin_write_1('k');
		mmt_bin_write_str(buf);
		mmt_bin_end();
	}

	if ((id & 0x0000FF00) == 0x4600)
	{
		size = (id & 0x3FFF0000) >> 16;

		mmt_bin_write_1('n');
		mmt_bin_write_1('i');
		mmt_bin_write_4(fd);
		mmt_bin_write_4(id);
		mmt_bin_write_buffer((UChar *)data, size);
		mmt_bin_end();
	}
	else
		VG_(message)(Vg_UserMsg, "pre_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		case NVRM_IOCTL_CREATE_DEV_OBJ:
		{
			struct nvrm_ioctl_create_dev_obj *s = (void *)data;

			// argument can be a string (7:0, indicating the bus number), but only if
			// argument is 0xff
			dumpmem("in ", s->ptr, 0x3C);

			break;
		}
		case NVRM_IOCTL_UNK38:
		{
			struct nvrm_ioctl_unk38 *s = (void *)data;
			dumpmem("in ", s->ptr, s->size);
			break;
		}
		case NVRM_IOCTL_QUERY:
		{
			struct nvrm_ioctl_query *s = (void *)data;
			dumpmem("in ", s->ptr, s->size);
			break;
		}
		case NVRM_IOCTL_CALL:
		{
			handle_nvrm_ioctl_call((void *)data, 1);
			break;
		}
		case NVRM_IOCTL_UNK41:
		{
			struct nvrm_ioctl_unk41 *s = (void *)data;
			dumpmem("in", s->ptr1, 4);
			dumpmem("in", s->ptr2, 4);
			dumpmem("in", s->ptr3, 4);
			break;
		}
		case NVRM_IOCTL_UNK4D:
		{
			struct nvrm_ioctl_unk4d *s = (void *)data;
			dumpmem("in", s->sptr, s->slen);
			break;
		}
		case NVRM_IOCTL_UNK4D_OLD:
		{
			struct nvrm_ioctl_unk4d_old *s = (void *)data;

			// todo: convert to dumpmem once spotted
			mmt_bin_write_1('n');
			mmt_bin_write_1('4');
			mmt_bin_write_str((void *)(unsigned long)s->ptr);
			mmt_bin_end();
			break;
		}
		case NVRM_IOCTL_UNK52:
		{
			struct nvrm_ioctl_unk52 *s = (void *)data;
			dumpmem("in", s->ptr, 8);
			break;
		}
		case NVRM_IOCTL_UNK5E:
		{
			// Copy data from mem to GPU
			struct nvrm_ioctl_unk5e *s = (void *)data;
			if (0)
				dumpmem("in", s->ptr, 0x01000000);
			break;
		}
		case NVRM_IOCTL_CREATE:
		{
			struct nvrm_ioctl_create *s = (void *)data;
			struct object_type *objtype = find_objtype(s->cls);

			if (s->ptr && objtype)
				dumpmem("in ", s->ptr, objtype->cargs * 4);

			break;
		}
	}
}

void mmt_nv_ioctl_post(UWord *args, SysRes res)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;
	struct mmt_mmap_data *region;

	if (!FD_ISSET(fd, &nvidiactl_fds) && !FD_ISSET(fd, &nvidia0_fds))
		return;
	if (!data)
		return;

	if (sr_Res(res) || sr_isError(res))
		VG_(message)(Vg_UserMsg, "ioctl result: %ld (0x%lx), iserr: %d, err: %ld (0x%lx)\n", sr_Res(res), sr_Res(res), sr_isError(res), sr_Err(res), sr_Err(res));

	if (mmt_trace_marks)
	{
		char buf[50];
		VG_(snprintf)(buf, 50, "VG-%d-%d-POST\n", VG_(getpid)(), trace_mark_cnt++);
		VG_(write)(trace_mark_fd, buf, VG_(strlen)(buf));

		mmt_bin_write_1('n');
		mmt_bin_write_1('k');
		mmt_bin_write_str(buf);
		mmt_bin_end();
	}

	if ((id & 0x0000FF00) == 0x4600)
	{
		size = (id & 0x3FFF0000) >> 16;

		mmt_bin_write_1('n');
		mmt_bin_write_1('j');
		mmt_bin_write_4(fd);
		mmt_bin_write_4(id);
		mmt_bin_write_buffer((UChar *)data, size);
		mmt_bin_end();
	}
	else
		VG_(message)(Vg_UserMsg, "post_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		case NVRM_IOCTL_CREATE_DEV_OBJ:
		{
			struct nvrm_ioctl_create_dev_obj *s = (void *)data;
			dumpmem("out", s->ptr, 0x3C);
			break;
		}
		case NVRM_IOCTL_HOST_MAP: // Allocate map for existing object
		{
			struct nvrm_ioctl_host_map *s = (void *)data;

			region = get_nvidia_mapping(s->foffset);
			if (region)
			{
				region->data1 = s->subdev;
				region->data2 = s->handle;
			}

			break;
		}
		case NVRM_IOCTL_HOST_UNMAP: // Deallocate map for existing object
		{
			struct nvrm_ioctl_host_unmap *s = (void *)data;
			/// XXX some currently mapped memory might be orphaned

			release_nvidia_mapping(s->foffset);

			break;
		}
		case NVRM_IOCTL_CREATE_VSPACE: // Allocate map (also create object)
		{
			struct nvrm_ioctl_create_vspace *s = (void *)data;
			if (s->foffset == 0)
				break;

			region = get_nvidia_mapping(s->foffset);
			if (region)
			{
				region->data1 = s->parent;
				region->data2 = s->handle;
			}
			break;
		}
		case NVRM_IOCTL_DESTROY:
		{
			struct nvrm_ioctl_destroy *s = (void *)data;
			/// XXX some currently mapped memory might be orphaned

			release_nvidia_mapping2(s->parent, s->handle);
			break;
		}
		case NVRM_IOCTL_CALL:
		{
			handle_nvrm_ioctl_call((void *)data, 0);
			break;
		}
		case NVRM_IOCTL_UNK41:
		{
			struct nvrm_ioctl_unk41 *s = (void *)data;
			dumpmem("out", s->ptr1, 4);
			dumpmem("out", s->ptr2, 4);
			dumpmem("out", s->ptr3, 4);
			break;
		}
		case NVRM_IOCTL_UNK4D:
		{
			struct nvrm_ioctl_unk4d *s = (void *)data;
			dumpmem("out", s->sptr, s->slen);
			break;
		}
		case NVRM_IOCTL_UNK38:
		{
			struct nvrm_ioctl_unk38 *s = (void *)data;
			dumpmem("out", s->ptr, s->size);
			break;
		}
		case NVRM_IOCTL_UNK52:
		{
			struct nvrm_ioctl_unk52 *s = (void *)data;
			dumpmem("out", s->ptr, 8);
			break;
		}
		case NVRM_IOCTL_QUERY:
		{
			struct nvrm_ioctl_query *s = (void *)data;

			UInt *addr2 = (UInt *)(unsigned long)s->ptr;
			dumpmem("out", s->ptr, s->size);
			if (s->query == 0x14c && addr2[2])
				// List supported object types
				dumpmem("out2 ", addr2[2], addr2[0] * 4);

			break;
		}
		case NVRM_IOCTL_CREATE:
		{
			struct nvrm_ioctl_create *s = (void *)data;
			struct object_type *objtype = find_objtype(s->cls);

			if (s->ptr && objtype)
				dumpmem("out", s->ptr, objtype->cargs * 4);

			if (s->cls == NVRM_CLASS_SUBDEVICE_0)
			{
				// inject GET_CHIPSET ioctl
				struct nvrm_mthd_subdevice_get_chipset chip;
				VG_(memset)(&chip, 0, sizeof(chip));

				struct nvrm_ioctl_call call;
				VG_(memset)(&call, 0, sizeof(call));
				call.cid = s->cid;
				call.handle = s->handle;
				call.mthd = NVRM_MTHD_SUBDEVICE_GET_CHIPSET;
				call.ptr = (unsigned long) &chip;
				call.size = sizeof(chip);
				UWord ioctlargs[3] = { fd, (UWord)NVRM_IOCTL_CALL, (UWord)&call };

				mmt_nv_ioctl_pre(ioctlargs);
				SysRes ioctlres = VG_(do_syscall3)(__NR_ioctl, fd, (UWord)NVRM_IOCTL_CALL, (UWord)&call);
				mmt_nv_ioctl_post(ioctlargs, ioctlres);
			}

			break;
		}
	}
}

void mmt_nv_ioctl_pre_clo_init(void)
{
	FD_ZERO(&nvidiactl_fds);
	FD_ZERO(&nvidia0_fds);
}
