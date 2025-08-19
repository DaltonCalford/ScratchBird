/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_generators.h
 *	DESCRIPTION:	Generator/sequence metadata management
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

#ifndef JRD_MET_GENERATORS_H
#define JRD_MET_GENERATORS_H

#include "scratchbird.h"

namespace Jrd {
// Forward declarations
class thread_db;
class GeneratorItem;
class QualifiedName;
}

namespace ScratchBird {

// Generator/sequence metadata functions
bool MET_load_generator(Jrd::thread_db* tdbb, Jrd::GeneratorItem& item, bool* sysGen = nullptr, SLONG* step = nullptr);
SLONG MET_lookup_generator(Jrd::thread_db* tdbb, const Jrd::QualifiedName& name, bool* sysGen = nullptr, SLONG* step = nullptr);
bool MET_lookup_generator_id(Jrd::thread_db* tdbb, SLONG gen_id, Jrd::QualifiedName& name, bool* sysGen = nullptr);
void MET_update_generator_increment(Jrd::thread_db* tdbb, SLONG gen_id, SLONG step);

} // namespace ScratchBird

#endif // JRD_MET_GENERATORS_H