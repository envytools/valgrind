/*--------------------------------------------------------------------*/
/*--- nvtrace: mmaptracer tool that tracks NVidia ioctls           ---*/
/*--------------------------------------------------------------------*/

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

/*
	Vg_UserMsg for important messages
	Vg_DebugMsg for memory load/store messages
	Vg_DebugExtraMsg for other messages 
*/

#include "pub_tool_basics.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcassert.h"

#include "pub_tool_tooliface.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_options.h"
#include "pub_tool_machine.h"
#include "pub_tool_threadstate.h"
#include "pub_tool_vki.h"

#include "pub_tool_vkiscnums.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_mallocfree.h"

#include "coregrind/pub_core_basics.h"
#include "coregrind/pub_core_libcassert.h"
#include "coregrind/m_syswrap/priv_types_n_macros.h"

#include <fcntl.h>
#include <string.h>

#define MAX_REGIONS 100
#define MAX_TRACE_FILES 10

#ifdef __LP64__
#define MMT_64BIT
#endif

static struct mmt_mmap_data {
	Addr start;
	Addr end;
	int fd;
	Off64T offset;
	UInt id;
	UWord data1;
	UWord data2;
} mmt_mmaps[MAX_REGIONS];
static int last_region = -1;

static struct object_type {
	UInt id;		// type id
	char *name;		// some name
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

static UInt current_item = 1;

/* Command line options */
//UInt mmt_clo_offset = (UInt) -1;
int dump_load = True, dump_store = True;
static int trace_opens = False;

static struct trace_file {
	const char *path;
	fd_set fds;
} trace_files[MAX_TRACE_FILES];
static int trace_all_files = False;

static int trace_nvidia_ioctls = False;
static fd_set nvidiactl_fds;
static fd_set nvidia0_fds;

static struct mmt_mmap_data *find_mmap(Addr addr)
{
	struct mmt_mmap_data *region = NULL;
	int i;

	for (i = 0; i <= last_region; i++)
	{
		region = &mmt_mmaps[i];
		if (addr >= region->start && addr < region->end)
			return region;
	}

	return NULL;
}

static struct object_type *find_objtype(UInt id)
{
	int i;
	int n = sizeof(object_types) / sizeof(struct object_type);

	for (i = 0; i < n; ++i)
		if (object_types[i].id == id)
			return &object_types[i];

