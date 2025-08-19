/*
 *	PROGRAM:	ScratchBird Access Method
 *	MODULE:		met_cache.cpp
 *	DESCRIPTION:	Metadata cache management
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

#include "firebird.h"
#include <stdio.h>
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/met_cache.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/met.h"
#include "../jrd/flags.h"
#include "../jrd/Attachment.h"
#include "../jrd/Database.h"
#include "../jrd/Function.h"

#include "../common/gdsassert.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../yvalve/gds_proto.h"
#include "../common/classes/Hash.h"

using namespace Jrd;
using namespace ScratchBird;

// Helper function to increment internal use count for statement dependencies
static void inc_int_use_count(Statement* statement)
{
	// Handle sub-statements
	for (Statement** subStatement = statement->subStatements.begin();
		 subStatement != statement->subStatements.end();
		 ++subStatement)
	{
		inc_int_use_count(*subStatement);
	}

	// Increment int_use_count for all procedures in resource list of request
	ResourceList& list = statement->resources;
	FB_SIZE_T i;

	for (list.find(Resource(Resource::rsc_procedure, 0, NULL, NULL, NULL), i);
		 i < list.getCount(); i++)
	{
		Resource& resource = list[i];
		if (resource.rsc_type != Resource::rsc_procedure)
			break;
		//// FIXME: CORE-4271: fb_assert(resource.rsc_routine->intUseCount >= 0);
		++resource.rsc_routine->intUseCount;
	}

	for (list.find(Resource(Resource::rsc_function, 0, NULL, NULL, NULL), i);
		 i < list.getCount(); i++)
	{
		Resource& resource = list[i];
		if (resource.rsc_type != Resource::rsc_function)
			break;
		//// FIXME: CORE-4271: fb_assert(resource.rsc_routine->intUseCount >= 0);
		++resource.rsc_routine->intUseCount;
	}
}

// Helper function to recursively handle trigger statement dependencies
static void inc_trigger_use_count(TrigVector* triggers)
{
	if (!triggers)
		return;

	for (FB_SIZE_T i = 0; i < triggers->getCount(); i++)
	{
		Statement* stmt = (*triggers)[i].statement;
		if (stmt && !stmt->isActive())
			inc_int_use_count(stmt);
	}
}

// Helper function to adjust dependencies for routines that cannot be deleted
static void adjust_dependencies(Routine* routine)
{
	if (routine->intUseCount == -1)
	{
		// Already processed
		return;
	}

	routine->intUseCount = -1; // Mark as undeletable

	if (routine->getStatement())
	{
		// Loop over procedures from resource list of request
		ResourceList& list = routine->getStatement()->resources;
		FB_SIZE_T i;

		for (list.find(Resource(Resource::rsc_procedure, 0, NULL, NULL, NULL), i);
			i < list.getCount(); i++)
		{
			Resource& resource = list[i];

			if (resource.rsc_type != Resource::rsc_procedure)
				break;

			routine = resource.rsc_routine;

			if (routine->intUseCount == routine->useCount)
			{
				// Mark it and all dependent procedures as undeletable
				adjust_dependencies(routine);
			}
		}

		for (list.find(Resource(Resource::rsc_function, 0, NULL, NULL, NULL), i);
			i < list.getCount(); i++)
		{
			Resource& resource = list[i];

			if (resource.rsc_type != Resource::rsc_function)
				break;

			routine = resource.rsc_routine;

			if (routine->intUseCount == routine->useCount)
			{
				// Mark it and all dependent functions as undeletable
				adjust_dependencies(routine);
			}
		}
	}
}

#ifdef DEV_BUILD
void MET_verify_cache(thread_db* tdbb)
{
/**************************************
 *
 *      M E T _ v e r i f y _ c a c h e
 *
 **************************************
 *
 * Functional description
 *      Check if all links between routines are properly counted
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* att = tdbb->getAttachment();
	if (!att)
		return;

	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine && routine->getStatement() /*&&
			!(routine->flags & Routine::FLAG_OBSOLETE)*/ )
		{
			fb_assert(routine->intUseCount == 0);
		}
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine && routine->getStatement() /*&&
			!(routine->flags & Routine::FLAG_OBSOLETE)*/ )
		{
			fb_assert(routine->intUseCount == 0);
		}
	}

	// Walk procedures and calculate internal dependencies
	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		jrd_prc* routine = *iter;

		if (routine && routine->getStatement() /*&&
			!(routine->flags & Routine::FLAG_OBSOLETE)*/ )
		{
			inc_int_use_count(routine->getStatement());
		}
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine && routine->getStatement() /*&&
			!(routine->flags & Routine::FLAG_OBSOLETE)*/ )
		{
			inc_int_use_count(routine->getStatement());
		}
	}

	// Walk procedures again and check dependencies
	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine && routine->getStatement() && /*
			 !(routine->flags & Routine::FLAG_OBSOLETE) && */
			 routine->useCount < routine->intUseCount)
		{
			string output;
			output.reserve(BUFFER_LARGE);
			char buffer[BUFFER_LARGE];
			snprintf(buffer, sizeof(buffer),
				"Procedure %d:%s is not properly counted (use count=%d, prc use=%d). Used by:\n",
				routine->getId(), routine->getName().toQuotedString().c_str(),
				routine->useCount, routine->intUseCount);
			output.append(buffer);

			for (jrd_prc** iter2 = att->att_procedures.begin(); iter2 != att->att_procedures.end(); ++iter2)
			{
				Routine* routine2 = *iter2;

				if (routine2 && routine2->getStatement() /*&& !(routine2->flags & Routine::FLAG_OBSOLETE)*/ )
				{
					// Loop over procedures from resource list of request
					const ResourceList& list = routine2->getStatement()->resources;
					FB_SIZE_T i;

					for (list.find(Resource(Resource::rsc_procedure, 0, NULL, NULL, NULL), i);
					     i < list.getCount(); i++)
					{
						const Resource& resource = list[i];
						if (resource.rsc_type != Resource::rsc_procedure)
							break;

						if (resource.rsc_routine == routine)
						{
							snprintf(buffer, sizeof(buffer), "%d:%s\n", routine2->getId(),
								routine2->getName().toQuotedString().c_str());
							output.append(buffer);
						}
					}
				}
			}

			for (Function** iter2 = att->att_functions.begin(); iter2 != att->att_functions.end(); ++iter2)
			{
				Routine* routine2 = *iter2;

				if (routine2 && routine2->getStatement() /*&& !(routine2->flags & Routine::FLAG_OBSOLETE)*/ )
				{
					// Loop over procedures from resource list of request
					const ResourceList& list = routine2->getStatement()->resources;
					FB_SIZE_T i;

					for (list.find(Resource(Resource::rsc_procedure, 0, NULL, NULL, NULL), i);
					     i < list.getCount(); i++)
					{
						const Resource& resource = list[i];
						if (resource.rsc_type != Resource::rsc_procedure)
							break;

						if (resource.rsc_routine == routine)
						{
							snprintf(buffer, sizeof(buffer), "%d:%s\n", routine2->getId(),
								routine2->getName().toQuotedString().c_str());
							output.append(buffer);
						}
					}
				}
			}

			gds__log(output.c_str());
			fb_assert(false);
		}
	}

	// Walk functions again and check dependencies
	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine && routine->getStatement() && /*
			 !(routine->flags & Routine::FLAG_OBSOLETE) && */
			 routine->useCount < routine->intUseCount)
		{
			string output;
			output.reserve(BUFFER_LARGE);
			char buffer[BUFFER_LARGE];
			snprintf(buffer, sizeof(buffer),
				"Function %d:%s is not properly counted (use count=%d, func use=%d). Used by:\n",
				routine->getId(), routine->getName().toQuotedString().c_str(),
				routine->useCount, routine->intUseCount);
			output.append(buffer);

			for (jrd_prc** iter2 = att->att_procedures.begin(); iter2 != att->att_procedures.end(); ++iter2)
			{
				Routine* routine2 = *iter2;

				if (routine2 && routine2->getStatement() /*&& !(routine2->flags & Routine::FLAG_OBSOLETE)*/ )
				{
					// Loop over procedures from resource list of request
					const ResourceList& list = routine2->getStatement()->resources;
					FB_SIZE_T i;

					for (list.find(Resource(Resource::rsc_function, 0, NULL, NULL, NULL), i);
					     i < list.getCount(); i++)
					{
						const Resource& resource = list[i];
						if (resource.rsc_type != Resource::rsc_function)
							break;

						if (resource.rsc_routine == routine)
						{
							snprintf(buffer, sizeof(buffer), "%d:%s\n", routine2->getId(),
								routine2->getName().toQuotedString().c_str());
							output.append(buffer);
						}
					}
				}
			}

			for (Function** iter2 = att->att_functions.begin(); iter2 != att->att_functions.end(); ++iter2)
			{
				Routine* routine2 = *iter2;

				if (routine2 && routine2->getStatement() /*&& !(routine2->flags & Routine::FLAG_OBSOLETE)*/ )
				{
					// Loop over procedures from resource list of request
					const ResourceList& list = routine2->getStatement()->resources;
					FB_SIZE_T i;

					for (list.find(Resource(Resource::rsc_function, 0, NULL, NULL, NULL), i);
					     i < list.getCount(); i++)
					{
						const Resource& resource = list[i];
						if (resource.rsc_type != Resource::rsc_function)
							break;

						if (resource.rsc_routine == routine)
						{
							snprintf(buffer, sizeof(buffer), "%d:%s\n", routine2->getId(),
								routine2->getName().toQuotedString().c_str());
							output.append(buffer);
						}
					}
				}
			}

			gds__log(output.c_str());
			fb_assert(false);
		}
	}

	// Fix back int_use_count
	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine)
			routine->intUseCount = 0;
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Routine* routine = *iter;

		if (routine)
			routine->intUseCount = 0;
	}
}
#endif


