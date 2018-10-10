#ifndef __VKI_LINUX_DRM_NOUVEAU_H
#define __VKI_LINUX_DRM_NOUVEAU_H

// nouveau_drm.h
#define VKI_DRM_NOUVEAU_GETPARAM           0x00
#define VKI_DRM_NOUVEAU_SETPARAM           0x01
#define VKI_DRM_NOUVEAU_CHANNEL_ALLOC      0x02
#define VKI_DRM_NOUVEAU_CHANNEL_FREE       0x03
#define VKI_DRM_NOUVEAU_GROBJ_ALLOC        0x04
#define VKI_DRM_NOUVEAU_NOTIFIEROBJ_ALLOC  0x05
#define VKI_DRM_NOUVEAU_GPUOBJ_FREE        0x06
#define VKI_DRM_NOUVEAU_GEM_NEW            0x40
#define VKI_DRM_NOUVEAU_GEM_PUSHBUF        0x41
#define VKI_DRM_NOUVEAU_GEM_CPU_PREP       0x42
#define VKI_DRM_NOUVEAU_GEM_CPU_FINI       0x43
#define VKI_DRM_NOUVEAU_GEM_INFO           0x44

struct vki_drm_nouveau_channel_alloc {
	__vki_u32     fb_ctxdma_handle;
	__vki_u32     tt_ctxdma_handle;

	int          channel;
	__vki_u32     pushbuf_domains;

	/* Notifier memory */
	__vki_u32     notifier_handle;

	/* DRM-enforced subchannel assignments */
	struct {
		__vki_u32 handle;
		__vki_u32 grclass;
	} subchan[8];
	__vki_u32 nr_subchan;
};

struct vki_drm_nouveau_channel_free {
	int channel;
};

struct vki_drm_nouveau_grobj_alloc {
	int      channel;
	__vki_u32 handle;
	int      class;
};

struct vki_drm_nouveau_notifierobj_alloc {
	__vki_u32 channel;
	__vki_u32 handle;
	__vki_u32 size;
	__vki_u32 offset;
};

struct vki_drm_nouveau_gpuobj_free {
	int      channel;
	__vki_u32 handle;
};

struct vki_drm_nouveau_getparam {
	__vki_u64 param;
	__vki_u64 value;
};

struct vki_drm_nouveau_setparam {
	__vki_u64 param;
	__vki_u64 value;
};

struct vki_drm_nouveau_gem_info {
	__vki_u32 handle;
	__vki_u32 domain;
	__vki_u64 size;
	__vki_u64 offset;
	__vki_u64 map_handle;
	__vki_u32 tile_mode;
	__vki_u32 tile_flags;
};

struct vki_drm_nouveau_gem_new {
	struct vki_drm_nouveau_gem_info info;
	__vki_u32 channel_hint;
	__vki_u32 align;
};


struct vki_drm_nouveau_gem_pushbuf_bo_presumed {
	__vki_u32 valid;
	__vki_u32 domain;
	__vki_u64 offset;
};

struct vki_drm_nouveau_gem_pushbuf_bo {
	__vki_u64 user_priv;
	__vki_u32 handle;
	__vki_u32 read_domains;
	__vki_u32 write_domains;
	__vki_u32 valid_domains;
	struct vki_drm_nouveau_gem_pushbuf_bo_presumed presumed;
};

struct vki_drm_nouveau_gem_pushbuf_reloc {
	__vki_u32 reloc_bo_index;
	__vki_u32 reloc_bo_offset;
	__vki_u32 bo_index;
	__vki_u32 flags;
	__vki_u32 data;
	__vki_u32 vor;
	__vki_u32 tor;
};

struct vki_drm_nouveau_gem_pushbuf_push {
	__vki_u32 bo_index;
	__vki_u32 pad;
	__vki_u64 offset;
	__vki_u64 length;
};

struct vki_drm_nouveau_gem_pushbuf {
	__vki_u32 channel;
	__vki_u32 nr_buffers;
	__vki_u64 buffers;
	__vki_u32 nr_relocs;
	__vki_u32 nr_push;
	__vki_u64 relocs;
	__vki_u64 push;
	__vki_u32 suffix0;
	__vki_u32 suffix1;
	__vki_u64 vram_available;
	__vki_u64 gart_available;
};

struct vki_drm_nouveau_gem_cpu_prep {
	__vki_u32 handle;
	__vki_u32 flags;
};

struct vki_drm_nouveau_gem_cpu_fini {
	__vki_u32 handle;
};

#define VKI_DRM_IOCTL_NOUVEAU_GETPARAM			VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GETPARAM, struct vki_drm_nouveau_getparam)
#define VKI_DRM_IOCTL_NOUVEAU_SETPARAM			VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_SETPARAM, struct vki_drm_nouveau_setparam)
#define VKI_DRM_IOCTL_NOUVEAU_CHANNEL_ALLOC		VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_CHANNEL_ALLOC, struct vki_drm_nouveau_channel_alloc)
#define VKI_DRM_IOCTL_NOUVEAU_CHANNEL_FREE		VKI_DRM_IOW (VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_CHANNEL_FREE, struct vki_drm_nouveau_channel_free)
#define VKI_DRM_IOCTL_NOUVEAU_GROBJ_ALLOC		VKI_DRM_IOW (VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GROBJ_ALLOC, struct vki_drm_nouveau_grobj_alloc)
#define VKI_DRM_IOCTL_NOUVEAU_NOTIFIEROBJ_ALLOC		VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_NOTIFIEROBJ_ALLOC, struct vki_drm_nouveau_notifierobj_alloc)
#define VKI_DRM_IOCTL_NOUVEAU_GPUOBJ_FREE		VKI_DRM_IOW (VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GPUOBJ_FREE, struct vki_drm_nouveau_gpuobj_free)
#define VKI_DRM_IOCTL_NOUVEAU_GEM_NEW			VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GEM_NEW, struct vki_drm_nouveau_gem_new)
#define VKI_DRM_IOCTL_NOUVEAU_GEM_PUSHBUF		VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GEM_PUSHBUF, struct vki_drm_nouveau_gem_pushbuf)
#define VKI_DRM_IOCTL_NOUVEAU_GEM_CPU_PREP		VKI_DRM_IOW (VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GEM_CPU_PREP, struct vki_drm_nouveau_gem_cpu_prep)
#define VKI_DRM_IOCTL_NOUVEAU_GEM_CPU_FINI		VKI_DRM_IOW (VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GEM_CPU_FINI, struct vki_drm_nouveau_gem_cpu_fini)
#define VKI_DRM_IOCTL_NOUVEAU_GEM_INFO			VKI_DRM_IOWR(VKI_DRM_COMMAND_BASE + VKI_DRM_NOUVEAU_GEM_INFO, struct vki_drm_nouveau_gem_info)

#endif