	return NULL;
}

static void mydescribe(Addr inst_addr, char *namestr, int len)
{
#if 0
	const SegInfo *si;
	/* Search for it in segments */
	VG_(snprintf) (namestr, len, "@%08x", inst_addr);
	for (si = VG_(next_seginfo) (NULL);
		 si != NULL; si = VG_(next_seginfo) (si))
	{
		Addr base = VG_(seginfo_start) (si);
		SizeT size = VG_(seginfo_size) (si);

		if (inst_addr >= base && inst_addr < base + size)
		{
			const UChar *filename = VG_(seginfo_filename) (si);
			VG_(snprintf) (namestr, len, "@%08x (%s:%08x)", inst_addr,
					filename, inst_addr - base);

			break;
		}
	}
#else
	VG_(strcpy) (namestr, "");
#endif

}

static VG_REGPARM(2)
void trace_store(Addr addr, SizeT size, Addr inst_addr, UWord value)
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

static VG_REGPARM(2)
void trace_store2(Addr addr, SizeT size, Addr inst_addr, UWord value1, UWord value2)
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
static VG_REGPARM(2)
void trace_store4(Addr addr, Addr inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
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

static VG_REGPARM(2)
void trace_load(Addr addr, SizeT size, UInt inst_addr, UWord value)
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

static VG_REGPARM(2)
void trace_load2(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2)
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
static VG_REGPARM(2)
void trace_load4(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2, UWord value3, UWord value4)
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

static void add_trace_load1(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr, IRExpr *val1)
{
	IRExpr **argv = mkIRExprVec_4(addr, mkIRExpr_HWord(size),
					  mkIRExpr_HWord(inst_addr), val1);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_load",
					VG_(fnptr_to_fnentry) (trace_load),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}

static void add_trace_load2(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr, IRExpr *val1, IRExpr *val2)
{
	IRExpr **argv = mkIRExprVec_5(addr, mkIRExpr_HWord(size),
					  mkIRExpr_HWord(inst_addr), val1, val2);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_load2",
					VG_(fnptr_to_fnentry) (trace_load2),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}

#ifndef MMT_64BIT
static void add_trace_load4(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr, IRExpr *val1, IRExpr *val2, IRExpr *val3, IRExpr *val4)
{
	IRExpr **argv = mkIRExprVec_7(addr, mkIRExpr_HWord(size),
					  mkIRExpr_HWord(inst_addr), val1, val2, val3, val4);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_load4",
					VG_(fnptr_to_fnentry) (trace_load4),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}
#endif

#ifdef MMT_64BIT
static void add_trace_load(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr, IRExpr *data, IRType arg_ty)
{
	IRTemp t;
	IRStmt *cast;
	IRExpr *data1, *data2;

	switch (arg_ty)
	{
		case Ity_I8:
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_8Uto64, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_I16:
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_16Uto64, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);
			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_F32:
			// reinterpret as I32
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF32asI32, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			// no break;
		case Ity_I32:
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_32Uto64, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_F64:
			// reinterpret as I64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF64asI64, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);
			// no break;

		case Ity_I64:
			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_V128:
			// upper 64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128HIto64, data));
			addStmtToIRSB(bb, cast);
			data1 = IRExpr_RdTmp(t);

			// lower 64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128to64, data));
			addStmtToIRSB(bb, cast);
			data2 = IRExpr_RdTmp(t);

			add_trace_load2(bb, addr, sizeofIRType(Ity_I64), inst_addr, data1, data2);
			break;
		default:
			VG_(message) (Vg_UserMsg, "Warning! we missed a read of 0x%08x\n", (UInt) arg_ty);
			break;
	}
}
#else
static void add_trace_load(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr, IRExpr *data, IRType arg_ty)
{
	IRTemp t;
	IRStmt *cast;
	IRExpr *data0;
	IRExpr *data1, *data2;
	IRExpr *data3, *data4;

	switch (arg_ty)
	{
		case Ity_I8:
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_8Uto32, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_I16:
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_16Uto32, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_F32:
			// reinterpret as I32
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF32asI32, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);

			// no break;
		case Ity_I32:
			add_trace_load1(bb, addr, size, inst_addr, data);
			break;

		case Ity_F64:
			// reinterpret as I64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF64asI64, data));

			addStmtToIRSB(bb, cast);
			data = IRExpr_RdTmp(t);
			// no break;
		case Ity_I64:
			// we cannot pass whole 64-bit value in one parameter, so we split it

			// upper 32
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data));
			addStmtToIRSB(bb, cast);
			data1 = IRExpr_RdTmp(t);

			// lower 32
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data));
			addStmtToIRSB(bb, cast);
			data2 = IRExpr_RdTmp(t);

			add_trace_load2(bb, addr, sizeofIRType(Ity_I32), inst_addr, data1, data2);
			break;
		case Ity_V128:
			// upper 64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128HIto64, data));
			addStmtToIRSB(bb, cast);
			data0 = IRExpr_RdTmp(t);

			// upper 32 of upper 64
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data0));
			addStmtToIRSB(bb, cast);
			data1 = IRExpr_RdTmp(t);

			// lower 32 of upper 64
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data0));
			addStmtToIRSB(bb, cast);
			data2 = IRExpr_RdTmp(t);

			// lower 64
			t = newIRTemp(bb->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128to64, data));
			addStmtToIRSB(bb, cast);
			data0 = IRExpr_RdTmp(t);

			// upper 32 of lower 64
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data0));
			addStmtToIRSB(bb, cast);
			data3 = IRExpr_RdTmp(t);

			// lower 32 of lower 64
			t = newIRTemp(bb->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data0));
			addStmtToIRSB(bb, cast);
			data4 = IRExpr_RdTmp(t);

			add_trace_load4(bb, addr, sizeofIRType(Ity_I32), inst_addr, data1, data2, data3, data4);
			break;
		default:
			VG_(message) (Vg_UserMsg, "Warning! we missed a read of 0x%08x\n", (UInt) arg_ty);
			break;
	}
}
#endif

static void
add_trace_store1(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr,
		IRExpr *data)
{
	IRExpr **argv = mkIRExprVec_4(addr, mkIRExpr_HWord(size),
					mkIRExpr_HWord(inst_addr), data);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_store",
					VG_(fnptr_to_fnentry) (trace_store),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}

static void
add_trace_store2(IRSB *bb, IRExpr *addr, Int size, Addr inst_addr,
		IRExpr *data1, IRExpr *data2)
{
	IRExpr **argv = mkIRExprVec_5(addr, mkIRExpr_HWord(size),
					mkIRExpr_HWord(inst_addr),
					data1, data2);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_store2",
					VG_(fnptr_to_fnentry) (trace_store2),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}

