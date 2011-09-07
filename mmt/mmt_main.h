#ifndef MMT_MAIN_H_
#define MMT_MAIN_H_

#include "pub_tool_basics.h"

#ifdef __LP64__
#define MMT_64BIT
#endif

#define MMT_MAX_REGIONS 100

struct mmt_mmap_data {
	Addr start;
	Addr end;
	int fd;
	Off64T offset;
	UInt id;
	UWord data1;
	UWord data2;
};

extern struct mmt_mmap_data mmt_mmaps[MMT_MAX_REGIONS];
extern int mmt_last_region;
extern UInt current_item;

void mmt_free_region(int idx);

VG_REGPARM(2)
void trace_store(Addr addr, SizeT size, Addr inst_addr, UWord value);

VG_REGPARM(2)
void trace_store2(Addr addr, SizeT size, Addr inst_addr, UWord value1, UWord value2);

#ifndef MMT_64BIT
VG_REGPARM(2)
void trace_store4(Addr addr, Addr inst_addr, UWord value1, UWord value2, UWord value3, UWord value4);
#endif

VG_REGPARM(2)
void trace_load(Addr addr, SizeT size, UInt inst_addr, UWord value);

VG_REGPARM(2)
void trace_load2(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2);

#ifndef MMT_64BIT
VG_REGPARM(2)
void trace_load4(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2, UWord value3, UWord value4);
#endif


#endif /* MMT_MAIN_H_ */