void MET_clear_cache(thread_db* tdbb)
{
/**************************************
 *
 *      M E T _ c l e a r _ c a c h e
 *
 **************************************
 *
 * Functional description
 *      Remove all unused objects from metadata cache to
 *      release resources they use
 *
 **************************************/
	SET_TDBB(tdbb);
#ifdef DEV_BUILD
	MET_verify_cache(tdbb);
#endif

	Attachment* const att = tdbb->getAttachment();

	// Release global (db-level and DDL) triggers

	for (unsigned i = 0; i < DB_TRIGGER_MAX; i++)
		MET_release_triggers(tdbb, &att->att_triggers[i], false);

	MET_release_triggers(tdbb, &att->att_ddl_triggers, false);

	// Release relation triggers

	vec<jrd_rel*>* const relations = att->att_relations;
	if (relations)
	{
		vec<jrd_rel*>::iterator ptr, end;
		for (ptr = relations->begin(), end = relations->end(); ptr < end; ++ptr)
		{
			jrd_rel* const relation = *ptr;
			if (!relation)
				continue;

			relation->releaseTriggers(tdbb, false);
		}
	}

	const auto walkProcFunc = [&att](std::function<void (Routine*)> func)
	{
		for (const auto routine : att->att_procedures)
		{
			if (routine)
				func(routine);
		}

		for (const auto routine : att->att_functions)
		{
			if (routine)
				func(routine);
		}
	};

	// Walk routines and calculate internal dependencies.

	walkProcFunc([](Routine* routine)
	{
		if (routine->getStatement() &&
			!(routine->flags & Routine::FLAG_OBSOLETE) )
		{
			inc_int_use_count(routine->getStatement());
		}
	});

	// Walk routines again and adjust dependencies for routines which will not be removed.

	walkProcFunc([](Routine* routine)
	{
		if (routine->getStatement() &&
			!(routine->flags & Routine::FLAG_OBSOLETE) &&
			routine->useCount != routine->intUseCount )
		{
			adjust_dependencies(routine);
		}
	});

	// Deallocate all used requests.

	walkProcFunc([&tdbb](Routine* routine)
	{
		if (routine->getStatement() && !(routine->flags & Routine::FLAG_OBSOLETE) &&
			routine->intUseCount >= 0 &&
			routine->useCount == routine->intUseCount)
		{
			routine->releaseStatement(tdbb);

			if (routine->existenceLock)
				LCK_release(tdbb, routine->existenceLock);
			routine->existenceLock = NULL;
			routine->flags |= Routine::FLAG_OBSOLETE;
		}

		// Leave it in state 0 to avoid extra pass next time to clear it
		// Note: we need to adjust intUseCount for all routines
		// in cache because any of them may have been affected from
		// dependencies earlier. Even routines that were not scanned yet !
		routine->intUseCount = 0;
	});

#ifdef DEV_BUILD
	MET_verify_cache(tdbb);
#endif
}


