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
			FD_SET(res._val, &nouveau_fds);
	}
}

void mmt_nouveau_ioctl_post_close(UWord *args)
{
	int fd = (int)args[0];

	if (mmt_trace_nouveau_ioctls)
		FD_CLR(fd, &nouveau_fds);
}

void mmt_nouveau_ioctl_pre(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	UInt *data = (UInt *) args[2];
	UInt size;
	int i;

	if (!FD_ISSET(fd, &nouveau_fds))
		return;

	if ((id & 0x0000FF00) == 0x6400)
	{
		size = (id & 0x3FFF0000) >> 16;
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('i');
			mmt_bin_write_4(fd);
			mmt_bin_write_4(id);
			mmt_bin_write_buffer((UChar *)data, size);
			mmt_bin_end();
		}
		else
		{
			char line[4096];
			int idx = 0;

			VG_(sprintf) (line, "pre_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
			idx = VG_(strlen(line));

			for (i = 0; i < size / 4; ++i)
			{
				if (idx + 11 >= 4095)
					break;
				VG_(sprintf) (line + idx, "0x%08x ", data[i]);
				idx += 11;
			}
			VG_(message) (Vg_DebugMsg, "%s\n", line);
		}
	}
	else
		VG_(message)(Vg_DebugMsg, "pre_ioctl, fd: %d, wrong id:0x%x\n", fd, id);
}

void mmt_nouveau_ioctl_post(UWord *args)
{
	int fd = args[0];
	UInt id = args[1];
	void *data = (void *) args[2];
	UInt *dataUint = (UInt *) args[2];
	UInt size;
	int i;

	if (!FD_ISSET(fd, &nouveau_fds))
		return;

	if ((id & 0x0000FF00) == 0x6400)
	{
		size = (id & 0x3FFF0000) >> 16;

		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('j');
			mmt_bin_write_4(fd);
			mmt_bin_write_4(id);
			mmt_bin_write_buffer((UChar *)data, size);
			mmt_bin_end();
		}
		else
		{
			char line[4096];
			int idx = 0;

			VG_(sprintf) (line, "post_ioctl: fd:%d, id:0x%02x (full:0x%x), data: ", fd, id & 0xFF, id);
			idx = VG_(strlen(line));

			for (i = 0; i < size / 4; ++i)
			{
				if (idx + 11 >= 4095)
					break;
				VG_(sprintf) (line + idx, "0x%08x ", dataUint[i]);
				idx += 11;
			}
			VG_(message) (Vg_DebugMsg, "%s\n", line);
		}
	}
	else
		VG_(message)(Vg_DebugMsg, "post_ioctl, fd: %d, wrong id:0x%x\n", fd, id);

	if (id == VKI_DRM_IOCTL_NOUVEAU_GROBJ_ALLOC)
	{
		struct vki_drm_nouveau_grobj_alloc *arg = data;
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('c');
			mmt_bin_write_4(0);
			mmt_bin_write_4(arg->handle);
			mmt_bin_write_4(arg->class);
			mmt_bin_write_str("");
			mmt_bin_end();
		}
		else
			VG_(message) (Vg_DebugMsg,
					"create gpu object 0x%08x:0x%08x type 0x%04x (%s)\n",
					0, arg->handle, arg->class, "");
	}
	else if (id == VKI_DRM_IOCTL_NOUVEAU_GPUOBJ_FREE)
	{
		struct vki_drm_nouveau_gpuobj_free *arg = data;
		if (mmt_binary_output)
		{
			mmt_bin_write_1('n');
			mmt_bin_write_1('d');
			mmt_bin_write_4(0);
			mmt_bin_write_4(arg->handle);
			mmt_bin_end();
		}
		else
			VG_(message) (Vg_DebugMsg, "destroy object 0x%08x:0x%08x\n", 0, arg->handle);
	}
}

void mmt_nouveau_ioctl_pre_clo_init(void)
{
	FD_ZERO(&nouveau_fds);
}
