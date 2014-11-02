#ifndef MMT_NVIDIA_IOCTL_H_
#define MMT_NVIDIA_IOCTL_H_

#include "pub_tool_basics.h"

extern int mmt_trace_nvidia_ioctls;
extern int mmt_trace_marks;
extern int mmt_ioctl_create_fuzzer;
extern int mmt_ioctl_call_fuzzer;

struct nv_object_type
{
	UInt id;		// type id
	UInt cargs;		// number of constructor args (uint32)
};

extern struct nv_object_type mmt_nv_object_types[];
extern int mmt_nv_object_types_count;

void mmt_nv_ioctl_fini(void);
void mmt_nv_ioctl_post_clo_init(void);

void mmt_nv_ioctl_post_open(UWord *args, SysRes res);
void mmt_nv_ioctl_post_close(UWord *args);

void mmt_nv_ioctl_pre(UWord *args);
void mmt_nv_ioctl_post(UWord *args, SysRes res);

void mmt_nv_ioctl_pre_clo_init(void);

#endif /* NVIDIA_IOCTL_H_ */