bool MET_routine_in_use(thread_db* tdbb, Routine* routine)
{
/**************************************
 *
 *      M E T _ r o u t i n e _ i n _ u s e
 *
 **************************************
 *
 * Functional description
 *      Check whether a routine is referenced by any other.
 *
 **************************************/
	SET_TDBB(tdbb);

#ifdef DEV_BUILD
	MET_verify_cache(tdbb);
#endif

	Attachment* const att = tdbb->getAttachment();

	// Release relation triggers are handled by releaseTriggers method
	// So we don't need to directly access individual trigger vectors here

	for (unsigned i = 0; i < DB_TRIGGER_MAX; i++)
		inc_trigger_use_count(att->att_triggers[i]);

	inc_trigger_use_count(att->att_ddl_triggers);

	// Walk procedures and calculate internal dependencies.

	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		jrd_prc* procedure = *iter;

		if (procedure && procedure->getStatement() &&
			!(procedure->flags & Routine::FLAG_OBSOLETE))
		{
			inc_int_use_count(procedure->getStatement());
		}
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Function* function = *iter;

		if (function && function->getStatement() &&
			!(function->flags & Routine::FLAG_OBSOLETE))
		{
			inc_int_use_count(function->getStatement());
		}
	}

	// Walk routines again and adjust dependencies for routines
	// which will not be removed.

	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		jrd_prc* procedure = *iter;

		if (procedure && procedure->getStatement() &&
			!(procedure->flags & Routine::FLAG_OBSOLETE) &&
			procedure->useCount != procedure->intUseCount && procedure != routine)
		{
			adjust_dependencies(procedure);
		}
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Function* function = *iter;

		if (function && function->getStatement() &&
			!(function->flags & Routine::FLAG_OBSOLETE) &&
			function->useCount != function->intUseCount && function != routine)
		{
			adjust_dependencies(function);
		}
	}

	const bool result = routine->useCount != routine->intUseCount;

	// Fix back intUseCount

	for (jrd_prc** iter = att->att_procedures.begin(); iter != att->att_procedures.end(); ++iter)
	{
		jrd_prc* procedure = *iter;

		if (procedure)
			procedure->intUseCount = 0;
	}

	for (Function** iter = att->att_functions.begin(); iter != att->att_functions.end(); ++iter)
	{
		Function* function = *iter;

		if (function)
			function->intUseCount = 0;
	}