#ifndef MMT_64BIT
static void
add_trace_store4(IRSB *bb, IRExpr *addr, Addr inst_addr,
		IRExpr *data1, IRExpr *data2, IRExpr *data3, IRExpr *data4)
{
	IRExpr **argv = mkIRExprVec_6(addr, mkIRExpr_HWord(inst_addr),
					data1, data2, data3, data4);
	IRDirty *di = unsafeIRDirty_0_N(2,
					"trace_store4",
					VG_(fnptr_to_fnentry) (trace_store4),
					argv);
	addStmtToIRSB(bb, IRStmt_Dirty(di));
}
#endif

#ifdef MMT_64BIT
static void add_trace_store(IRSB *bbOut, IRExpr *destAddr, Addr inst_addr,
				IRType arg_ty, IRExpr *data_expr)
{
	IRTemp t = IRTemp_INVALID;
	IRStmt *cast = NULL;
	IRExpr *data_expr1, *data_expr2;
	
	Int size = sizeofIRType(arg_ty);

	switch (arg_ty)
	{
		case Ity_I8:
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_8Uto64, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_I16:
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_16Uto64, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_F32:
			// reinterpret as I32
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF32asI32, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			// no break;
		case Ity_I32:
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_32Uto64, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_F64:
			// reinterpret as I64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF64asI64, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);
			// no break;
		case Ity_I64:
			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_V128:
			// we cannot pass whole 128-bit value in one parameter, so we split it
			
			// upper 64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128HIto64, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr1 = IRExpr_RdTmp(t);

			// lower 64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128to64, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr2 = IRExpr_RdTmp(t);

			add_trace_store2(bbOut, destAddr, sizeofIRType(Ity_I64), inst_addr,
					data_expr1, data_expr2);

			break;
		default:
			VG_(message) (Vg_UserMsg, "Warning! we missed a write of 0x%08x\n", (UInt) arg_ty);
			break;
	}
}
#else
static void add_trace_store(IRSB *bbOut, IRExpr *destAddr, Addr inst_addr,
				IRType arg_ty, IRExpr *data_expr)
{
	IRTemp t = IRTemp_INVALID;
	IRStmt *cast = NULL;
	IRExpr *data_expr0;
	IRExpr *data_expr1, *data_expr2;
	IRExpr *data_expr3, *data_expr4;
	
	Int size = sizeofIRType(arg_ty);

	switch (arg_ty)
	{
		case Ity_I8:
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_8Uto32, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_I16:
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_16Uto32, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_F32:
			// reinterpret as I32
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF32asI32, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);

			// no break;
		case Ity_I32:
			add_trace_store1(bbOut, destAddr, size, inst_addr, data_expr);
			break;
		case Ity_F64:
			// reinterpret as I64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_ReinterpF64asI64, data_expr));

			addStmtToIRSB(bbOut, cast);
			data_expr = IRExpr_RdTmp(t);
			// no break;
		case Ity_I64:
			// we cannot pass whole 64-bit value in one parameter, so we split it
			
			// upper 32
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr1 = IRExpr_RdTmp(t);

			// lower 32
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr2 = IRExpr_RdTmp(t);

			add_trace_store2(bbOut, destAddr, sizeofIRType(Ity_I32), inst_addr, data_expr1, data_expr2);
			break;
		case Ity_V128:
			// upper 64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128HIto64, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr0 = IRExpr_RdTmp(t);

			// upper 32 of upper 64
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data_expr0));
			addStmtToIRSB(bbOut, cast);
			data_expr1 = IRExpr_RdTmp(t);

			// lower 32 of upper 64
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data_expr0));
			addStmtToIRSB(bbOut, cast);
			data_expr2 = IRExpr_RdTmp(t);

			// lower 64
			t = newIRTemp(bbOut->tyenv, Ity_I64);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_V128to64, data_expr));
			addStmtToIRSB(bbOut, cast);
			data_expr0 = IRExpr_RdTmp(t);

			// upper 32 of lower 64
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64HIto32, data_expr0));
			addStmtToIRSB(bbOut, cast);
			data_expr3 = IRExpr_RdTmp(t);

			// lower 32 of lower 64
			t = newIRTemp(bbOut->tyenv, Ity_I32);
			cast = IRStmt_WrTmp(t, IRExpr_Unop(Iop_64to32, data_expr0));
			addStmtToIRSB(bbOut, cast);
			data_expr4 = IRExpr_RdTmp(t);

			add_trace_store4(bbOut, destAddr, inst_addr,
					data_expr1, data_expr2, data_expr3, data_expr4);

			break;
		default:
			VG_(message) (Vg_UserMsg, "Warning! we missed a write of 0x%08x\n", (UInt) arg_ty);
			break;
	}
}
#endif

