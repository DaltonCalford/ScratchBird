/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		file_params.h
 *	DESCRIPTION:	File parameter definitions
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2002.02.15 Sean Leyne - Code Cleanup, removed obsolete "EPSON" define*
 *
 * 2002.10.29 Sean Leyne - Removed obsolete "Netware" port
 *
 * 2002.10.30 Sean Leyne - Removed support for obsolete "PC_PLATFORM" define
 * 2002.10.30 Sean Leyne - Code Cleanup, removed obsolete "SUN3_3" port
 *
 */

#ifndef COMMON_FILE_PARAMS_H
#define COMMON_FILE_PARAMS_H

// Derived from ScratchBird major version
#define COMMON_FILE_PREFIX "60"

// Per-database usage
static const char* const EVENT_FILE		= "fb_event_%s";
static const char* const LOCK_FILE		= "fb_lock_%s";
static const char* const MONITOR_FILE	= "fb_monitor_%s";
static const char* const REPL_FILE 		= "fb_repl_%s";
static const char* const TPC_HDR_FILE	= "fb_tpc_%s";
static const char* const TPC_BLOCK_FILE = "fb_tpc_%s_%" UQUADFORMAT;
static const char* const SNAPSHOTS_FILE	= "fb_snap_%s";
static const char* const PROFILER_FILE	= "fb_profiler_%s_%" UQUADFORMAT;

// Global usage
static const char* const TRACE_FILE		= "fb" COMMON_FILE_PREFIX "_trace";
static const char* const USER_MAP_FILE	= "fb" COMMON_FILE_PREFIX "_user_mapping";
static const char* const SHARED_EVENT	= "fb" COMMON_FILE_PREFIX "_process%u_signal%d";

// Per-log file usage (for audit logging)
static const char* const FB_TRACE_LOG_MUTEX = "fb_trace_log_mutex";

// Per-trace session usage (for interactive trace)
static const char* const FB_TRACE_FILE = "fb_trace.";


#ifdef UNIX
static const char* const INIT_FILE		= "fb_init";
static const char* const SEM_FILE		= "fb_sem";
static const char* const PORT_FILE		= "fb_port_%d";
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif

#ifdef DARWIN
#undef FB_PREFIX
#define FB_PREFIX		"/all/files/are/in/framework/resources"
#define DARWIN_GEN_DIR		"var"
#define DARWIN_FRAMEWORK_ID	"com.firebirdsql.ScratchBird"
#endif

// keep MSG_FILE_LANG in sync with build_file.epp
#if defined(WIN_NT)
static const char* const WORKFILE	= "c:\\temp\\";
static const char MSG_FILE_LANG[]	= "intl\\%.10s.msg";
#elif defined(ANDROID)
static const char* const WORKFILE	= "/data/local/tmp/";
static const char MSG_FILE_LANG[]	= "intl/%.10s.msg";
#else
static const char* const WORKFILE	= "/tmp/";
static const char MSG_FILE_LANG[]	= "intl/%.10s.msg";
#endif

static const char* const LOCKDIR	= "scratchbird";		// created in WORKFILE

#ifndef FB_LOGFILENAME
#define FB_LOGFILENAME "scratchbird.log"
#endif

static const char* const LOGFILE	= FB_LOGFILENAME;
static const char* const MSG_FILE	= "scratchbird.msg";
static const char* const SECURITY_DB	= "security6.fdb";

// Keep in sync with MSG_FILE_LANG
const int LOCALE_MAX	= 10;

#endif // COMMON_FILE_PARAMS_H
