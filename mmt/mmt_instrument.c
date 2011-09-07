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

#include "mmt_instrument.h"
#include "mmt_main.h"

#include "pub_tool_machine.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcassert.h"

int dump_load = True, dump_store = True;

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

IRSB *mmt_instrument(VgCallbackClosure *closure,
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