static IRSB *mmt_instrument(VgCallbackClosure *closure,
				IRSB *bbIn,
				VexGuestLayout *layout,
				VexGuestExtents *vge,
				IRType gWordTy, IRType hWordTy)
{
	IRSB *bbOut;
	int i = 0;
	Addr inst_addr;

	if (gWordTy != hWordTy)
	{
		/* We don't currently support this case. */
		VG_(tool_panic) ("host/guest word size mismatch");
	}

	/* Set up BB */
	bbOut = deepCopyIRSBExceptStmts(bbIn);

	/* Copy verbatim any IR preamble preceding the first IMark */
	while (i < bbIn->stmts_used && bbIn->stmts[i]->tag != Ist_IMark)
	{
		addStmtToIRSB(bbOut, bbIn->stmts[i]);
		i++;
	}

	inst_addr = 0;

	for (; i < bbIn->stmts_used; i++)
	{
		IRStmt *st = bbIn->stmts[i];
		IRExpr *data_expr;
		IRType arg_ty;

		if (!st)
			continue;

		if (st->tag == Ist_IMark)
		{
			inst_addr = st->Ist.IMark.addr;
			addStmtToIRSB(bbOut, st);
		}
		else if (st->tag == Ist_Store && dump_store)
		{
			data_expr = st->Ist.Store.data;

			arg_ty = typeOfIRExpr(bbIn->tyenv, data_expr);

			add_trace_store(bbOut, st->Ist.Store.addr, inst_addr,
					arg_ty, data_expr);
			addStmtToIRSB(bbOut, st);
		}
		else if (st->tag == Ist_WrTmp && dump_load)
		{
			data_expr = st->Ist.WrTmp.data;

			if (data_expr->tag == Iex_Load)
			{
				IRTemp dest = st->Ist.WrTmp.tmp;
				IRExpr *value;

				addStmtToIRSB(bbOut, st);

				value = IRExpr_RdTmp(dest);

				arg_ty = typeOfIRExpr(bbIn->tyenv, value);

				add_trace_load(bbOut, data_expr->Iex.Load.addr,
						sizeofIRType(data_expr->Iex.Load.ty),
						inst_addr, value, arg_ty);
			}
			else
				addStmtToIRSB(bbOut, st);
		}
		else
			addStmtToIRSB(bbOut, st);

	}
	return bbOut;
}

#define TF_OPT "--mmt-trace-file="
#define TN_OPT "--mmt-trace-nvidia-ioctls"
#define TO_OPT "--mmt-trace-all-opens"
#define TA_OPT "--mmt-trace-all-files"

static Bool mmt_process_cmd_line_option(Char * arg)
{
//	VG_(printf)("arg: %s\n", arg);
	if (VG_(strncmp)(arg, TF_OPT, strlen(TF_OPT)) == 0)
	{
		int i;
		for (i = 0; i < MAX_TRACE_FILES; ++i)
			if (trace_files[i].path == NULL)
				break;
		if (i == MAX_TRACE_FILES)
		{
			VG_(printf)("too many files to trace\n");
			return False;
		}
		trace_files[i].path = VG_(strdup)("mmt.options-parsing", arg + strlen(TF_OPT));
		return True;
	}
	else if (VG_(strcmp)(arg, TN_OPT) == 0)
	{
		trace_nvidia_ioctls = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TO_OPT) == 0)
	{
		trace_opens = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TA_OPT) == 0)
	{
		trace_all_files = True;
		return True;
	}

	return False;
}

static void mmt_print_usage(void)
{
	VG_(printf)("    " TF_OPT "path     trace loads and stores to memory mapped for\n"
		"                              this file (e.g. /dev/nvidia0) (you can pass \n"
		"                              this option multiple times)\n");
	VG_(printf)("    " TA_OPT "     trace loads and store to memory mapped for all files\n");
	VG_(printf)("    " TN_OPT " trace ioctls on /dev/nvidiactl\n");
	VG_(printf)("    " TO_OPT "     trace all 'open' syscalls\n");
}

static void mmt_print_debug_usage(void)
{
}

