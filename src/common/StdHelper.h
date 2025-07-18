/*
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
 *  Copyright (c) 2024 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#ifndef FB_COMMON_STD_HELPER_H
#define FB_COMMON_STD_HELPER_H

namespace ScratchBird {

// To be used with std::visit

template <typename... Ts>
struct StdVisitOverloads : Ts...
{
	using Ts::operator()...;
};

template <typename... Ts>
StdVisitOverloads(Ts...) -> StdVisitOverloads<Ts...>;

}	// namespace ScratchBird

#endif	// FB_COMMON_STD_HELPER_H
