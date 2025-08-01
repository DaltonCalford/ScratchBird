/*
 *	PROGRAM:	ScratchBird Access Method
 *	MODULE:		met_cache.h
 *	DESCRIPTION:	Metadata cache management declarations
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

#ifndef JRD_MET_CACHE_H
#define JRD_MET_CACHE_H

#include "firebird.h"
#include "../jrd/jrd.h"
#include "../jrd/QualifiedName.h"
#include "../common/classes/MetaString.h"

namespace Jrd {

// Forward declarations
struct thread_db;
class Routine;
struct DSqlCacheItem;

// Cache management functions

#ifdef DEV_BUILD
// Development build cache verification
void MET_verify_cache(thread_db* tdbb);
#endif

// Clear all unused objects from metadata cache
void MET_clear_cache(thread_db* tdbb);

// Check if routine is currently in use
bool MET_routine_in_use(thread_db* tdbb, Routine* routine);

// DSQL cache management
bool MET_dsql_cache_use(thread_db* tdbb, sym_type type, const QualifiedName& name);
void MET_dsql_cache_release(thread_db* tdbb, sym_type type, const QualifiedName& name);

// Internal helper functions (defined in this module)
// Note: These are internal to the cache module and not exposed publicly

} // namespace Jrd

#endif // JRD_MET_CACHE_H