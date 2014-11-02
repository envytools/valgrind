#ifndef MMT_NOUVEAU_IOCTL_H_
#define MMT_NOUVEAU_IOCTL_H_

#include "pub_tool_basics.h"

extern int mmt_trace_nouveau_ioctls;

int mmt_nouveau_ioctl_pre(UWord *args);
int mmt_nouveau_ioctl_post(UWord *args, SysRes res);

#endif
