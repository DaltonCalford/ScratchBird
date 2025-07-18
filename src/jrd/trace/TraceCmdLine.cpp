/*
 *	MODULE:		TraceCmdLine.cpp
 *	DESCRIPTION:
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
 *  The Original Code was created by Khorsun Vladyslav
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Khorsun Vladyslav <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */

#include "firebird.h"
#include "firebird/impl/consts_pub.h"
#include "sb_exception.h"
#include "iberror.h"
#include "../../common/classes/fb_string.h"
#include "../../common/StatusArg.h"
#include "../../common/utils_proto.h"
#include "../../common/UtilSvc.h"
#include "../common/classes/Switches.h"
#include "../../jrd/trace/traceswi.h"
#include "../../jrd/trace/TraceService.h"
#include "../common/classes/MsgPrint.h"
#include "../common/classes/ClumpletReader.h"
#include "../jrd/license.h"


namespace
{
	using namespace ScratchBird;

	using MsgFormat::SafeArg;
	const USHORT TRACEMGR_MSG_FAC = 25;

	void printMsg(USHORT number, const SafeArg& arg, bool newLine = true)
	{
		char buffer[256];
		fb_msg_format(NULL, TRACEMGR_MSG_FAC, number, sizeof(buffer), buffer, arg);
		if (newLine)
			printf("%s\n", buffer);
		else
			printf("%s", buffer);
	}

	void printMsg(USHORT number, bool newLine = true)
	{
		static const SafeArg dummy;
		printMsg(number, dummy, newLine);
	}

	void usage(UtilSvc* uSvc, const ISC_STATUS code, const char* msg1 = NULL, const char* msg2 = NULL)
	{
		if (uSvc->isService())
		{
			fb_assert(code);
			Arg::Gds gds(code);
			if (msg1)
				gds << msg1;
			if (msg2)
				gds << msg2;
			gds.raise();
		}

		if (code)
		{
			printMsg(2, false); // ERROR:
			USHORT dummy;
			USHORT number = (USHORT) gds__decode(code, &dummy, &dummy);
			fb_assert(number);
			SafeArg safe;
			if (msg1)
				safe << msg1;
			if (msg2)
				safe << msg2;

			printMsg(number, safe);
			printf("\n");
		}

		// If the items aren't contiguous, a scheme like in nbackup.cpp will have to be used.
		// ASF: This is message codes!
		const int MAIN_USAGE[] = {3, 21};
		const int EXAMPLES[] = {22, 27};
		const int NOTES[] = {28, 29};

		for (int i = MAIN_USAGE[0]; i <= MAIN_USAGE[1]; ++i)
			printMsg(i);

		printf("\n");
		for (int i = EXAMPLES[0]; i <= EXAMPLES[1]; ++i)
			printMsg(i);

		printf("\n");
		for (int i = NOTES[0]; i <= NOTES[1]; ++i)
			printMsg(i);

		exit(FINI_ERROR);
	}
}


