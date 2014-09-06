#ifndef MMT_NOUVEAU_IOCTL_H_
#define MMT_NOUVEAU_IOCTL_H_

#include "pub_tool_basics.h"

extern int mmt_trace_nouveau_ioctls;

void mmt_nouveau_ioctl_post_open(UWord *args, SysRes res);
void mmt_nouveau_ioctl_post_close(UWord *args);
void mmt_nouveau_ioctl_pre(UWord *args);
void mmt_nouveau_ioctl_post(UWord *args, SysRes res);
void mmt_nouveau_ioctl_pre_clo_init(void);


#endif