#ifdef DEV_BUILD
	MET_verify_cache(tdbb);
#endif
	return result;
}


bool MET_dsql_cache_use(thread_db* tdbb, sym_type type, const QualifiedName& name)
{
	DSqlCacheItem* item = get_dsql_cache_item(tdbb, type, name);

	bool obsolete = false;
	item->obsoleteMap.get(name, obsolete);

	if (!item->locked)
	{
		// lock to be notified by others when we should mark as obsolete
		LCK_lock(tdbb, item->lock, LCK_SR, LCK_WAIT);
		item->locked = true;
	}

	item->obsoleteMap.put(name, false);

	return obsolete;
}


void MET_dsql_cache_release(thread_db* tdbb, sym_type type, const QualifiedName& name)
{
	DSqlCacheItem* item = get_dsql_cache_item(tdbb, type, name);

	// release the shared lock
	LCK_release(tdbb, item->lock);

	// notify others through AST to mark as obsolete
	AutoPtr<Lock> tempExLock(FB_NEW_RPT(*tdbb->getDefaultPool(), item->key.length())
		Lock(tdbb, item->key.length(), LCK_dsql_cache));
	memcpy(tempExLock->getKeyPtr(), item->key.c_str(), item->key.length());

	if (LCK_lock(tdbb, tempExLock, LCK_EX, LCK_WAIT))
		LCK_release(tdbb, tempExLock);

	item->locked = false;

	LeftPooledMap<QualifiedName, bool>::Accessor accessor(&item->obsoleteMap);
	for (bool found = accessor.getFirst(); found; found = accessor.getNext())
		accessor.current()->second = accessor.current()->first != name;
}