static void mmt_fini(Int exitcode)
{
}

static void mmt_post_clo_init(void)
{
}

static void dumpmem(char *s, Addr addr, UInt size)
{
	char line[4096];
	int idx = 0;

	UInt i;
	if (!addr || (addr & 0xffff0000) == 0xbeef0000)
		return;

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

static inline unsigned long long mmt_2x4to8(UInt h, UInt l)
{
	return (((unsigned long long)h) << 32) | l;
}

static void pre_ioctl(ThreadId tid, UWord *args, UInt nArgs)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UWord addr;
	UInt obj1, obj2, size;
	int i;

	if (!FD_ISSET(fd, &nvidiactl_fds))
		return;

	if ((id & 0x0000FF00) == 0x4600)
	{
		char line[4096];
		int idx = 0;

		size = ((id & 0x3FFF0000) >> 16) / 4;
		VG_(sprintf) (line, "pre_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
		idx = strlen(line);
		
		for (i = 0; i < size; ++i)
		{
			if (idx + 11 >= 4095)
				break;
			VG_(sprintf) (line + idx, "0x%08x ", data[i]);
			idx += 11;
		}
		VG_(message) (Vg_DebugMsg, "%s\n", line);
	}
	else
		VG_(message)(Vg_DebugMsg, "pre_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		// 0x23
		// c1d00041 5c000001 00000080 00000000 00000000 00000000 00000000 00000000
		// c1d0004a beef0003 000000ff 00000000 04fe8af8 00000000 00000000 00000000 
		case 0xc0204623:
			obj1 = data[1];
			VG_(message) (Vg_DebugMsg, "create device object 0x%08x\n", obj1);

			// argument can be a string (7:0, indicating the bus number), but only if 
			// argument is 0xff
			dumpmem("in ", data[4], 0x3C);

			break;
			// 0x37 read stuff from video ram?
			//case 0xc0204637:
		case 0xc0204638:
			dumpmem("in ", data[4], data[6]);
			break;
#if 1
		case 0xc0204637:
		{
			UInt *addr2;
			dumpmem("in ", data[4], data[6]);

			addr2 = (*(UInt **) (&data[4]));
			//if(data[2]==0x14c && addr2[2])
			//    dumpmem("in2 ", addr2[2], 0x40);
			break;
		}
#endif
		case 0xc020462a:
			VG_(message) (Vg_DebugMsg, "call method 0x%08x:0x%08x\n", data[1], data[2]);
			dumpmem("in ", mmt_2x4to8(data[5], data[4]), mmt_2x4to8(data[7], data[6]));
			// 0x10000002
			// word 2 is an address 
			// what is there?
			if (data[2] == 0x10000002)
			{
				UInt *addr2 = (*(UInt **) (&data[4]));
				dumpmem("in2 ", addr2[2], 0x3c);
			}
			break;

		case 0xc040464d:
			VG_(message) (Vg_DebugMsg, "in %s\n", *(char **) (&data[6]));
			break;
		case 0xc028465e:
		{
			// Copy data from mem to GPU
#if 0
			SysRes ff;
			ff = VG_(open) ("dump5e", O_CREAT | O_WRONLY | O_TRUNC, 0777);
			if (!ff.isError)
			{
				VG_(write) (ff.res, (void *) data[6], 0x01000000);
				VG_(close) (ff.res);
			}
#endif
			break;
		}
		case 0xc0104629:
			obj1 = data[1];
			obj2 = data[2];
			VG_(message) (Vg_DebugMsg, "destroy object 0x%08x:0x%08x\n", obj1, obj2);
			break;
		case 0xc020462b:
		{
			struct object_type *objtype;
			char *name = "???";
			obj1 = data[1];
			obj2 = data[2];
			addr = data[3];
			objtype = find_objtype(addr);
			if (objtype && objtype->name)
				name = objtype->name;
			VG_(message) (Vg_DebugMsg,
					"create gpu object 0x%08x:0x%08x type 0x%04lx (%s)\n",
					obj1, obj2, addr, name);
			if (data[4])
			{
				if (objtype)
					dumpmem("in ", mmt_2x4to8(data[5], data[4]), objtype->cargs * 4);
				else
					dumpmem("in ", mmt_2x4to8(data[5], data[4]), 0x40);
			}

			break;
		}
		case 0xc014462d:
			obj1 = data[1];
			obj2 = data[2];
			addr = data[3];
			VG_(message) (Vg_DebugMsg,
					"create driver object 0x%08x:0x%08x type 0x%04lx\n", obj1, obj2, addr);
			break;

	}
}

static void pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs)
{
	if (syscallno == __NR_ioctl)
		pre_ioctl(tid, args, nArgs);
}

