/*
   Copyright (C) 2006 Dave Airlie
   Copyright (C) 2007 Wladimir J. van der Laan
   Copyright (C) 2009 Marcin Slusarz <marcin.slusarz@gmail.com>

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

/*
	Vg_UserMsg for important messages
	Vg_DebugMsg for memory load/store messages
	Vg_DebugExtraMsg for other messages 
*/

#include "pub_tool_basics.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"

#include <fcntl.h>

#include "mmt_nv_ioctl.h"
#include "mmt_nouveau_ioctl.h"
#include "mmt_instrument.h"
#include "mmt_trace.h"

#define TF_OPT "--mmt-trace-file="
#define TN_OPT "--mmt-trace-nvidia-ioctls"
#define TO_OPT "--mmt-trace-all-opens"
#define TA_OPT "--mmt-trace-all-files"
#define TMEM_OPT "--mmt-trace-all-mem"
#define TM_OPT "--mmt-trace-marks"
#define TV_OPT "--mmt-trace-nouveau-ioctls"
#define TS_OPT "--mmt-trace-stdout-stderr"
#define FZ_OPT "--mmt-ioctl-create-fuzzer="
#define OT_OPT "--mmt-object-ctr="
#define FC_OPT "--mmt-ioctl-call-fuzzer="
#define SF_OPT "--mmt-sync-file="

static char *mmt_sync_file = NULL;
static Bool mmt_process_cmd_line_option(const HChar *arg)
{
//	VG_(printf)("arg: %s\n", arg);
	if (VG_(strncmp)(arg, TF_OPT, VG_(strlen(TF_OPT))) == 0)
	{
		int i;
		for (i = 0; i < MMT_MAX_TRACE_FILES; ++i)
			if (mmt_trace_files[i] == NULL)
				break;
		if (i == MMT_MAX_TRACE_FILES)
		{
			VG_(printf)("too many files to trace\n");
			return False;
		}
		mmt_trace_files[i] = VG_(strdup)("mmt.options-parsing", arg + VG_(strlen(TF_OPT)));
		return True;
	}
	else if (VG_(strcmp)(arg, TN_OPT) == 0)
	{
		mmt_trace_nvidia_ioctls = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TO_OPT) == 0)
	{
		mmt_trace_all_opens = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TA_OPT) == 0)
	{
		mmt_trace_all_files = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TM_OPT) == 0)
	{
		mmt_trace_marks = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TV_OPT) == 0)
	{
		mmt_trace_nouveau_ioctls = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TS_OPT) == 0)
	{
		mmt_trace_stdout_stderr = True;
		return True;
	}
	else if (VG_(strncmp)(arg, FZ_OPT, VG_(strlen(FZ_OPT))) == 0)
	{
		const HChar *val = arg + VG_(strlen(FZ_OPT));
		if (val[0] >= '0' && val[0] <= '2')
		{
			mmt_ioctl_create_fuzzer = val[0] - '0';
			return True;
		}
		return False;
	}
	else if (VG_(strncmp)(arg, OT_OPT, VG_(strlen(OT_OPT))) == 0)
	{
		const HChar *val = arg + VG_(strlen(OT_OPT));
		HChar *comma = VG_(strchr)(val, ',');
		if (!comma)
			return False;
		*comma = 0;

		Long cls = VG_(strtoll16)(val, NULL);
		Long sz = VG_(strtoll10)(comma + 1, NULL);
		int i;
		for (i = 0; i < mmt_nv_object_types_count; ++i)
			if (mmt_nv_object_types[i].id == cls)
			{
				mmt_nv_object_types[i].cargs = sz;
				return True;
			}
		for (i = 0; i < mmt_nv_object_types_count; ++i)
			if (mmt_nv_object_types[i].id == -1)
			{
				mmt_nv_object_types[i].id = cls;
				mmt_nv_object_types[i].cargs = sz;
				return True;
			}

		// not enough space
		return False;
	}
	else if (VG_(strncmp)(arg, FC_OPT, VG_(strlen(FC_OPT))) == 0)
	{
		const HChar *val = arg + VG_(strlen(FC_OPT));
		if (val[0] >= '0' && val[0] <= '1')
		{
			mmt_ioctl_call_fuzzer = val[0] - '0';
			return True;
		}
		return False;
	}
	else if (VG_(strncmp)(arg, SF_OPT, VG_(strlen(SF_OPT))) == 0)
	{
		const HChar *val = arg + VG_(strlen(SF_OPT));
		mmt_sync_file = VG_(strdup)("mmt.options-parsing", val);
		return True;
	}
	else if (VG_(strcmp)(arg, TMEM_OPT) == 0)
	{
		all_mem = 1;
		return True;
	}

	return False;
}

