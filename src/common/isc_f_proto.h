/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		isc_f_proto.h
 *	DESCRIPTION:	Prototype header file for isc_file.cpp
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
 * 2002.10.29 Sean Leyne - Removed support for obsolete IPX/SPX Protocol
 *
 */

#ifndef JRD_ISC_FILE_PROTO_H
#define JRD_ISC_FILE_PROTO_H

#include "../common/classes/fb_string.h"
#include "../common/common.h"

enum iscProtocol {ISC_PROTOCOL_LOCAL, ISC_PROTOCOL_TCPIP};

#ifndef NO_NFS
bool		ISC_analyze_nfs(ScratchBird::PathName&, ScratchBird::PathName&);
#endif
#ifdef WIN_NT
bool		ISC_analyze_pclan(ScratchBird::PathName&, ScratchBird::PathName&);
#endif
bool		ISC_analyze_protocol(const char*, ScratchBird::PathName&, ScratchBird::PathName&, const char*, bool needFile);
bool		ISC_analyze_tcp(ScratchBird::PathName&, ScratchBird::PathName&, bool = true);
bool		ISC_check_if_remote(const ScratchBird::PathName&, bool);
iscProtocol	ISC_extract_host(ScratchBird::PathName&, ScratchBird::PathName&, bool);
bool		ISC_expand_filename(ScratchBird::PathName&, bool);
void		ISC_systemToUtf8(ScratchBird::AbstractString& str);
void		ISC_utf8ToSystem(ScratchBird::AbstractString& str);
void		ISC_escape(ScratchBird::AbstractString& str);
void		ISC_unescape(ScratchBird::AbstractString& str);

// This form of ISC_expand_filename makes epp files happy
inline bool	ISC_expand_filename(const TEXT* unexpanded, USHORT len_unexpanded,
								TEXT* expanded, FB_SIZE_T len_expanded,
								bool expand_share)
{
	ScratchBird::PathName pn(unexpanded, len_unexpanded ? len_unexpanded : fb_strlen(unexpanded));
	ISC_expand_filename(pn, expand_share);
	// What do I return here if the previous call returns false?
	return (pn.copyTo(expanded, len_expanded) != 0);
}

void		ISC_expand_share(ScratchBird::PathName&);
int			ISC_file_lock(SSHORT);
int			ISC_file_unlock(SSHORT);

#endif // JRD_ISC_FILE_PROTO_H