static struct mmt_mmap_data *get_nvidia_mapping(Off64T offset)
{
	struct mmt_mmap_data *region;
	int i;
	for (i = 0; i <= last_region; ++i)
	{
		region = &mmt_mmaps[i];
		if (FD_ISSET(region->fd, &nvidia0_fds))
			if (region->offset == offset)
				return region;
	}

	if (last_region + 1 >= MAX_REGIONS)
	{
		VG_(message)(Vg_UserMsg, "no space for new mapping!\n");
		return NULL;
	}

	region = &mmt_mmaps[++last_region];
	region->id = current_item++;
	region->fd = 0;
	region->offset = offset;
	return region;
}

static inline void free_region(int idx)
{
	if (last_region != idx)
		VG_(memmove)(mmt_mmaps + idx, mmt_mmaps + idx + 1,
				(last_region - idx) * sizeof(struct mmt_mmap_data));
	VG_(memset)(&mmt_mmaps[last_region--], 0, sizeof(struct mmt_mmap_data));
}

static Addr release_nvidia_mapping(Off64T offset)
{
	int i;
	for (i = 0; i <= last_region; ++i)
	{
		struct mmt_mmap_data *region = &mmt_mmaps[i];
		if (FD_ISSET(region->fd, &nvidia0_fds))
			if (region->offset == offset)
			{
				Addr addr = region->start;
				free_region(i);
				return addr;
			}
	}
	return 0;
}

static Addr release_nvidia_mapping2(UWord data1, UWord data2)
{
	int i;
	for (i = 0; i <= last_region; ++i)
	{
		struct mmt_mmap_data *region = &mmt_mmaps[i];
		if (FD_ISSET(region->fd, &nvidia0_fds))
			if (region->data1 == data1 && region->data2 == data2)
			{
				Addr addr = region->start;
				free_region(i);
				return addr;
			}
	}
	return 0;
}

