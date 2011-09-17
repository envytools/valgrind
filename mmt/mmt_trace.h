#ifndef MMT_TRACE_H_
#define MMT_TRACE_H_

#include "pub_tool_basics.h"

#include <sys/select.h>

#ifdef __LP64__
#define MMT_64BIT
#endif

//#define MMT_PRINT_FILENAMES
#define MMT_DEBUG
//#define MMT_DEBUG_VERBOSE
#define MMT_MAX_TRACE_FILES 10
#define MMT_MAX_REGIONS 1000

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

#define maybe_unused __attribute__((unused))

extern int mmt_trace_opens;
extern struct mmt_trace_file mmt_trace_files[MMT_MAX_TRACE_FILES];
extern int mmt_trace_all_files;

void mmt_free_region(struct mmt_mmap_data *m);
struct mmt_mmap_data *mmt_add_region(int fd, Addr start, Addr end,
		Off64T offset, UInt id, UWord data1, UWord data2);

struct mmt_mmap_data *mmt_find_region_by_fd_offset(int fd, Off64T offset);
struct mmt_mmap_data *mmt_find_region_by_fdset_offset(fd_set *fds, Off64T offset);
struct mmt_mmap_data *mmt_find_region_by_fdset_data(fd_set *fds, UWord data1, UWord data2);

void mmt_pre_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs);

void mmt_post_syscall(ThreadId tid, UInt syscallno, UWord *args, UInt nArgs, SysRes res);

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
#endif

#ifndef MMT_64BIT
	VG_REGPARM(2)
	void mmt_trace_load_4_4_4_4(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4);
	VG_REGPARM(2)
	void mmt_trace_load_4_4_4_4_ia(Addr addr, UWord value1, UWord value2, UWord value3, UWord value4, Addr inst_addr);
#endif


#endif /* MMT_TRACE_H_ */
