#ifndef MMT_NVIDIA_IOCTL_H_
#define MMT_NVIDIA_IOCTL_H_

#include "pub_tool_basics.h"

extern int mmt_trace_nvidia_ioctls;
extern int mmt_trace_marks;

void mmt_nv_ioctl_fini(void);
void mmt_nv_ioctl_post_clo_init(void);

void mmt_nv_ioctl_post_open(UWord *args, SysRes res);
void mmt_nv_ioctl_post_close(UWord *args);

int mmt_nv_ioctl_post_mmap(UWord *args, SysRes res, int offset_unit);

void mmt_nv_ioctl_pre(UWord *args);
void mmt_nv_ioctl_post(UWord *args);

void mmt_nv_ioctl_pre_clo_init(void);

#endif /* NVIDIA_IOCTL_H_ */