static void mmt_print_usage(void)
{
	VG_(printf)("    " TF_OPT "path       trace loads and stores to memory mapped for\n"
		 "                                this file (e.g. /dev/nvidia0) (you can pass \n"
		 "                                this option multiple times)\n");
	VG_(printf)("    " TA_OPT     "       trace loads and stores to memory mapped for\n\t\t\t\tall files\n");
	VG_(printf)("    " TMEM_OPT "         trace loads and stores for all memory\n");
	VG_(printf)("    " TN_OPT         "   trace nvidia ioctls on /dev/nvidiactl and\n\t\t\t\t/dev/nvidia0\n");
	VG_(printf)("    " TV_OPT          "  trace nouveau ioctls on /dev/dri/cardX\n");
	VG_(printf)("    " TO_OPT     "       trace all 'open' syscalls\n");
	VG_(printf)("    " TM_OPT "           send mmiotrace marks before and after ioctls\n");
	VG_(printf)("    " TS_OPT         "   trace writes to stdout and stderr\n");
	VG_(printf)("    " FZ_OPT          "  0-disabled (default), 1-enabled (safe),\n\t\t\t\t2-enabled (unsafe)\n");
	VG_(printf)("    " OT_OPT          "class,cargs sets the number of u32 constructor args(dec)\n\t\t\t\tfor specified class(hex)\n");
	VG_(printf)("    " FC_OPT        "    0-disabled (default), 1-enabled\n");
	VG_(printf)("    " SF_OPT"path        emit synchronization markers in output stream\n\t\t\t\tand wait for replies from specified file\n");
}

static void mmt_print_debug_usage(void)
{
}

static void mmt_fini(Int exitcode)
{
	mmt_nv_ioctl_fini();
}

static void mmt_post_clo_init(void)
{
	mmt_nv_ioctl_post_clo_init();

	if (mmt_sync_file)
	{
		SysRes r = VG_(open)(mmt_sync_file, O_RDONLY, 0);
		tl_assert2(!sr_isError(r), "cannot open file %s: %d\n", mmt_sync_file, sr_Err(r));
		mmt_sync_fd = sr_Res(r);
	}
}

static void mmt_pre_clo_init(void)
{
	VG_(details_name) ("mmaptrace");
	VG_(details_version) (NULL);
	VG_(details_description) ("an MMAP tracer");
	VG_(details_copyright_author)
		("Copyright (C) 2007,2009,2011,2014 and GNU GPL'd, by Dave Airlie, W.J. van der Laan, Marcin Slusarz.");
	VG_(details_bug_reports_to) (VG_BUGS_TO);

	VG_(basic_tool_funcs) (mmt_post_clo_init, mmt_instrument, mmt_fini);

	VG_(needs_command_line_options) (mmt_process_cmd_line_option,
					 mmt_print_usage,
					 mmt_print_debug_usage);

	VG_(needs_syscall_wrapper) (mmt_pre_syscall, mmt_post_syscall);

	FD_ZERO(&trace_fds);
}

VG_DETERMINE_INTERFACE_VERSION(mmt_pre_clo_init)
