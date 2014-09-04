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
#include "pub_tool_libcbase.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_mallocfree.h"

#include "mmt_nv_ioctl.h"
#include "mmt_nouveau_ioctl.h"
#include "mmt_instrument.h"
#include "mmt_trace.h"

#define TF_OPT "--mmt-trace-file="
#define TN_OPT "--mmt-trace-nvidia-ioctls"
#define TO_OPT "--mmt-trace-all-opens"
#define TA_OPT "--mmt-trace-all-files"
#define TM_OPT "--mmt-trace-marks"
#define TV_OPT "--mmt-trace-nouveau-ioctls"

static Bool mmt_process_cmd_line_option(const HChar *arg)
{
//	VG_(printf)("arg: %s\n", arg);
	if (VG_(strncmp)(arg, TF_OPT, VG_(strlen(TF_OPT))) == 0)
	{
		int i;
		for (i = 0; i < MMT_MAX_TRACE_FILES; ++i)
			if (mmt_trace_files[i].path == NULL)
				break;
		if (i == MMT_MAX_TRACE_FILES)
		{
			VG_(printf)("too many files to trace\n");
			return False;
		}
		mmt_trace_files[i].path = VG_(strdup)("mmt.options-parsing", arg + VG_(strlen(TF_OPT)));
		return True;
	}
	else if (VG_(strcmp)(arg, TN_OPT) == 0)
	{
		mmt_trace_nvidia_ioctls = True;
		return True;
	}
	else if (VG_(strcmp)(arg, TO_OPT) == 0)
	{
		mmt_trace_opens = True;
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

	return False;
}

static void mmt_print_usage(void)
{
	VG_(printf)("    " TF_OPT "path     trace loads and stores to memory mapped for\n"
		"                              this file (e.g. /dev/nvidia0) (you can pass \n"
		"                              this option multiple times)\n");
	VG_(printf)("    " TA_OPT     "     trace loads and stores to memory mapped for all files\n");
	VG_(printf)("    " TN_OPT         " trace nvidia ioctls on /dev/nvidiactl and /dev/nvidia0\n");
	VG_(printf)("    " TV_OPT      "    trace nouveau ioctls on /dev/dri/cardX\n");
	VG_(printf)("    " TO_OPT     "     trace all 'open' syscalls\n");
	VG_(printf)("    " TM_OPT "         send mmiotrace marks before and after ioctls\n");
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
}

static void mmt_pre_clo_init(void)
{
	int i;
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

	for (i = 0; i < MMT_MAX_TRACE_FILES; ++i)
		FD_ZERO(&mmt_trace_files[i].fds);

	mmt_nv_ioctl_pre_clo_init();
	mmt_nouveau_ioctl_pre_clo_init();
}

VG_DETERMINE_INTERFACE_VERSION(mmt_pre_clo_init)
