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
#include "coregrind/pub_core_aspacemgr.h"
#include "nvrm_ioctl.h"
#include "nvrm_mthd.h"
#include "nvrm_query.h"

#include <sys/select.h>
#include <sys/mman.h>

#define NVRM_CLASS_SUBDEVICE_0 0x2080

int mmt_trace_nvidia_ioctls = False;
int mmt_trace_marks = False;
static int trace_mark_fd;
static int trace_mark_cnt = 0;

int mmt_ioctl_create_fuzzer = 0;
int mmt_ioctl_call_fuzzer = 0;

static void *test_page = NULL;
static void *inaccessible_page = NULL;

#define MMT_ALLOC_SIZE 4096

#ifdef MMT_64BIT
#define MMT_INITIAL_OFFSET 0x100000000
#else
#define MMT_INITIAL_OFFSET 0x10000000
#endif

struct nv_object_type mmt_nv_object_types[] =
{
	{0x0000, 1},

	{0x0005, 6},

	{0x0019, 4},

	{0x0039, 4},

	{0x0041, 1},

	{0x0043, 4},

	{0x0044, 4},

	{0x004a, 4},

	{0x3062, 4},

	{0x3066, 4},

	{0x406e, 8},

	{0x506f, 8},
	{0x826f, 8},
	{0x906f, 8},
	{0xa06f, 8},

	{0x8270, 1},
	{0x8570, 1},
	{0x9470, 1},

	{0x0072, 4},
	{0x5072, 3},
	{0x9072, 3},

	{0x0073, 0},

	{0x0079, 6},
	{0x5079, 2},

	{0x827a, 4},
	{0x857a, 4},

	{0x307b, 4},

	{0x357c, 3},
	{0x827c, 8},
	{0x857c, 8},

	{0x827d, 8},
	{0x857d, 8},

	{0x0080, 8}, // 10 on maxwell/340.32, TODO
	{0x2080, 1},

	{0x3089, 4},

	{0x308a, 4},

	{0x9096, 0},

	{0x4097, 4},
	{0x8297, 4},
	{0x8597, 4},
	{0x9197, 4},

	{0x309e, 4},

	{0x009f, 4},

	{0x85b5, 2},
	{0x90b5, 2},
	{0xb0b5, 2},

	{0x83de, 3},

	{0x00f1, 6},

	{-1, 0},
	{-1, 0},
	{-1, 0},
	{-1, 0},
	{-1, 0},
	{-1, 0},
	{-1, 0},
	{-1, 0},
};
int mmt_nv_object_types_count = sizeof(mmt_nv_object_types) / sizeof(mmt_nv_object_types[0]);