static int blocking_ast_dsql_cache(void* ast_object)
{
/**************************************
 *
 *	b l o c k i n g _ a s t _ d s q l _ c a c h e
 *
 **************************************
 *
 * Functional description
 *	Someone is trying to drop an item from the DSQL cache.
 *	Mark the symbol as obsolete and release the lock.
 *
 **************************************/
	DSqlCacheItem* const item = static_cast<DSqlCacheItem*>(ast_object);

	try
	{
		Database* const dbb = item->lock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, item->lock);

		LeftPooledMap<QualifiedName, bool>::Accessor accessor(&item->obsoleteMap);
		for (bool found = accessor.getFirst(); found; found = accessor.getNext())
			accessor.current()->second = true;

		item->locked = false;
		LCK_release(tdbb, item->lock);
	}
	catch (const Exception&)
	{} // no-op

	return 0;
}


static DSqlCacheItem* get_dsql_cache_item(thread_db* tdbb, sym_type type, const QualifiedName& name)
{
	Database* dbb = tdbb->getDatabase();
	Attachment* attachment = tdbb->getAttachment();

	fb_assert((int) type <= MAX_UCHAR);
	UCHAR ucharType = (UCHAR) type;

	string key("0");	// name (not hash)
	key.append((char*) &ucharType, 1);

	USHORT len = (USHORT) name.object.length();
	key.append((char*) &len, sizeof(len));
	key.append(name.object.c_str(), len);

	len = (USHORT) name.package.length();
	key.append((char*) &len, sizeof(len));
	key.append(name.package.c_str(), len);

	len = (USHORT) name.schema.length();
	key.append((char*) &len, sizeof(len));
	key.append(name.schema.c_str(), len);

	if (key.length() > MAX_UCHAR)
	{
		FB_SIZE_T hash = DefaultHash<char>::hash(key.c_str(), key.length(), sizeof(FB_SIZE_T));
		key = "1";	// hash
		key.append((char*) &ucharType, 1);
		key.append((char*) &hash, sizeof(hash));
	}

	DSqlCacheItem* item = attachment->att_dsql_cache.put(key);
	if (item)
	{
		item->key = key;
		item->lock = FB_NEW_RPT(*attachment->att_pool, key.length())
			Lock(tdbb, key.length(), LCK_dsql_cache, item, (lock_ast_t)blocking_ast_dsql_cache);
		memcpy(item->lock->getKeyPtr(), key.c_str(), key.length());
	}
	else
		item = attachment->att_dsql_cache.get(key);

	return item;
}