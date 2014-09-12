/*
   Copyright (C) 2012,2014 Marcin Slusarz <marcin.slusarz@gmail.com>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "mmt_nouveau_ioctl.h"
#include "mmt_trace_bin.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "vki-linux-drm-nouveau.h"

#include <sys/select.h>

static fd_set nouveau_fds;
int mmt_trace_nouveau_ioctls = False;

void mmt_nouveau_ioctl_post_open(UWord *args, SysRes res)
{
	const char *path = (const char *)args[0];

	if (mmt_trace_nouveau_ioctls)
	{
		if (VG_(strncmp)(path, "/dev/dri/card", 13) == 0)
		{
			FD_SET(res._val, &nouveau_fds);
			mmt_dump_open(args, res);
		}
	}
}

void mmt_nouveau_ioctl_post_close(UWord *args)
{
	int fd = (int)args[0];

	if (mmt_trace_nouveau_ioctls)
		FD_CLR(fd, &nouveau_fds);
}

static void mmt_nouveau_pushbuf(struct vki_drm_nouveau_gem_pushbuf *pushbuf)
{
	struct vki_drm_nouveau_gem_pushbuf_bo *buffers = ULong_to_Ptr(pushbuf->buffers);
	struct vki_drm_nouveau_gem_pushbuf_push *push = ULong_to_Ptr(pushbuf->push);
	struct vki_drm_nouveau_gem_pushbuf_reloc *relocs = ULong_to_Ptr(pushbuf->relocs);

	mmt_bin_write_1('n');
	mmt_bin_write_1('P');
	int size = pushbuf->nr_buffers * sizeof(buffers[0]) + pushbuf->nr_push * sizeof(push[0]) + pushbuf->nr_relocs * sizeof(relocs[0]);
	mmt_bin_write_4(size + 3 * 4);
	mmt_bin_write_buffer((UChar *)buffers, pushbuf->nr_buffers * sizeof(buffers[0]));
	mmt_bin_write_buffer((UChar *)push, pushbuf->nr_push * sizeof(push[0]));
	mmt_bin_write_buffer((UChar *)relocs, pushbuf->nr_relocs * sizeof(relocs[0]));
	mmt_bin_end();
}

void mmt_nouveau_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;

	if (!FD_ISSET(fd, &nouveau_fds))
		return;

	if ((id & 0x0000FF00) == 0x6400)
	{
		size = (id & 0x3FFF0000) >> 16;

		mmt_bin_write_1('n');
		mmt_bin_write_1('i');
		mmt_bin_write_4(fd);
		mmt_bin_write_4(id);
		mmt_bin_write_buffer((UChar *)data, size);
		mmt_bin_end();

		if ((id & 0xff) == 0x40 + 0x41) // DRM_NOUVEAU_GEM_PUSHBUF
			mmt_nouveau_pushbuf((void *)data);
	}
	else
	{
		mmt_bin_flush();
		VG_(message)(Vg_UserMsg, "pre_ioctl, fd: %d, wrong id:0x%x\n", fd, id);
	}
}

void mmt_nouveau_ioctl_post(UWord *args, SysRes res)
{
	int fd = args[0];
	UInt id = args[1];
	void *data = (void *) args[2];
	UInt size;

	if (!FD_ISSET(fd, &nouveau_fds))
		return;

	if ((id & 0x0000FF00) == 0x6400)
	{
		size = (id & 0x3FFF0000) >> 16;

		mmt_bin_write_1('n');
		mmt_bin_write_1('j');
		mmt_bin_write_4(fd);
		mmt_bin_write_4(id);
		mmt_bin_write_buffer((UChar *)data, size);
		mmt_bin_end();
	}
	else
	{
		mmt_bin_flush();
		VG_(message)(Vg_UserMsg, "post_ioctl, fd: %d, wrong id:0x%x\n", fd, id);
	}
}

void mmt_nouveau_ioctl_pre_clo_init(void)
{
	FD_ZERO(&nouveau_fds);
}
