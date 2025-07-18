/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		QualifiedName.h
 *	DESCRIPTION:	Qualified metadata name holder.
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_QUALIFIEDNAME_H
#define JRD_QUALIFIEDNAME_H

#include "../jrd/MetaName.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_pair.h"
#include "../common/classes/MetaString.h"
#include "../common/classes/QualifiedMetaString.h"
#include "../common/StatusArg.h"

namespace Jrd {

using QualifiedName = ScratchBird::BaseQualifiedName<MetaName>;
using QualifiedNameMetaNamePair = ScratchBird::FullPooledPair<QualifiedName, MetaName>;
using QualifiedNamePair = ScratchBird::FullPooledPair<QualifiedName, QualifiedName>;

} // namespace Jrd

#endif // JRD_QUALIFIEDNAME_H
