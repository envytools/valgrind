#ifndef MMT_TRACE_TEXT_H
#define MMT_TRACE_TEXT_H

#include "pub_tool_basics.h"

#include "mmt_trace.h"

VG_REGPARM(2)
void mmt_trace_store_1(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_store_1_ia(Addr addr, UWord value, Addr inst_addr);
VG_REGPARM(2)
void mmt_trace_store_2(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_store_2_ia(Addr addr, UWord value, Addr inst_addr);
VG_REGPARM(2)
void mmt_trace_store_4(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_store_4_ia(Addr addr, UWord value, Addr inst_addr);

#ifdef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_store_8(Addr addr, UWord value);
	VG_REGPARM(2)
	void mmt_trace_store_8_ia(Addr addr, UWord value, Addr inst_addr);
#endif

VG_REGPARM(2)
void mmt_trace_store_4_4(Addr addr, UWord value1, UWord value2);
VG_REGPARM(2)
void mmt_trace_store_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr);

#ifdef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_store_8_8(Addr addr, UWord value1, UWord value2);
	VG_REGPARM(2)
	void mmt_trace_store_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr);
	VG_REGPARM(2)
	void mmt_trace_store_8_8_8_8(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4);
	VG_REGPARM(2)
	void mmt_trace_store_8_8_8_8_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr);
#endif

#ifndef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_store_4_4_4_4(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4);
	VG_REGPARM(2)
	void mmt_trace_store_4_4_4_4_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr);
#endif

VG_REGPARM(2)
void mmt_trace_load_1(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_load_1_ia(Addr addr, UWord value, Addr inst_addr);
VG_REGPARM(2)
void mmt_trace_load_2(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_load_2_ia(Addr addr, UWord value, Addr inst_addr);
VG_REGPARM(2)
void mmt_trace_load_4(Addr addr, UWord value);
VG_REGPARM(2)
void mmt_trace_load_4_ia(Addr addr, UWord value, Addr inst_addr);

#ifdef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_load_8(Addr addr, UWord value);
	VG_REGPARM(2)
	void mmt_trace_load_8_ia(Addr addr, UWord value, Addr inst_addr);
#endif

VG_REGPARM(2)
void mmt_trace_load_4_4(Addr addr, UWord value1, UWord value2);
VG_REGPARM(2)
void mmt_trace_load_4_4_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr);

#ifdef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_load_8_8(Addr addr, UWord value1, UWord value2);
	VG_REGPARM(2)
	void mmt_trace_load_8_8_ia(Addr addr, UWord value1, UWord value2, Addr inst_addr);
	VG_REGPARM(2)
	void mmt_trace_load_8_8_8_8(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4);
	VG_REGPARM(2)
	void mmt_trace_load_8_8_8_8_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr);
#endif

#ifndef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_load_4_4_4_4(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4);
	VG_REGPARM(2)
	void mmt_trace_load_4_4_4_4_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr);
#endif

#endif
