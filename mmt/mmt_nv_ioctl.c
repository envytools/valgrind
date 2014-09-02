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
#include "mmt_trace.h"
#include "mmt_trace_bin.h"
#include "pub_tool_vki.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcproc.h"
#include "pub_tool_libcassert.h"
#include "nvrm_ioctl.h"
#include "nvrm_mthd.h"

#include <sys/select.h>

static fd_set nvidiactl_fds;
static fd_set nvidia0_fds;

int mmt_trace_nvidia_ioctls = False;
int mmt_trace_marks = False;
static int trace_mark_fd;
static int trace_mark_cnt = 0;

/*
 * Binary format message subtypes:
 *     a = allocate map
 *     b = bind
 *     c = create object
 *     d = destroy object
 *     e = deallocate map
 *     g = gpu map
 *     h = gpu unmap
 *     i = ioctl before
 *     j = ioctl after
 *     k = mark (mmiotrace)
 *     l = call method
 *     m = mmap
 *     o = memory dump
 *     p = create mapped object
 *     P = nouveau's GEM_PUSHBUF data
 *     r = create driver object
 *     t = create dma object
 *     v = create device object
 *     x = create context object
 *     1 = call method data
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
	char line[4096];
	int idx = 0;
	line[0] = 0;

	UInt i;
	if (mmt_binary_output)
	{
		mmt_bin_write_1('n');
		mmt_bin_write_1('o');
		mmt_bin_write_8(addr);
	}

	if (!addr || (addr & 0xffff0000) == 0xbeef0000)
	{
		if (mmt_binary_output)
		{
			mmt_bin_write_str("");
			mmt_bin_write_buffer((UChar *)"", 0);
			mmt_bin_end();
		}
		return;
	}

	if (mmt_binary_output)
	{
		mmt_bin_write_str(s);
		mmt_bin_write_buffer((UChar *)addr, size);
		mmt_bin_end();
	}
	else
	{
		size = size / 4;

		for (i = 0; i < size; ++i)
		{
			if (idx + 11 >= 4095)
				break;
			VG_(sprintf) (line + idx, "0x%08x ", ((UInt *) addr)[i]);
			idx += 11;
		}
		VG_(message) (Vg_DebugMsg, "%s%s\n", s, line);
	}
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

	if (mmt_binary_output)
	{
		mmt_bin_write_1('n');
		mmt_bin_write_1('m');
		mmt_bin_write_8(region->offset);
		mmt_bin_write_4(region->id);
		mmt_bin_write_8(region->start);
		mmt_bin_write_8(len);
		mmt_bin_write_8(region->data1);
		mmt_bin_write_8(region->data2);
		mmt_bin_end();
	}
	else
		VG_(message) (Vg_DebugMsg,
				"got new mmap for 0x%08lx:0x%08lx at %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
				region->data1, region->data2, (void *)region->start, len,
				region->offset, region->id);

	return 1;
}

static struct object_type {
	UInt id;		// type id
	const char *name;		// some name
	UInt cargs;		// number of constructor args (uint32)
} object_types[] =
{
	{0x0000, "NV_CONTEXT_NEW", 0},

	{0x0004, "NV_PTIMER", 0},

	{0x0041, "NV_CONTEXT", 0},

	{0x502d, "NV50_2D", 0},
	{0x902d, "NVC0_2D", 0},

	{0x5039, "NV50_M2MF", 0},
	{0x9039, "NVC0_M2MF", 0},

	{0x9068, "NVC0_PEEPHOLE", 0},

	{0x406e, "NV40_FIFO_DMA", 6},

	{0x506f, "NV50_FIFO_IB", 6},
	{0x826f, "NV84_FIFO_IB", 6},
	{0x906f, "NVC0_FIFO_IB", 6},

	{0x5070, "NV84_DISPLAY", 4},
	{0x8270, "NV84_DISPLAY", 4},
	{0x8370, "NVA0_DISPLAY", 4},
	{0x8870, "NV98_DISPLAY", 4},
	{0x8570, "NVA3_DISPLAY", 4},

	{0x5072, NULL, 8},

	{0x7476, "NV84_VP", 0},

	{0x507a, "NV50_DISPLAY_CURSOR", 0},
	{0x827a, "NV84_DISPLAY_CURSOR", 0},
	{0x857a, "NVA3_DISPLAY_CURSOR", 0},

	{0x507b, "NV50_DISPLAY_OVERLAY", 0},
	{0x827b, "NV84_DISPLAY_OVERLAY", 0},
	{0x857b, "NVA3_DISPLAY_OVERLAY", 0},

	{0x507c, "NV50_DISPLAY_SYNC_FIFO", 8},
	{0x827c, "NV84_DISPLAY_SYNC_FIFO", 8},
	{0x837c, "NVA0_DISPLAY_SYNC_FIFO", 8},
	{0x857c, "NVA3_DISPLAY_SYNC_FIFO", 8},

	{0x507d, "NV50_DISPLAY_MASTER_FIFO", 0},
	{0x827d, "NV84_DISPLAY_MASTER_FIFO", 0},
	{0x837d, "NVA0_DISPLAY_MASTER_FIFO", 0},
	{0x887d, "NV98_DISPLAY_MASTER_FIFO", 0},
	{0x857d, "NVA3_DISPLAY_MASTER_FIFO", 0},

	{0x307e, "NV30_PEEPHOLE", 0},

	{0x507e, "NV50_DISPLAY_OVERLAY_FIFO", 8},
	{0x827e, "NV84_DISPLAY_OVERLAY_FIFO", 8},
	{0x837e, "NVA0_DISPLAY_OVERLAY_FIFO", 8},
	{0x857e, "NVA3_DISPLAY_OVERLAY_FIFO", 8},

	{0x0080, "NV_DEVICE", 1},
	{0x2080, "NV_SUBDEVICE_0", 0},
	{0x2081, "NV_SUBDEVICE_1", 0},
	{0x2082, "NV_SUBDEVICE_2", 0},
	{0x2083, "NV_SUBDEVICE_3", 0},

	{0x5097, "NV50_3D", 0},
	{0x8297, "NV84_3D", 0},
	{0x8397, "NVA0_3D", 0},
	{0x8597, "NVA3_3D", 0},
	{0x8697, "NVAF_3D", 0},
	{0x9097, "NVC0_3D", 0},

	{0x74b0, "NV84_BSP", 0},

	{0x88b1, "NV98_BSP", 0},
	{0x85b1, "NVA3_BSP", 0},
	{0x86b1, "NVAF_BSP", 0},
	{0x90b1, "NVC0_BSP", 0},

	{0x88b2, "NV98_VP", 0},
	{0x85b2, "NVA3_VP", 0},
	{0x90b2, "NVC0_VP", 0},

	{0x88b3, "NV98_PPP", 0},
	{0x85b3, "NVA3_PPP", 0},
	{0x90b3, "NVC0_PPP", 0},

	{0x88b4, "NV98_CRYPT", 0},

	{0x85b5, "NVA3_COPY", 0},
	{0x90b5, "NVC0_COPY0", 0},

	{0x50c0, "NV50_COMPUTE", 0},
	{0x85c0, "NVA3_COMPUTE", 0},
	{0x90c0, "NVC0_COMPUTE", 0},

	{0x74c1, "NV84_CRYPT", 0},

	{0x50e0, "NV50_PGRAPH", 0},
	{0x50e2, "NV50_PFIFO", 0},
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

void mmt_nv_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;
	int i;

	if (!FD_ISSET(fd, &nvidiactl_fds) && !FD_ISSET(fd, &nvidia0_fds))
		return;

	if (mmt_trace_marks)
	{
		char buf[50];
		VG_(snprintf)(buf, 50, "VG-%d-%d-PRE\n", VG_(getpid)(), trace_mark_cnt);
		VG_(write)(trace_mark_fd, buf, VG_(strlen)(buf));
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('k');
			mmt_bin_write_str(buf);
			mmt_bin_end();
		}
		else
			VG_(message)(Vg_DebugMsg, "MARK: %s", buf);
	}

	if ((id & 0x0000FF00) == 0x4600)
	{
		size = (id & 0x3FFF0000) >> 16;
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('i');
			mmt_bin_write_4(fd);
			mmt_bin_write_4(id);
			mmt_bin_write_buffer((UChar *)data, size);
			mmt_bin_end();
		}
		else
		{
			char line[4096];
			int idx = 0;

			VG_(sprintf) (line, "pre_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
			idx = VG_(strlen(line));

			for (i = 0; i < size / 4; ++i)
			{
				if (idx + 11 >= 4095)
					break;
				VG_(sprintf) (line + idx, "0x%08x ", data[i]);
				idx += 11;
			}
			VG_(message) (Vg_DebugMsg, "%s\n", line);
		}
	}
	else
		VG_(message)(Vg_UserMsg, "pre_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		case NVRM_IOCTL_CREATE_DEV_OBJ:
		{
			struct nvrm_ioctl_create_dev_obj *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('v');
				mmt_bin_write_4(s->handle);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "create device object 0x%08x\n", s->handle);

			// argument can be a string (7:0, indicating the bus number), but only if
			// argument is 0xff
			dumpmem("in ", s->ptr, 0x3C);

			break;
		}
		case NVRM_IOCTL_UNK38:
		{
			struct nvrm_ioctl_unk38 *s = (void *)data;
			dumpmem("in ", s->ptr, s->unk18);
			break;
		}
		case NVRM_IOCTL_QUERY:
		{
			struct nvrm_ioctl_query *s = (void *)data;
			dumpmem("in ", s->ptr, s->status);//should be s/status/size/ ?
			break;
		}
		case NVRM_IOCTL_CALL:
		{
			struct nvrm_ioctl_call *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('l');
				mmt_bin_write_4(s->handle);
				mmt_bin_write_4(s->mthd);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "call method 0x%08x:0x%08x\n", s->handle, s->mthd);

			dumpmem("in ", s->ptr, s->size);

			if (s->mthd == 0x10000002)
			{
				UInt *addr2 = (UInt *) (unsigned long)s->ptr;
				dumpmem("in2 ", addr2[2], 0x3c);
			}
			else if (s->mthd == NVRM_MTHD_SUBDEVICE_BAR0)
			{
				struct nvrm_mthd_subdevice_bar0 *m = (void *)(unsigned long)s->ptr;
				UInt *tx = (UInt *)(unsigned long)m->ptr;
				if (mmt_binary_output)
				{
					mmt_bin_write_1('n');
					mmt_bin_write_1('1');
					mmt_bin_write_4(m->cnt);
					mmt_bin_write_8(m->ptr);
					mmt_bin_write_buffer((UChar *)tx, m->cnt * 8 * 4);
					mmt_bin_end();
				}
				else
				{
					UInt k;
					VG_(message) (Vg_DebugMsg, "<==(%u at %p)\n", m->cnt, tx);

					for (k = 0; k < m->cnt; ++k)
						VG_(message) (Vg_DebugMsg, "REQUEST: DIR=%x MMIO=%x VALUE=%08x MASK=%08x UNK=%08x,%08x,%08x,%08x\n",
									tx[k * 8 + 0],
									tx[k * 8 + 3],
									tx[k * 8 + 5],
									tx[k * 8 + 7],
									tx[k * 8 + 1],
									tx[k * 8 + 2],
									tx[k * 8 + 4],
									tx[k * 8 + 6]);
				}
			}

			break;
		}
		case NVRM_IOCTL_UNK4D_OLD:
		{
			struct nvrm_ioctl_unk4d_old *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('4');
				mmt_bin_write_str((void *)(unsigned long)s->ptr);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "in %s\n", (char *)(unsigned long)s->ptr);
			break;
		}

		case NVRM_IOCTL_UNK5E:
		{
			// Copy data from mem to GPU
#if 0
			struct nvrm_ioctl_unk5e *s = (void *)data;
			SysRes ff;
			ff = VG_(open) ("dump5e", O_CREAT | O_WRONLY | O_TRUNC, 0777);
			if (!ff.isError)
			{
				VG_(write) (ff.res, (void *) s->ptr, 0x01000000);
				VG_(close) (ff.res);
			}
#endif
			break;
		}
		case NVRM_IOCTL_DESTROY:
		{
			struct nvrm_ioctl_destroy *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('d');
				mmt_bin_write_4(s->parent);
				mmt_bin_write_4(s->handle);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "destroy object 0x%08x:0x%08x\n", s->parent, s->handle);
			break;
		}
		case NVRM_IOCTL_CREATE:
		{
			struct nvrm_ioctl_create *s = (void *)data;

			struct object_type *objtype;
			const char *name = "???";
			objtype = find_objtype(s->cls);
			if (objtype && objtype->name)
				name = objtype->name;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('c');
				mmt_bin_write_4(s->parent);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_4(s->cls);
				mmt_bin_write_str(name);
				mmt_bin_end();
			}
			else
			{
				VG_(message) (Vg_DebugMsg,
						"create gpu object 0x%08x:0x%08x type 0x%04x (%s)\n",
						s->parent, s->handle, s->cls, name);
			}

			if (s->ptr)
			{
				if (objtype)
					dumpmem("in ", s->ptr, objtype->cargs * 4);
				else
					dumpmem("in ", s->ptr, 0x40);
			}

			break;
		}
		case NVRM_IOCTL_CREATE_DRV_OBJ:
		{
			struct nvrm_ioctl_create_drv_obj *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('r');
				mmt_bin_write_4(s->parent);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_8(s->cls);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg,
						"create driver object 0x%08x:0x%08x type 0x%04x\n", s->parent, s->handle, s->cls);
			break;
		}
	}
}

void mmt_nv_ioctl_post(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;
	int i;
	struct mmt_mmap_data *region;

	if (!FD_ISSET(fd, &nvidiactl_fds) && !FD_ISSET(fd, &nvidia0_fds))
		return;

	if (mmt_trace_marks)
	{
		char buf[50];
		VG_(snprintf)(buf, 50, "VG-%d-%d-POST\n", VG_(getpid)(), trace_mark_cnt++);
		VG_(write)(trace_mark_fd, buf, VG_(strlen)(buf));
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('k');
			mmt_bin_write_str(buf);
			mmt_bin_end();
		}
		else
			VG_(message)(Vg_DebugMsg, "MARK: %s", buf);
	}

	if ((id & 0x0000FF00) == 0x4600)
	{
		size = (id & 0x3FFF0000) >> 16;

		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('j');
			mmt_bin_write_4(fd);
			mmt_bin_write_4(id);
			mmt_bin_write_buffer((UChar *)data, size);
			mmt_bin_end();
		}
		else
		{
			char line[4096];
			int idx = 0;

			VG_(sprintf) (line, "post_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
			idx = VG_(strlen(line));

			for (i = 0; i < size / 4; ++i)
			{
				if (idx + 11 >= 4095)
					break;
				VG_(sprintf) (line + idx, "0x%08x ", data[i]);
				idx += 11;
			}
			VG_(message) (Vg_DebugMsg, "%s\n", line);
		}
	}
	else
		VG_(message)(Vg_UserMsg, "post_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		case NVRM_IOCTL_CREATE_CTX: // Initialize
		{
			struct nvrm_ioctl_create_ctx *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('x');
				mmt_bin_write_4(s->handle);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "created context object 0x%08x\n", s->handle);

			break;
		}
		case NVRM_IOCTL_CREATE_DEV_OBJ:
		{
			struct nvrm_ioctl_create_dev_obj *s = (void *)data;
			dumpmem("out", s->ptr, 0x3C);
			break;
		}
		case NVRM_IOCTL_HOST_MAP: // Allocate map for existing object
		{
			struct nvrm_ioctl_host_map *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('a');
				mmt_bin_write_4(s->subdev);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_8(s->foffset);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "allocate map 0x%08x:0x%08x 0x%08llx\n", s->subdev, s->handle, (Off64T)s->foffset);

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

			if (release_nvidia_mapping(s->foffset))
			{
				if (mmt_binary_output)
				{
					mmt_bin_write_1('n');
					mmt_bin_write_1('e');
					mmt_bin_write_4(s->subdev);
					mmt_bin_write_4(s->handle);
					mmt_bin_write_8(s->foffset);
					mmt_bin_end();
				}
				else
					VG_(message) (Vg_DebugMsg, "deallocate map 0x%08x:0x%08x 0x%08llx\n",
							s->subdev, s->handle, (Off64T)s->foffset);
			}

			break;
		}
		case NVRM_IOCTL_CREATE_VSPACE: // Allocate map (also create object)
		{
			struct nvrm_ioctl_create_vspace *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('p');
				mmt_bin_write_4(s->parent);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_4(s->cls);
				mmt_bin_write_8(s->foffset);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg,
						"create mapped object 0x%08x:0x%08x type=0x%08x 0x%08llx\n",
						s->parent, s->handle, s->cls, (Off64T)s->foffset);
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

			Addr addr1 = release_nvidia_mapping2(s->parent, s->handle);
			if ((void *)addr1 != NULL)
			{
				if (mmt_binary_output)
				{
					mmt_bin_write_1('n');
					mmt_bin_write_1('e');
					mmt_bin_write_4(s->parent);
					mmt_bin_write_4(s->handle);
					mmt_bin_write_8(addr1);
					mmt_bin_end();
				}
				else
					VG_(message) (Vg_DebugMsg, "deallocate map 0x%08x:0x%08x %p\n",
							s->parent, s->handle, (void *)addr1);
			}
			break;
		}
		case NVRM_IOCTL_CALL:
		{
			struct nvrm_ioctl_call *s = (void *)data;

			dumpmem("out ", s->ptr, s->size);

			if (data[2] == 0x10000002)
			{
				UInt *addr2 = (UInt *) (unsigned long)s->ptr;
				dumpmem("out2 ", addr2[2], 0x3c);
			}
			else if (data[2] == NVRM_MTHD_SUBDEVICE_BAR0)
			{
				struct nvrm_mthd_subdevice_bar0 *m = (void *)(unsigned long)s->ptr;
				UInt *tx = (UInt *)(unsigned long)m->ptr;

				if (mmt_binary_output)
				{
					mmt_bin_write_1('n');
					mmt_bin_write_1('1');
					mmt_bin_write_4(m->cnt);
					mmt_bin_write_8(m->ptr);
					mmt_bin_write_buffer((UChar *)tx, m->cnt * 8 * 4);
					mmt_bin_end();
				}
				else
				{
					UInt k;
					for (k = 0; k < m->cnt; ++k)
						VG_(message) (Vg_DebugMsg, "RETURND: DIR=%x MMIO=%x VALUE=%08x MASK=%08x UNK=%08x,%08x,%08x,%08x\n",
									tx[k * 8 + 0],
									tx[k * 8 + 3],
									tx[k * 8 + 5],
									tx[k * 8 + 7],
									tx[k * 8 + 1],
									tx[k * 8 + 2],
									tx[k * 8 + 4],
									tx[k * 8 + 6]);
				}
			}

			break;
		}
		case NVRM_IOCTL_UNK38:
		{
			struct nvrm_ioctl_unk38 *s = (void *)data;
			dumpmem("out", s->ptr, s->unk18);
			break;
		}
		case NVRM_IOCTL_QUERY:
		{
			struct nvrm_ioctl_query *s = (void *)data;

			UInt *addr2 = (UInt *)(unsigned long)s->ptr;
			dumpmem("out", s->ptr, s->status); // s/status/size/ ?
			if (s->query == 0x14c && addr2[2])
				// List supported object types
				dumpmem("out2 ", addr2[2], addr2[0] * 4);

			break;
		}
		case NVRM_IOCTL_VSPACE_MAP: // map GPU address
		{
			struct nvrm_ioctl_vspace_map *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('G');
				mmt_bin_write_4(s->dev);
				mmt_bin_write_4(s->vspace);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_8(s->addr);
				mmt_bin_write_4(s->size);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg,
						"gpu map 0x%08x:0x%08x:0x%08x, addr 0x%08llx, len 0x%08llx\n",
						s->dev, s->vspace, s->handle, (Off64T)s->addr, (Off64T)s->size);
			break;
		}
		case NVRM_IOCTL_VSPACE_UNMAP: // unmap GPU address
		{
			struct nvrm_ioctl_vspace_unmap *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('H');
				mmt_bin_write_4(s->dev);
				mmt_bin_write_4(s->vspace);
				mmt_bin_write_4(s->handle);
				mmt_bin_write_8(s->addr);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg,
						"gpu unmap 0x%08x:0x%08x:0x%08x addr 0x%08llx\n", s->dev,
						s->vspace, s->handle, (Off64T)s->addr);
			break;
		}
		case NVRM_IOCTL_CREATE_DMA:		// create DMA object [3] is some kind of flags, [6] is an offset?
		{
			struct nvrm_ioctl_create_dma *s = (void *)data;

			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('t');
				mmt_bin_write_4(s->handle);
				mmt_bin_write_4(s->cls);
				mmt_bin_write_4(s->parent);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg,
						"create dma object 0x%08x, type 0x%08x, parent 0x%08x\n",
						s->handle, s->cls, s->parent);
			break;
		}
		case NVRM_IOCTL_BIND: // bind
		{
			struct nvrm_ioctl_bind *s = (void *)data;
			if (mmt_binary_output)
			{
				mmt_bin_write_1('n');
				mmt_bin_write_1('b');
				mmt_bin_write_4(s->target);
				mmt_bin_write_4(s->handle);
				mmt_bin_end();
			}
			else
				VG_(message) (Vg_DebugMsg, "bind 0x%08x 0x%08x\n", s->target, s->handle);
			break;
		}
	}
}

void mmt_nv_ioctl_pre_clo_init(void)
{
	FD_ZERO(&nvidiactl_fds);
	FD_ZERO(&nvidia0_fds);
}
