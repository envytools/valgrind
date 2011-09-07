#ifndef MMT_TRACE_H_
#define MMT_TRACE_H_

#include "pub_tool_basics.h"

#include <sys/select.h>

#ifdef __LP64__
#define MMT_64BIT
#endif

#define MMT_MAX_TRACE_FILES 10
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

struct mmt_trace_file {
	const char *path;
	fd_set fds;
};

extern struct mmt_mmap_data mmt_mmaps[MMT_MAX_REGIONS];
extern int mmt_last_region;

extern UInt mmt_current_item;

extern int mmt_trace_opens;
extern struct mmt_trace_file mmt_trace_files[MMT_MAX_TRACE_FILES];
extern int mmt_trace_all_files;

void mmt_free_region(int idx);

void mmt_pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs);

void mmt_post_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs, SysRes res);

VG_REGPARM(2)
void mmt_trace_store(Addr addr, SizeT size, Addr inst_addr, UWord value);

VG_REGPARM(2)
void mmt_trace_store2(Addr addr, SizeT size, Addr inst_addr, UWord value1, UWord value2);

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_store4(Addr addr, Addr inst_addr, UWord value1, UWord value2, UWord value3, UWord value4);
#endif

VG_REGPARM(2)
void mmt_trace_load(Addr addr, SizeT size, UInt inst_addr, UWord value);

VG_REGPARM(2)
void mmt_trace_load2(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2);

#ifndef MMT_64BIT
VG_REGPARM(2)
void mmt_trace_load4(Addr addr, SizeT size, UInt inst_addr, UWord value1, UWord value2, UWord value3, UWord value4);
#endif


#endif /* MMT_TRACE_H_ */
