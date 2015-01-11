#ifndef MMT_FGLRX_IOCTL_H_
#define MMT_FGLRX_IOCTL_H_

#include "pub_tool_basics.h"

extern int mmt_trace_fglrx_ioctls;

int mmt_fglrx_ioctl_pre(UWord *args);
int mmt_fglrx_ioctl_post(UWord *args, SysRes res);

#endif