/*
 * Binary format message subtypes: (some of them are not used anymore, so they are reserved)
 *     a = reserved (allocate map)
 *     b = reserved (bind)
 *     c = reserved (create object)
 *     d = reserved (destroy object)
 *     e = reserved (deallocate map)
 *     g = reserved (gpu map)
 *     h = reserved (gpu unmap)
 *     i = reserved (ioctl before)
 *     j = reserved (ioctl after)
 *     k = mark (mmiotrace)
 *     l = reserved (call method)
 *     m = reserved (old mmap)
 *     M = reserved (mmap)
 *     o = memory dump
 *     p = reserved (create mapped object)
 *     P = reserved (nouveau's GEM_PUSHBUF data)
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

static const struct nv_object_type *find_objtype(UInt id)
{
	int i;

	for (i = 0; i < mmt_nv_object_types_count; ++i)
		if (mmt_nv_object_types[i].id == id)
			return &mmt_nv_object_types[i];

	return NULL;
}

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

static void mthd_dumpmem(const char *str, uint64_t *ptr, uint32_t len, int in)
{
	if (mmt_ioctl_call_fuzzer == 1 && !in && *ptr && test_page_available())
	{
		// restore arguments pointer before dumping ioctl data
		uint64_t orig_addr = ((uint64_t *)test_page)[0];
		VG_(memcpy)(u64_to_ptr(orig_addr), u64_to_ptr(*ptr), len);
		*ptr = orig_addr;
	}

	dumpmem(str, *ptr, len);

	if (mmt_ioctl_call_fuzzer == 1 && in && *ptr && test_page_available())
	{
		((uint64_t *)test_page)[0] = *ptr;
		uint64_t tmp_addr = ptr_to_u64(inaccessible_page) - len;
		VG_(memcpy)(u64_to_ptr(tmp_addr), u64_to_ptr(*ptr), len);
		*ptr = tmp_addr;
	}
}

static void handle_nvrm_ioctl_call(struct nvrm_ioctl_call *s, int in)
{
	void *ptr = u64_to_ptr(s->ptr);
	const char *str;
	if (in)
		str = "in";
	else
		str = "out";

	if (in)
		dumpmem(str, s->ptr, s->size);

	if (s->mthd == 0x10000002)
	{
		UInt *addr2 = ptr;
		//FIXME: this is probably going to crash on 64-bit systems
		//TODO:  convert to mthd_dumpmem once above will be fixed
		dumpmem(str, addr2[2], 0x3c);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_BAR0)
	{
		struct nvrm_mthd_subdevice_bar0 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4 * 8, in);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_GET_CLASSES)
	{
		struct nvrm_mthd_device_get_classes *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_FB_GET_PARAMS)
	{
		struct nvrm_mthd_subdevice_fb_get_params *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4 * 2, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_BUS_GET_PARAMS)
	{
		struct nvrm_mthd_subdevice_bus_get_params *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4 * 2, in);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1401)
	{
		struct nvrm_mthd_device_unk1401 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_GET_FIFO_ENGINES)
	{
		struct nvrm_mthd_subdevice_get_fifo_engines *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4, in);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1102)
	{
		struct nvrm_mthd_device_unk1102 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt, in);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK1701)
	{
		struct nvrm_mthd_device_unk1701 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK1201)
	{
		struct nvrm_mthd_subdevice_unk1201 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4 * 2, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0101)
	{
		struct nvrm_mthd_subdevice_unk0101 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4 * 2, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0522)
	{
		struct nvrm_mthd_subdevice_unk0522 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->size, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_UNK0512)
	{
		struct nvrm_mthd_subdevice_unk0512 *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->size, in);
	}
	else if (s->mthd == NVRM_MTHD_SUBDEVICE_GET_FIFO_CLASSES)
	{
		struct nvrm_mthd_subdevice_get_fifo_classes *m = ptr;
		mthd_dumpmem(str, &m->ptr, m->cnt * 4, in);
	}
	else if (s->mthd == NVRM_MTHD_DEVICE_UNK170D)
	{
		struct nvrm_mthd_device_unk170d *m = ptr;
		mthd_dumpmem(str, &m->ptr1, m->cnt * 4, in);
		mthd_dumpmem(str, &m->ptr2, m->cnt * 4, in);
	}

	if (!in)
		dumpmem(str, s->ptr, s->size);
}

static void handle_nvrm_ioctl_query(struct nvrm_ioctl_query *s, int in)
{
	void *ptr = u64_to_ptr(s->ptr);
	const char *str;
	if (in)
		str = "in";
	else
		str = "out";

	dumpmem(str, s->ptr, s->size);

	if (s->query == NVRM_QUERY_OBJECT_CLASSES)
	{
		struct nvrm_query_object_classes *q = ptr;
		dumpmem(str, q->ptr, q->cnt * 4);
	}
}

static void inject_ioctl_call(int fd, uint32_t cid, uint32_t handle, uint32_t mthd, void *ptr, int size)
{
	struct nvrm_ioctl_call call;
	VG_(memset)(&call, 0, sizeof(call));
	call.cid = cid;
	call.handle = handle;
	call.mthd = mthd;
	call.ptr = ptr_to_u64(ptr);
	call.size = size;
	UWord ioctlargs[3] = { fd, (UWord)NVRM_IOCTL_CALL, (UWord)&call };

	mmt_nv_ioctl_pre(ioctlargs);
	SysRes ioctlres = VG_(do_syscall3)(__NR_ioctl, fd, (UWord)NVRM_IOCTL_CALL, (UWord)&call);
	mmt_nv_ioctl_post(ioctlargs, ioctlres);
}

static int in_fuzzer_mode = 0;
static void mess_with_ioctl_create(int fd, UInt *data)
{
	struct nvrm_ioctl_create *s = (void *)data;

	if (!s->ptr || in_fuzzer_mode || !test_page_available())
		return;

	if (mmt_ioctl_create_fuzzer == 1 && find_objtype(s->cls) != NULL)
		return;

	// Kernel does not handle IOCTL_DESTROY for this class, so when userspace
	// tries to create this object the 2nd time, kernel returns OBJECT_ERROR.
	// And it pushes the driver into a state with abysmal performance (<2 FPS
	// for glxgears)...
	// Comment out if you really want to trace that.
	if (s->cls == 0x0079)
		return;

	struct nvrm_ioctl_create create;
	int test_size = 0;
	SysRes res;
	int err;

	mmt_bin_flush();
	VG_(message)(Vg_UserMsg,
			"trying to detect minimal argument size for class 0x%04x\n", s->cls);
	uint32_t cid = s->cid;
	uint32_t parent = s->parent;
	uint32_t handle = s->handle;
	do
	{
		VG_(memcpy)(&create, s, sizeof(create));

		test_size++;
		create.ptr = ptr_to_u64(inaccessible_page) - test_size;
		VG_(memcpy)(u64_to_ptr(create.ptr), u64_to_ptr(s->ptr), test_size);

		UWord args[3] = { fd, (UWord)NVRM_IOCTL_CREATE, (UWord)&create };

		in_fuzzer_mode = 1;
		mmt_nv_ioctl_pre(args);
		res = VG_(do_syscall3)(__NR_ioctl, args[0], args[1], args[2]);
		mmt_nv_ioctl_post(args, res);
		in_fuzzer_mode = 0;
		err = (sr_isError(res) || create.status != NVRM_STATUS_SUCCESS) ? 1 : 0;
	}
	while (err && test_size < 4000);

	if (!err)
	{
		dumpmem("out", create.ptr, test_size);
		mmt_bin_flush();
		VG_(message)(Vg_UserMsg,
				"minimal argument size for class 0x%04x: %d bytes (%d words)\n", s->cls, test_size, test_size / 4);

		if (s->cls == 0)
			cid = parent = handle = ((uint32_t *)u64_to_ptr(create.ptr))[0];
		struct nvrm_ioctl_destroy destroy;
		destroy.cid = cid;
		destroy.parent = parent;
		destroy.handle = handle;
		destroy.status = 0;

		UWord args[3] = { fd, (UWord)NVRM_IOCTL_DESTROY, (UWord)&destroy };

		in_fuzzer_mode = 1;
		mmt_nv_ioctl_pre(args);
		res = VG_(do_syscall3)(__NR_ioctl, args[0], args[1], args[2]);
		mmt_nv_ioctl_post(args, res);
		in_fuzzer_mode = 0;

	}
	else
	{
		mmt_bin_flush();
		VG_(message)(Vg_UserMsg,
				"could not detect minimal argument size for class 0x%04x\n", s->cls);
	}
}

int mmt_nv_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;

	if (!mmt_trace_nvidia_ioctls)
		return 0;
	if (!data)
		return 0;
	if ((id & 0x0000FF00) != 0x4600)
		return 0;

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

	if (id == NVRM_IOCTL_CREATE && mmt_ioctl_create_fuzzer)
		mess_with_ioctl_create(fd, data);

	size = (id & 0x3FFF0000) >> 16;

	mmt_bin_write_1('i');
	mmt_bin_write_4(fd);
	mmt_bin_write_4(id);
	mmt_bin_write_buffer((UChar *)data, size);
	mmt_bin_end();

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
			handle_nvrm_ioctl_query((void *)data, 1);
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
			dumpmem("in", s->ptr1, 4 * s->cnt);
			dumpmem("in", s->ptr2, 4 * s->cnt);
			dumpmem("in", s->ptr3, 4 * s->cnt);
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
			mmt_bin_write_str(u64_to_ptr(s->ptr));
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
			const struct nv_object_type *objtype = find_objtype(s->cls);

			if (s->ptr && objtype && !in_fuzzer_mode)
			{
				dumpmem("in ", s->ptr, objtype->cargs * 4);
				if (mmt_ioctl_create_fuzzer == 1 && test_page_available())
				{
					((uint64_t *)test_page)[0] = s->ptr;
					uint64_t tmp_addr = ptr_to_u64(inaccessible_page) - objtype->cargs * 4;
					VG_(memcpy)(u64_to_ptr(tmp_addr), u64_to_ptr(s->ptr), objtype->cargs * 4);
					s->ptr = tmp_addr;
				}
			}

			break;
		}
	}
	mmt_bin_sync();

	return 1;
}

int mmt_nv_ioctl_post(UWord *args, SysRes res)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;

	if (!mmt_trace_nvidia_ioctls)
		return 0;
	if (!data)
		return 0;
	if ((id & 0x0000FF00) != 0x4600)
		return 0;

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

	if (id == NVRM_IOCTL_CREATE && mmt_ioctl_create_fuzzer == 1)
	{
		struct nvrm_ioctl_create *s = (void *)data;
		const struct nv_object_type *objtype = find_objtype(s->cls);

		// restore arguments pointer before dumping ioctl data
		if (s->ptr && objtype && !in_fuzzer_mode && test_page_available())
		{
			uint64_t orig_addr = ((uint64_t *)test_page)[0];
			VG_(memcpy)(u64_to_ptr(orig_addr), u64_to_ptr(s->ptr), objtype->cargs * 4);
			s->ptr = orig_addr;
		}
	}

	size = (id & 0x3FFF0000) >> 16;

	mmt_bin_write_1('j');
	mmt_bin_write_4(fd);
	mmt_bin_write_4(id);
	mmt_bin_write_8(sr_Res(res));
	mmt_bin_write_8(sr_Err(res));
	mmt_bin_write_buffer((UChar *)data, size);
	mmt_bin_end();

	switch (id)
	{
		case NVRM_IOCTL_CREATE_DEV_OBJ:
		{
			struct nvrm_ioctl_create_dev_obj *s = (void *)data;
			dumpmem("out", s->ptr, 0x3C);
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
			dumpmem("out", s->ptr1, 4 * s->cnt);
			dumpmem("out", s->ptr2, 4 * s->cnt);
			dumpmem("out", s->ptr3, 4 * s->cnt);
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
			handle_nvrm_ioctl_query((void *)data, 0);
			break;
		}
		case NVRM_IOCTL_CREATE:
		{
			struct nvrm_ioctl_create *s = (void *)data;
			const struct nv_object_type *objtype = find_objtype(s->cls);

			if (s->ptr && objtype && !in_fuzzer_mode)
				dumpmem("out", s->ptr, objtype->cargs * 4);

			if (s->cls == NVRM_CLASS_SUBDEVICE_0 && !in_fuzzer_mode)
			{
				// inject GET_CHIPSET ioctl
				struct nvrm_mthd_subdevice_get_chipset chip;
				VG_(memset)(&chip, 0, sizeof(chip));

				inject_ioctl_call(fd, s->cid, s->handle, NVRM_MTHD_SUBDEVICE_GET_CHIPSET,
					&chip, sizeof(chip));
			}

			break;
		}
	}
	mmt_bin_sync();

	return 1;
}