namespace ScratchBird
{

void fbtrace(UtilSvc* uSvc, TraceSvcIntf* traceSvc)
{
	UtilSvc::ArgvType& argv = uSvc->argv;
	const int argc = argv.getCount();

	bool version = false, help = false;
	// search for "action" switch, set NULL into recognized argv

	const Switches actSwitches(trace_action_in_sw_table, FB_NELEM(trace_action_in_sw_table),
								false, true);

	const Switches::in_sw_tab_t* action_sw = NULL;
	for (int itr = 1; itr < argc; ++itr)
	{
		if (!uSvc->isService() && strcmp(argv[itr], "-?") == 0)
		{
			help = true;
			argv[itr] = NULL;
			break;
		}

		const Switches::in_sw_tab_t* sw = actSwitches.findSwitch(argv[itr]);
		if (sw)
		{
			if (sw->in_sw == IN_SW_TRACE_VERSION)
			{
				version = true;
				argv[itr] = NULL;
				continue;
			}
			if (action_sw)
				usage(uSvc, isc_trace_conflict_acts, action_sw->in_sw_name, sw->in_sw_name);
			else
				action_sw = sw;

			argv[itr] = NULL;
		}
	}

	if (version)
	{
		printMsg(1, SafeArg() << FB_VERSION);
		if (!action_sw)
			exit(FINI_OK);
	}

	if (!action_sw)
	{
		if (help)
			usage(uSvc, 0);
		else
			usage(uSvc, isc_trace_act_notfound);
	}

	// search for action's parameters, set NULL into recognized argv
	const Switches optSwitches(trace_option_in_sw_table, FB_NELEM(trace_option_in_sw_table),
								false, true);
	TraceSession session(*getDefaultMemoryPool());
	for (int itr = 1; itr < argc; ++itr)
	{
		if (!argv[itr])
			continue;

		const Switches::in_sw_tab_t* sw = optSwitches.findSwitch(argv[itr]);
		if (!sw)
			continue;

		argv[itr] = NULL;

		switch (sw->in_sw)
		{
		case IN_SW_TRACE_CONFIG:
			switch (action_sw->in_sw)
			{
				case IN_SW_TRACE_STOP:
				case IN_SW_TRACE_SUSPEND:
				case IN_SW_TRACE_RESUME:
				case IN_SW_TRACE_LIST:
					usage(uSvc, isc_trace_param_act_notcompat, sw->in_sw_name, action_sw->in_sw_name);
					break;
			}

			if (!session.ses_config.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
				session.ses_config = argv[itr];
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_NAME:
			switch (action_sw->in_sw)
			{
				case IN_SW_TRACE_STOP:
				case IN_SW_TRACE_SUSPEND:
				case IN_SW_TRACE_RESUME:
				case IN_SW_TRACE_LIST:
					usage(uSvc, isc_trace_param_act_notcompat, sw->in_sw_name, action_sw->in_sw_name);
					break;
			}

			if (!session.ses_name.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
				session.ses_name = argv[itr];
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_ID:
			switch (action_sw->in_sw)
			{
				case IN_SW_TRACE_START:
				case IN_SW_TRACE_LIST:
					usage(uSvc, isc_trace_param_act_notcompat, sw->in_sw_name, action_sw->in_sw_name);
					break;
			}

			if (session.ses_id)
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
			{
				session.ses_id = atol(argv[itr]);
				if (!session.ses_id)
					usage(uSvc, isc_trace_param_invalid, argv[itr], sw->in_sw_name);
			}
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		default:
			fb_assert(false);
		}
		argv[itr] = NULL;
	}

	// search for authentication parameters
	const Switches authSwitches(trace_auth_in_sw_table, FB_NELEM(trace_auth_in_sw_table),
								false, true);
	string svc_name, user, role, pwd;
	bool trusted = false;
	for (int itr = 1; itr < argc; ++itr)
	{
		if (!argv[itr])
			continue;

		const Switches::in_sw_tab_t* sw = authSwitches.findSwitch(argv[itr]);
		if (!sw) {
			usage(uSvc, isc_trace_switch_unknown, argv[itr]);
		}

		switch (sw->in_sw)
		{
		case IN_SW_TRACE_USERNAME:
			if (!user.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
				user = argv[itr];
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_ROLE:
			if (!role.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
				role = argv[itr];
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_PASSWORD:
			if (!pwd.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
			{
				pwd = argv[itr];
				uSvc->hidePasswd(argv, itr);
			}
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_FETCH_PWD:
			if (uSvc->isService())
				usage(uSvc, isc_trace_switch_user_only, sw->in_sw_name);

			if (!pwd.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
			{
				const PathName fileName(argv[itr]);
				const char *s = NULL;
				switch (fb_utils::fetchPassword(fileName, s))
				{
					case fb_utils::FETCH_PASS_OK:
						pwd = s;
						break;

					case fb_utils::FETCH_PASS_FILE_OPEN_ERROR:
						(Arg::Gds(isc_io_error) << Arg::Str("open") << Arg::Str(fileName) <<
							Arg::Gds(isc_io_open_err) << Arg::OsError()).raise();
						break;

					case fb_utils::FETCH_PASS_FILE_READ_ERROR:
					case fb_utils::FETCH_PASS_FILE_EMPTY:
						(Arg::Gds(isc_io_error) << Arg::Str("read") << Arg::Str(fileName) <<
							Arg::Gds(isc_io_read_err) << Arg::OsError()).raise();
						break;
				}
			}
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		case IN_SW_TRACE_TRUSTED_AUTH:
			if (uSvc->isService())
				usage(uSvc, isc_trace_switch_user_only, sw->in_sw_name);

			trusted = true;
			break;

		case IN_SW_TRACE_SERVICE_NAME:
			if (uSvc->isService())
				continue;

			if (!svc_name.empty())
				usage(uSvc, isc_trace_switch_once, sw->in_sw_name);

			itr++;
			if (itr < argc && argv[itr])
				svc_name = argv[itr];
			else
				usage(uSvc, isc_trace_param_val_miss, sw->in_sw_name);
			break;

		default:
			fb_assert(false);
		}
	}

	// validate missed action's parameters and perform action
	if (!uSvc->isService() && svc_name.isEmpty()) {
		usage(uSvc, isc_trace_mandatory_switch_miss, "SERVICE");
	}

	if (!session.ses_id)
	{
		switch (action_sw->in_sw)
		{
			case IN_SW_TRACE_STOP:
			case IN_SW_TRACE_SUSPEND:
			case IN_SW_TRACE_RESUME:
				usage(uSvc, isc_trace_switch_param_miss, "ID", action_sw->in_sw_name);
				break;
		}
	}

	if (session.ses_config.empty())
	{
		if (action_sw->in_sw == IN_SW_TRACE_START) {
			usage(uSvc, isc_trace_switch_param_miss, "CONFIG", action_sw->in_sw_name);
		}
	}



	traceSvc->setAttachInfo(svc_name, user, role, pwd, trusted);

	switch (action_sw->in_sw)
	{
	case IN_SW_TRACE_START:
		traceSvc->startSession(session, true);
		break;

	case IN_SW_TRACE_STOP:
		traceSvc->stopSession(session.ses_id);
		break;

	case IN_SW_TRACE_SUSPEND:
		traceSvc->setActive(session.ses_id, false);
		break;

	case IN_SW_TRACE_RESUME:
		traceSvc->setActive(session.ses_id, true);
		break;

	case IN_SW_TRACE_LIST:
		traceSvc->listSessions();
		break;

	default:
		fb_assert(false);
	}
}

} // namespace ScratchBird
