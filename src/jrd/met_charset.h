/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_charset.h
 *	DESCRIPTION:	Character set and collation metadata functions
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

#ifndef JRD_MET_CHARSET_H
#define JRD_MET_CHARSET_H

#include "scratchbird.h"
#include "../common/classes/fb_string.h"
#include "../jrd/constants.h"

namespace Jrd {
	class thread_db;
	class QualifiedName;
}

struct SubtypeInfo;

namespace Jrd {

// Character set and collation metadata functions
bool MET_get_char_coll_subtype(thread_db* tdbb, USHORT* id, const QualifiedName& name);
bool MET_get_char_coll_subtype_info(thread_db* tdbb, USHORT id, SubtypeInfo* info);

} // namespace Jrd

#endif // JRD_MET_CHARSET_H