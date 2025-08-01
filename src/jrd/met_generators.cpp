/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		met_generators.cpp
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
 *
 * 2001.6.25 Claudio Valderrama: Finish MET_lookup_generator_id() by
 *	assigning it a number in the compiled requests table.
 */

#include "scratchbird.h"
#include <stdio.h>
#include <string.h>

#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/met.h"
#include "../jrd/irq.h"
#include "../jrd/constants.h"
#include "../jrd/exe_proto.h"
#include "../common/gdsassert.h"
#include "met_generators.h"

#define MAX_SQL_IDENTIFIER_LEN 68

using namespace Jrd;

namespace ScratchBird {

bool MET_load_generator(Jrd::thread_db* tdbb, Jrd::GeneratorItem& item, bool* sysGen, SLONG* step)
{
/**************************************
 *
 *      M E T _ l o a d _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *      Lookup generator ID by its name and load its metadata into the passed object.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	if (item.name == Jrd::QualifiedName(MASTER_GENERATOR, SYSTEM_SCHEMA))
	{
		item.id = 0;
		if (sysGen)
			*sysGen = true;
		if (step)
			*step = 1;
		return true;
	}

	AutoCacheRequest request(tdbb, irq_r_gen_id, IRQ_REQUESTS);

	// Converted FOR loop: Load generator by name and schema
	EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
	EXE_send(tdbb, request.getRequest(), 0, item.name.schema.length(), item.name.schema.c_str());
	EXE_send(tdbb, request.getRequest(), 0, item.name.object.length(), item.name.object.c_str());

	while (EXE_receive(tdbb, request.getRequest(), 1))
	{
		SLONG gen_id;
		char gen_security_class[MAX_SQL_IDENTIFIER_LEN];
		char sch_security_class[MAX_SQL_IDENTIFIER_LEN];
		USHORT system_flag;
		SLONG increment;

		EXE_receive(tdbb, request.getRequest(), 1, sizeof(gen_id), &gen_id);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(gen_security_class), gen_security_class);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(sch_security_class), sch_security_class);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(system_flag), &system_flag);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(increment), &increment);

		item.id = gen_id;
		item.secName = Jrd::QualifiedName(gen_security_class, sch_security_class);

		if (sysGen)
			*sysGen = (system_flag == fb_sysflag_system);

		if (step)
			*step = increment;

		return true;
	}

	return false;
}


SLONG MET_lookup_generator(Jrd::thread_db* tdbb, const Jrd::QualifiedName& name, bool* sysGen, SLONG* step)
{
/**************************************
 *
 *      M E T _ l o o k u p _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *      Lookup generator ID by its name.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	if (name == Jrd::QualifiedName(MASTER_GENERATOR, SYSTEM_SCHEMA))
	{
		if (sysGen)
			*sysGen = true;
		if (step)
			*step = 1;
		return 0;
	}

	AutoCacheRequest request(tdbb, irq_l_gen_id, IRQ_REQUESTS);

	// Converted FOR loop: Lookup generator by name
	EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
	EXE_send(tdbb, request.getRequest(), 0, name.schema.length(), name.schema.c_str());
	EXE_send(tdbb, request.getRequest(), 0, name.object.length(), name.object.c_str());

	while (EXE_receive(tdbb, request.getRequest(), 1))
	{
		USHORT system_flag;
		SLONG increment;
		SLONG gen_id;

		EXE_receive(tdbb, request.getRequest(), 1, sizeof(system_flag), &system_flag);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(increment), &increment);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(gen_id), &gen_id);

		if (sysGen)
			*sysGen = (system_flag == fb_sysflag_system);
		if (step)
			*step = increment;

		return gen_id;
	}

	return -1;
}


bool MET_lookup_generator_id(Jrd::thread_db* tdbb, SLONG gen_id, Jrd::QualifiedName& name, bool* sysGen)
{
/**************************************
 *
 *      M E T _ l o o k u p _ g e n e r a t o r _ i d
 *
 **************************************
 *
 * Functional description
 *      Lookup generator (aka gen_id) by ID. It will load
 *		the name in the third parameter.
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	fb_assert(gen_id != 0);

	name.clear();

	AutoCacheRequest request(tdbb, irq_r_gen_id_num, IRQ_REQUESTS);

	// Converted FOR loop: Lookup generator by ID
	EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
	EXE_send(tdbb, request.getRequest(), 0, sizeof(gen_id), &gen_id);

	while (EXE_receive(tdbb, request.getRequest(), 1))
	{
		char generator_name[MAX_SQL_IDENTIFIER_LEN];
		char schema_name[MAX_SQL_IDENTIFIER_LEN];
		USHORT system_flag;

		EXE_receive(tdbb, request.getRequest(), 1, sizeof(generator_name), generator_name);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(schema_name), schema_name);
		EXE_receive(tdbb, request.getRequest(), 1, sizeof(system_flag), &system_flag);

		if (sysGen)
			*sysGen = (system_flag == fb_sysflag_system);

		name = Jrd::QualifiedName(generator_name, schema_name);
		return true;
	}

	return false;
}


void MET_update_generator_increment(Jrd::thread_db* tdbb, SLONG gen_id, SLONG step)
{
/**************************************
 *
 *      M E T _ u p d a t e _ g e n e r a t o r _ i n c r e m e n t
 *
 **************************************
 *
 * Functional description
 *      Update the step in a generator searched by ID.
 *		This function is for legacy code "SET GENERATOR TO value" only!
 *
 **************************************/
	SET_TDBB(tdbb);
	Attachment* attachment = tdbb->getAttachment();

	AutoCacheRequest request(tdbb, irq_upd_gen_id_increm, IRQ_REQUESTS);

	// Converted FOR/MODIFY loop: Update generator increment by ID
	EXE_start(tdbb, request.getRequest(), attachment->getSysTransaction());
	EXE_send(tdbb, request.getRequest(), 0, sizeof(gen_id), &gen_id);

	while (EXE_receive(tdbb, request.getRequest(), 1))
	{
		USHORT system_flag;

		EXE_receive(tdbb, request.getRequest(), 1, sizeof(system_flag), &system_flag);

		// We never accept changing the step in sys gens.
		if (system_flag == fb_sysflag_system)
			return;

		// Send modified values
		EXE_send(tdbb, request.getRequest(), 2, sizeof(step), &step);
	}
}

} // namespace ScratchBird