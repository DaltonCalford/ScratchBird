/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_shadows.h
 *	DESCRIPTION:	Shadow/backup file metadata management
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
 */

#ifndef JRD_MET_SHADOWS_H
#define JRD_MET_SHADOWS_H

#include "scratchbird.h"

namespace Jrd {
// Forward declarations
class thread_db;
class Shadow;
}

// Shadow/backup file metadata functions
void MET_activate_shadow(Jrd::thread_db* tdbb);
void MET_delete_shadow(Jrd::thread_db* tdbb, USHORT shadow_number);
void MET_get_shadow_files(Jrd::thread_db* tdbb, bool delete_files);
void MET_update_shadow(Jrd::thread_db* tdbb, Jrd::Shadow* shadow, USHORT file_flags);

#endif // JRD_MET_SHADOWS_H