static void post_ioctl(ThreadId tid, UWord *args, UInt nArgs)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UWord addr;
	UInt obj1, obj2, size, type;
	int i;
	struct mmt_mmap_data *region;

	if (!FD_ISSET(fd, &nvidiactl_fds))
		return;

	if ((id & 0x0000FF00) == 0x4600)
	{
		char line[4096];
		int idx = 0;

		size = ((id & 0x3FFF0000) >> 16) / 4;
		VG_(sprintf) (line, "post_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
		idx = strlen(line);
		
		for (i = 0; i < size; ++i)
		{
			if (idx + 11 >= 4095)
				break;
			VG_(sprintf) (line + idx, "0x%08x ", data[i]);
			idx += 11;
		}
		VG_(message) (Vg_DebugMsg, "%s\n", line);
	}
	else
		VG_(message)(Vg_DebugMsg, "post_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	switch (id)
	{
		// NVIDIA
		case 0xc00c4622:		// Initialize
			obj1 = data[0];
			VG_(message) (Vg_DebugMsg, "created context object 0x%08x\n", obj1);
			break;

		case 0xc0204623:
			dumpmem("out", data[4], 0x3C);
			break;

		case 0xc030464e:		// Allocate map for existing object
			obj1 = data[1];
			obj2 = data[2];
			addr = data[8];
			VG_(message) (Vg_DebugMsg, "allocate map 0x%08x:0x%08x 0x%08lx\n", obj1, obj2, addr);

			region = get_nvidia_mapping(addr);
			if (region)
			{
				region->data1 = obj1;
				region->data2 = obj2;
			}

			break;
		case 0xc020464f:		// Deallocate map for existing object
			obj1 = data[1];
			obj2 = data[2];
			addr = data[4];
			/// XXX some currently mapped memory might be orphaned

			if (release_nvidia_mapping(addr))
				VG_(message) (Vg_DebugMsg, "deallocate map 0x%08x:0x%08x 0x%08lx\n", obj1, obj2, addr);

			break;
		case 0xc0304627:		// 0x27 Allocate map (also create object)
			obj1 = data[1];
			obj2 = data[2];
			type = data[3];
			addr = data[6];
			VG_(message) (Vg_DebugMsg,
					"create mapped object 0x%08x:0x%08x type=0x%08x 0x%08lx\n",
					obj1, obj2, type, addr);
			if (addr == 0)
				break;

			region = get_nvidia_mapping(addr);
			if (region)
			{
				region->data1 = obj1;
				region->data2 = obj2;
			}
#if 0
			dumpmem("out ", data[2], 0x40);
#endif
			break;
			// 0x29 seems to destroy/deallocate
		case 0xc0104629:
			obj1 = data[1];
			obj2 = data[2];
			/// XXX some currently mapped memory might be orphaned

			{
			Addr addr1 = release_nvidia_mapping2(obj1, obj2);
			if ((void *)addr1 != NULL)
				VG_(message) (Vg_DebugMsg, "deallocate map 0x%08x:0x%08x %p\n",
						obj1, obj2, (void *)addr1);
			}
			break;
			// 0x2a read stuff from video ram?
			//   i 3 pre  2a: c1d00046 c1d00046 02000014 00000000 be88a948 00000000 00000080 00000000 
		case 0xc020462a:
			dumpmem("out ", mmt_2x4to8(data[5], data[4]), mmt_2x4to8(data[7], data[6]));

			if (data[2] == 0x10000002)
			{
				UInt *addr2 = (*(UInt **) (&data[4]));
				dumpmem("out2 ", addr2[2], 0x3c);
			}
			break;
			// 0x37 read configuration parameter
		case 0xc0204638:
			dumpmem("out", data[4], data[6]);
			break;
		case 0xc0204637:
			{
				UInt *addr2 = (*(UInt **) (&data[4]));
				dumpmem("out", data[4], data[6]);
				if (data[2] == 0x14c && addr2[2])
					/// List supported object types
					dumpmem("out2 ", addr2[2], addr2[0] * 4);
			}
			break;
		case 0xc0384657:		// map GPU address
			VG_(message) (Vg_DebugMsg,
					"gpu map 0x%08x:0x%08x:0x%08x, addr 0x%08x, len 0x%08x\n",
					data[1], data[2], data[3], data[10], data[6]);
			break;
		case 0xc0284658:		// unmap GPU address
			VG_(message) (Vg_DebugMsg,
					"gpu unmap 0x%08x:0x%08x:0x%08x addr 0x%08x\n", data[1],
					data[2], data[3], data[6]);
			break;
		case 0xc0304654:		// create DMA object [3] is some kind of flags, [6] is an offset?
			VG_(message) (Vg_DebugMsg,
					"create dma object 0x%08x, type 0x%08x, parent 0x%08x\n",
					data[1], data[2], data[5]);
			break;
		case 0xc0104659:		// bind
			VG_(message) (Vg_DebugMsg, "bind 0x%08x 0x%08x\n", data[1], data[2]);
			break;
		//case 0xc01c4634:
			//    dumpmem("out", data[4], 0x40);
			//    break;
			//   to c1d00046 c1d00046 02000014 00000000, from be88a948 00000000, size 00000080 00000000
			//            2b: c1d00046 5c000001 5c000009 0000506f be88a888 00000000 00000000 00000000 
			//   same, but other way around?
			//   i 5 pre  37: c1d00046 5c000001 0000014c 00000000 be88a9c8 00000000 00000010 00000000

			// 0x23 create first object??
			// 0x2a method call? args/in/out depend 
			// 0x2b object creation
			//      c1d00046 beef0003 beef0028 0000307e
			// 0x32 gets some value
			// 0x37 read from GPU object? seems a read, not a write
			// 0x4a memory allocation
			// 0x4d after opening /dev/nvidiaX
			// 0xd2 version id check
			// 0x22 initialize (get context)
			// 0x54 bind? 0xc0304654
			// 0x57 map to card  0xc0384657
			// 0x58 unmap from card 0xc0284658
			// 0xca ??

			// These have external pointer:
			// 0x2a (offset 0x10, size 0x18) 
			// 0x2b (offset 0x10, no size specified)
			// 0x37 (offset 0x10, size 0x18)
			// 0x38 (offset 0x10, size 0x18)
	}
}

static void post_open(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	const char *path = (const char *)args[0];
	int i;

	if (trace_opens)
	{
		int flags = (int)args[1];
		int mode = (int)args[2];
		VG_(message)(Vg_DebugMsg, "sys_open: %s, flags: 0x%x, mode: 0x%x, ret: %ld\n", path, flags, mode, res._val);
	}
	if (res._isError)
		return;

	if (!trace_all_files)
	{
		for (i = 0; i < MAX_TRACE_FILES; ++i)
		{
			const char *path2 = trace_files[i].path;
			if (path2 != NULL && VG_(strcmp)(path, path2) == 0)
			{
				FD_SET(res._val, &trace_files[i].fds);
//				VG_(message)(Vg_DebugMsg, "fd %ld connected to %s\n", res._val, path);
				break;
			}
		}
	}

	if (trace_nvidia_ioctls)
	{
		if (VG_(strcmp)(path, "/dev/nvidiactl") == 0)
			FD_SET(res._val, &nvidiactl_fds);
		else if (VG_(strncmp)(path, "/dev/nvidia", 11) == 0)
			FD_SET(res._val, &nvidia0_fds);
	}
}

static void post_close(ThreadId tid, UWord *args, UInt nArgs, SysRes res)
{
	int fd = (int)args[0];
	int i;

	if (!trace_all_files)
		for(i = 0; i < MAX_TRACE_FILES; ++i)
		{
			if (trace_files[i].path != NULL && FD_ISSET(fd, &trace_files[i].fds))
			{
				FD_CLR(fd, &trace_files[i].fds);
				break;
			}
		}

	if (trace_nvidia_ioctls)
	{
		FD_CLR(fd, &nvidiactl_fds);
		FD_CLR(fd, &nvidia0_fds);
	}
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
	
	if (!trace_all_files)
	{
		for(i = 0; i < MAX_TRACE_FILES; ++i)
		{
			if (FD_ISSET(fd, &trace_files[i].fds))
				break;
		}
		if (i == MAX_TRACE_FILES)
		{
//			VG_(message)(Vg_DebugMsg, "fd %ld not found\n", fd);
			return;
		}
	}

	if (trace_nvidia_ioctls && FD_ISSET(fd, &nvidia0_fds))
	{
		for (i = 0; i <= last_region; ++i)
		{
			region = &mmt_mmaps[i];
			if (region->id > 0 &&
				(region->fd == fd || region->fd == 0) && //region->fd=0 when created from get_nvidia_mapping
				region->offset == offset * offset_unit)
			{
				region->fd = fd;
				region->start = (Addr)start;
				region->end = (Addr)(((char *)start) + len);
				VG_(message) (Vg_DebugMsg,
						"got new mmap for 0x%08lx:0x%08lx at %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
						region->data1, region->data2, (void *)region->start, len,
						region->offset, region->id);
				return;
			}
		}
	}

	if (last_region + 1 >= MAX_REGIONS)
	{
		VG_(message)(Vg_UserMsg, "not enough space for new mmap!\n");
		return;
	}

	region = &mmt_mmaps[++last_region];

	region->fd = fd;
	region->id = current_item++;
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

	for (i = 0; i <= last_region; ++i)
	{
		region = &mmt_mmaps[i];
		if (region->start == (Addr)start)
		{
			VG_(message) (Vg_DebugMsg,
					"removed mmap 0x%lx:0x%lx for: %p, len: 0x%08lx, offset: 0x%llx, serial: %d\n",
					region->data1, region->data2, (void *)region->start,
					region->end - region->start, region->offset, region->id);
			free_region(i);
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

	for (i = 0; i <= last_region; ++i)
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

static void post_syscall(ThreadId tid, UInt syscallno, UWord *args,
			UInt nArgs, SysRes res)
{
	if (syscallno == __NR_ioctl)
		post_ioctl(tid, args, nArgs);
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

static void mmt_pre_clo_init(void)
{
	int i;
	VG_(details_name) ("mmaptrace");
	VG_(details_version) (NULL);
	VG_(details_description) ("an MMAP tracer");
	VG_(details_copyright_author)
		("Copyright (C) 2007,2009, and GNU GPL'd, by Dave Airlie, W.J. van der Laan, Marcin Slusarz.");
	VG_(details_bug_reports_to) (VG_BUGS_TO);

	VG_(basic_tool_funcs) (mmt_post_clo_init, mmt_instrument, mmt_fini);

	VG_(needs_command_line_options) (mmt_process_cmd_line_option,
					 mmt_print_usage,
					 mmt_print_debug_usage);

	VG_(needs_syscall_wrapper) (pre_syscall, post_syscall);

	for (i = 0; i < MAX_TRACE_FILES; ++i)
		FD_ZERO(&trace_files[i].fds);
	FD_ZERO(&nvidiactl_fds);
	FD_ZERO(&nvidia0_fds);
}

VG_DETERMINE_INTERFACE_VERSION(mmt_pre_clo_init)
