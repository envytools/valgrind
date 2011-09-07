#ifndef MMT_MAIN_H_
#define MMT_MAIN_H_

#include "pub_tool_basics.h"

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

#endif /* MMT_MAIN_H_ */
