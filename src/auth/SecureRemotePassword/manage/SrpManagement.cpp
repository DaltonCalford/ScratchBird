/*
 *	PROGRAM:		ScratchBird authentication
 *	MODULE:			SrpManagement.cpp
 *	DESCRIPTION:	Manages security database with SRP
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
 *  The Original Code was created by Alex Peshkov
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"

#include "../common/classes/ImplementHelper.h"
#include "../common/classes/ClumpletWriter.h"
#include "../common/StatusHolder.h"
#include "firebird/Interface.h"
#include "../auth/SecureRemotePassword/srp.h"
#include "../jrd/constants.h"
#include "firebird/impl/inf_pub.h"
#include "../utilities/gsec/gsec.h"
#include "../auth/SecureRemotePassword/Message.h"
#include "../common/classes/auto.h"
#include "../common/classes/ParsedList.h"

namespace {

const unsigned int SZ_LOGIN = 31;
const unsigned int SZ_NAME = 31;
typedef Field<Varying> Varfield;
typedef Field<ISC_QUAD> Blob;
typedef Field<FB_BOOLEAN> Boolean;

ScratchBird::GlobalPtr<ScratchBird::ConfigKeys> keys;

} // anonymous namespace

namespace Auth {

class SrpManagement final : public ScratchBird::StdPlugin<ScratchBird::IManagementImpl<SrpManagement, ScratchBird::CheckStatusWrapper> >
{
public:
	explicit SrpManagement(ScratchBird::IPluginConfig* par)
		: curAtt(nullptr), mainTra(nullptr), grAdminTra(nullptr)
	{
		ScratchBird::LocalStatus s;
		ScratchBird::CheckStatusWrapper statusWrapper(&s);
		config.assignRefNoIncr(par->getScratchBirdConf(&statusWrapper));
		check(&statusWrapper);
	}

private:
	void prepareDataStructures()
	{
		const char* script[] = {
			"CREATE SCHEMA PLG$SRP DEFAULT CHARACTER SET UTF8",
			"GRANT USAGE ON SCHEMA PLG$SRP TO PUBLIC",

			"CREATE TABLE PLG$SRP.PLG$SRP (PLG$USER_NAME SYSTEM.SEC$USER_NAME NOT NULL PRIMARY KEY, "
			"PLG$VERIFIER VARCHAR(128) CHARACTER SET OCTETS NOT NULL, "
			"PLG$SALT VARCHAR(32) CHARACTER SET OCTETS NOT NULL, "
			"PLG$COMMENT SYSTEM.RDB$DESCRIPTION, PLG$FIRST SYSTEM.SEC$NAME_PART, "
			"PLG$MIDDLE SYSTEM.SEC$NAME_PART, PLG$LAST SYSTEM.SEC$NAME_PART, "
			"PLG$ATTRIBUTES SYSTEM.RDB$DESCRIPTION, "
			"PLG$ACTIVE BOOLEAN)"
			,
			"CREATE VIEW PLG$SRP.PLG$SRP_VIEW AS "
			"SELECT PLG$USER_NAME, PLG$VERIFIER, PLG$SALT, PLG$COMMENT, "
			"   PLG$FIRST, PLG$MIDDLE, PLG$LAST, PLG$ATTRIBUTES, PLG$ACTIVE "
			"FROM PLG$SRP.PLG$SRP WHERE RDB$SYSTEM_PRIVILEGE(USER_MANAGEMENT) "
			"   OR CURRENT_USER = PLG$SRP.PLG$USER_NAME"
			,

			"GRANT ALL ON PLG$SRP.PLG$SRP TO VIEW PLG$SRP.PLG$SRP_VIEW"
			,
			"GRANT SELECT ON PLG$SRP.PLG$SRP_VIEW TO PUBLIC"
			,
			"GRANT UPDATE(PLG$VERIFIER, PLG$SALT, PLG$FIRST, PLG$MIDDLE, PLG$LAST, "
			"   PLG$COMMENT, PLG$ATTRIBUTES) ON PLG$SRP.PLG$SRP_VIEW TO PUBLIC"
			,
			"GRANT ALL ON PLG$SRP.PLG$SRP_VIEW TO SYSTEM PRIVILEGE USER_MANAGEMENT"
			,
			NULL
		};

		ScratchBird::LocalStatus s;
		ScratchBird::CheckStatusWrapper statusWrapper(&s);
		ScratchBird::ITransaction* ddlTran(curAtt->startTransaction(&statusWrapper, 0, NULL));
		check(&statusWrapper);

		try
		{
			for (const char** s = script; *s; ++s)
			{
				const char* sql = *s;
				bool err = false;
				if (sql[0] == '*')
				{
					++sql;
					err = true;
				}

				curAtt->execute(&statusWrapper, ddlTran, 0, sql, SQL_DIALECT_V6, NULL, NULL, NULL, NULL);

				if (!err)
					check(&statusWrapper);
			}

			ddlTran->commit(&statusWrapper);
			check(&statusWrapper);
		}
		catch (const ScratchBird::Exception&)
		{
			if (ddlTran)
			{
				ddlTran->rollback(&statusWrapper);
			}
			throw;
		}
	}

	void prepareName(ScratchBird::string& s, char c)
	{
		for (unsigned i = 0; i < s.length(); ++i)
		{
			if (s[i] == c)
			{
				s.insert(i++, 1, c);
			}
		}
	}

	void grantRevokeAdmin(ScratchBird::IUser* user, bool ignoreRevoke = false)
	{
		if (!user->admin()->entered())
		{
			return;
		}

		ScratchBird::LocalStatus s;
		ScratchBird::CheckStatusWrapper statusWrapper(&s);

		ScratchBird::string userName(user->userName()->get());
		prepareName(userName, '"');

		ScratchBird::string sql;
		if (user->admin()->get() == 0)
		{
			ScratchBird::string userName2(user->userName()->get());
			prepareName(userName2, '\'');
			ScratchBird::string selGrantor;
			selGrantor.printf("SELECT RDB$GRANTOR FROM SYSTEM.RDB$USER_PRIVILEGES "
				"WHERE RDB$USER = '%s' AND RDB$RELATION_NAME = '%s' AND RDB$PRIVILEGE = 'M'",
				userName2.c_str(), ADMIN_ROLE);
			Message out;
			Field<Varying> grantor(out, MAX_SQL_IDENTIFIER_SIZE);
			ScratchBird::IResultSet* curs = curAtt->openCursor(&statusWrapper, grAdminTra, selGrantor.length(),
				selGrantor.c_str(), SQL_DIALECT_V6, NULL, NULL, out.getMetadata(), NULL, 0);
			check(&statusWrapper);

			bool hasGrant = curs->fetchNext(&statusWrapper, out.getBuffer()) == ScratchBird::IStatus::RESULT_OK;
			curs->close(&statusWrapper);
			check(&statusWrapper);

			if (hasGrant)
			{
				selGrantor = grantor;
				prepareName(selGrantor, '"');

				sql.printf("REVOKE %s FROM \"%s\" GRANTED BY \"%s\"",
					ADMIN_ROLE, userName.c_str(), selGrantor.c_str());
			}
			else
			{
				if (ignoreRevoke)
					return;

				// no grant - let engine produce correct error message
				sql.printf("REVOKE %s FROM \"%s\"", ADMIN_ROLE, userName.c_str());
			}
		}
		else
		{
			sql.printf("GRANT DEFAULT %s TO \"%s\"", ADMIN_ROLE, userName.c_str());
		}

		curAtt->execute(&statusWrapper, grAdminTra, sql.length(), sql.c_str(),
			SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
		check(&statusWrapper);
	}

	bool getUid(ScratchBird::CheckStatusWrapper* status, ScratchBird::IAttachment* att, ScratchBird::UCharBuffer& uid)
	{
		UCHAR item = fb_info_db_file_id;
		UCHAR outbuf[256];
		att->getInfo(status, 1, &item, sizeof outbuf, outbuf);
		check(status);

		const UCHAR* const end = &outbuf[sizeof outbuf];
		for (const UCHAR* d = outbuf; *d != isc_info_end && d + 3 < end;)
		{
			item = *d++;
			const int length = gds__vax_integer(d, 2);
			d += 2;
			if (d + length > end)
				break;

			if (item == fb_info_db_file_id)
			{
				uid.assign(d, length);
				return true;
			}
		}

		return false;
	}


public:
	// IManagement implementation
	void start(ScratchBird::CheckStatusWrapper* status, ScratchBird::ILogonInfo* logonInfo)
	{
		status->init();

		try
		{
			if (curAtt)
			{
				(ScratchBird::Arg::Gds(isc_random) << "Database is already attached in SRP user management").raise();
			}

			unsigned int secDbKey = keys->getKey(config, "SecurityDatabase");
			const char* secDbName = config->asString(secDbKey);
			if (!(secDbName && secDbName[0]))
			{
				ScratchBird::Arg::Gds(isc_secdb_name).raise();
			}

			ScratchBird::ClumpletWriter dpb(ScratchBird::ClumpletReader::dpbList, MAX_DPB_SIZE);
			dpb.insertByte(isc_dpb_sec_attach, TRUE);
			dpb.insertString(isc_dpb_config, ScratchBird::ParsedList::getNonLoopbackProviders(secDbName));

			unsigned int authBlockSize;
			const unsigned char* authBlock = logonInfo->authBlock(&authBlockSize);

			const char* str = logonInfo->role();
			if (str && str[0])
				dpb.insertString(isc_dpb_sql_role_name, str, fb_strlen(str));

			if (authBlockSize)
			{
#if SRP_DEBUG > 0
				fprintf(stderr, "SrpManagement: Using authBlock size %d\n", authBlockSize);
#endif
				dpb.insertBytes(isc_dpb_auth_block, authBlock, authBlockSize);
			}
			else
			{
				str = logonInfo->name();
#if SRP_DEBUG > 0
				fprintf(stderr, "SrpManagement: Using name '%s'\n", str);
#endif
				if (str && str[0])
					dpb.insertString(isc_dpb_trusted_auth, str, fb_strlen(str));
			}

			ScratchBird::DispatcherPtr p;
			curAtt = p->attachDatabase(status, secDbName, dpb.getBufferLength(), dpb.getBuffer());
			check(status);
			ownAtt.reset(curAtt);

			// Check: is passed attachment OK for us?

			// ID of requested sec.db
			ScratchBird::UCharBuffer reqId;
			if (getUid(status, curAtt, reqId))
			{
				ScratchBird::IAttachment* att = logonInfo->attachment(status);

				// ID of passed attachment
				ScratchBird::UCharBuffer actualId;
				if (att && getUid(status, att, actualId))
				{
					if (actualId == reqId)
					{
						ownAtt.reset(nullptr);	// close own attachment
						curAtt = att;
						grAdminTra = mainTra = logonInfo->transaction(status);
						check(status);

#if SRP_DEBUG > 0
						fprintf(stderr, "SrpManagement: Use att from logon info\n");
#endif

						return;
					}
				}
			}
			status->init();

			grAdminTra = mainTra = curAtt->startTransaction(status, 0, NULL);
			check(status);
			ownTra.reset(mainTra);

#if SRP_DEBUG > 0
			fprintf(stderr, "SrpManagement: Use own att\n");
#endif
		}
		catch (const ScratchBird::Exception& ex)
		{
			ownTra.reset(nullptr);
			ownAtt.reset(nullptr);
			ex.stuffException(status);
		}
	}

	int execute(ScratchBird::CheckStatusWrapper* status, ScratchBird::IUser* user, ScratchBird::IListUsers* callback)
	{
		try
		{
			status->init();

			fb_assert(curAtt);
			fb_assert(mainTra);
			fb_assert(grAdminTra);

			curAtt->execute(status, nullptr, 0, "SET BIND OF BOOLEAN TO NATIVE", SQL_DIALECT_V6, NULL, NULL, NULL, NULL);

			switch(user->operation())
			{
			case ScratchBird::IUser::OP_USER_DROP_MAP:
			case ScratchBird::IUser::OP_USER_SET_MAP:
				{
					ScratchBird::string sql;
					sql.printf("ALTER ROLE " ADMIN_ROLE " %s AUTO ADMIN MAPPING",
						user->operation() == ScratchBird::IUser::OP_USER_SET_MAP ? "SET" : "DROP");
					curAtt->execute(status, grAdminTra, sql.length(), sql.c_str(), SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
					check(status);
				}
				break;

			case ScratchBird::IUser::OP_USER_ADD:
				{
					const char* insert =
						"INSERT INTO plg$srp.plg$srp_view(PLG$USER_NAME, PLG$VERIFIER, PLG$SALT, PLG$FIRST, PLG$MIDDLE, PLG$LAST,"
						"PLG$COMMENT, PLG$ATTRIBUTES, PLG$ACTIVE) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)";

					ScratchBird::IStatement* stmt = NULL;
					try
					{
						for (unsigned repeat = 0; ; ++repeat)
						{
							stmt = curAtt->prepare(status, mainTra, 0, insert, SQL_DIALECT_V6, ScratchBird::IStatement::PREPARE_PREFETCH_METADATA);
							if (!(status->getState() & ScratchBird::IStatus::STATE_ERRORS))
							{
								break;
							}
							else if (repeat > 0)
							{
								ScratchBird::status_exception::raise(status);
							}

							if (fb_utils::containsErrorCode(status->getErrors(), isc_dsql_relation_err))
							{
								prepareDataStructures();
								if (ownAtt)
								{
									mainTra->commit(status);
									check(status);
									ownTra.release();
								}

								mainTra = curAtt->startTransaction(status, 0, NULL);
								check(status);
								ownTra.reset(mainTra);

								if (ownAtt)
								{
									grAdminTra = mainTra;
								}
#if SRP_DEBUG > 0
								else
									fprintf(stderr, "SrpManagement: switch to own tra\n");
#endif
							}
						}

						fb_assert(stmt);

						Meta im(stmt, false);
						Message add(im);
						Varfield login(add);
						Varfield verifier(add), slt(add);
						Varfield first(add), middle(add), last(add);
						Blob comment(add), attr(add);
						Boolean active(add);

						setField(login, user->userName());
						setField(first, user->firstName());
						setField(middle, user->middleName());
						setField(last, user->lastName());
						setField(status, comment, user->comment());
						setField(status, attr, user->attributes());
						setField(active, user->active());

#if SRP_DEBUG > 1
						ScratchBird::BigInteger salt("02E268803000000079A478A700000002D1A6979000000026E1601C000000054F");
#else
						ScratchBird::BigInteger salt;
						salt.random(RemotePassword::SRP_SALT_SIZE);
#endif
						ScratchBird::UCharBuffer s;
						salt.getBytes(s);
						slt.set(s.getCount(), s.begin());

						dumpIt("salt", s);
#if SRP_DEBUG > 0
						fprintf(stderr, ">%s< >%s<\n", user->userName()->get(), user->password()->get());
#endif
						ScratchBird::string s1;
						salt.getText(s1);
						server.computeVerifier(user->userName()->get(), s1, user->password()->get()).getBytes(s);
						dumpIt("verifier", s);
						verifier.set(s.getCount(), s.begin());

						stmt->execute(status, mainTra, add.getMetadata(), add.getBuffer(), NULL, NULL);
						check(status);

						stmt->free(status);
						check(status);

						grantRevokeAdmin(user);
					}
					catch (const ScratchBird::Exception&)
					{
						if (stmt)
						{
							stmt->release();
						}
						throw;
					}
				}
				break;

			case ScratchBird::IUser::OP_USER_MODIFY:
				{
					ScratchBird::string update = "UPDATE plg$srp.plg$srp_view SET ";

					ScratchBird::AutoPtr<Varfield> verifier, slt;
					if (user->password()->entered())
					{
						update += "PLG$VERIFIER=?,PLG$SALT=?,";
					}

					ScratchBird::AutoPtr<Varfield> first, middle, last;
					ScratchBird::AutoPtr<Blob> comment, attr;
					ScratchBird::AutoPtr<Boolean> active;
					allocField(user->firstName(), update, "PLG$FIRST");
					allocField(user->middleName(), update, "PLG$MIDDLE");
					allocField(user->lastName(), update, "PLG$LAST");
					allocField(user->comment(), update, "PLG$COMMENT");
					allocField(user->attributes(), update, "PLG$ATTRIBUTES");
					allocField(user->active(), update, "PLG$ACTIVE");

					if (update[update.length() - 1] != ',')
					{
						grantRevokeAdmin(user);
						return 0;
					}
					update.rtrim(",");
					update += " WHERE PLG$USER_NAME=?";

					ScratchBird::IStatement* stmt = NULL;
					try
					{
						stmt = curAtt->prepare(status, mainTra, 0, update.c_str(), SQL_DIALECT_V6, ScratchBird::IStatement::PREPARE_PREFETCH_METADATA);
						check(status);

						Meta im(stmt, false);
						Message up(im);

						if (user->password()->entered())
						{
							verifier = FB_NEW Varfield(up);
							slt = FB_NEW Varfield(up);
#if SRP_DEBUG > 1
							ScratchBird::BigInteger salt("02E268803000000079A478A700000002D1A6979000000026E1601C000000054F");
#else
							ScratchBird::BigInteger salt;
							salt.random(RemotePassword::SRP_SALT_SIZE);
#endif
							ScratchBird::UCharBuffer s;
							salt.getBytes(s);
							slt->set(s.getCount(), s.begin());

							dumpIt("salt", s);
#if SRP_DEBUG > 0
							fprintf(stderr, ">%s< >%s<\n", user->userName()->get(), user->password()->get());
#endif
							ScratchBird::string s1;
							salt.getText(s1);
							server.computeVerifier(user->userName()->get(), s1, user->password()->get()).getBytes(s);
							dumpIt("verifier", s);
							verifier->set(s.getCount(), s.begin());
						}

						allocField(first, up, user->firstName());
						allocField(middle, up, user->middleName());
						allocField(last, up, user->lastName());
						allocField(comment, up, user->comment());
						allocField(attr, up, user->attributes());
						allocField(active, up, user->active());

						Varfield login(up);

						assignField(first, user->firstName());
						assignField(middle, user->middleName());
						assignField(last, user->lastName());
						assignField(status, comment, user->comment());
						assignField(status, attr, user->attributes());
						assignField(active, user->active());
						setField(login, user->userName());

						stmt->execute(status, mainTra, up.getMetadata(), up.getBuffer(), NULL, NULL);
						check(status);
						if (recordsCount(status, stmt, isc_info_req_update_count) != 1)
						{
							stmt->release();
							return GsecMsg22;
						}

						stmt->free(status);
						check(status);

						grantRevokeAdmin(user);
					}
					catch (const ScratchBird::Exception&)
					{
						if (stmt)
						{
							stmt->release();
						}
						throw;
					}
				}
				break;

			case ScratchBird::IUser::OP_USER_DELETE:
				{
					const char* del = "DELETE FROM plg$srp.plg$srp_view WHERE PLG$USER_NAME=?";
					ScratchBird::IStatement* stmt = NULL;
					try
					{
						stmt = curAtt->prepare(status, mainTra, 0, del, SQL_DIALECT_V6, ScratchBird::IStatement::PREPARE_PREFETCH_METADATA);
						check(status);

						Meta im(stmt, false);
						Message dl(im);
						Varfield login(dl);
						setField(login, user->userName());

						stmt->execute(status, mainTra, dl.getMetadata(), dl.getBuffer(), NULL, NULL);
						check(status);
						if (recordsCount(status, stmt, isc_info_req_delete_count) != 1)
						{
							stmt->release();
							return GsecMsg22;
						}

						stmt->free(status);
						check(status);

						user->admin()->set(status, 0);
						check(status);
						user->admin()->setEntered(status, 1);
						check(status);
						grantRevokeAdmin(user, true);
					}
					catch (const ScratchBird::Exception&)
					{
						if (stmt)
						{
							stmt->release();
						}
						throw;
					}
				}
				break;

			case ScratchBird::IUser::OP_USER_DISPLAY:
				{
					ScratchBird::string disp =
						"WITH ADMINS AS (SELECT RDB$USER FROM SYSTEM.RDB$USER_PRIVILEGES "
						"	WHERE RDB$RELATION_NAME = 'RDB$ADMIN' AND RDB$PRIVILEGE = 'M' GROUP BY RDB$USER) "
						"SELECT PLG$USER_NAME, PLG$FIRST, PLG$MIDDLE, PLG$LAST, PLG$COMMENT, PLG$ATTRIBUTES, "
						"	CASE WHEN RDB$USER IS NULL THEN FALSE ELSE TRUE END, PLG$ACTIVE "
						"FROM PLG$SRP%SCHEMA.PLG$SRP_VIEW LEFT JOIN ADMINS "
						"	ON PLG$SRP_VIEW.PLG$USER_NAME = ADMINS.RDB$USER ";
					if (user->userName()->entered())
					{
						disp += " WHERE PLG$USER_NAME = ?";
					}

					ScratchBird::IStatement* stmt = NULL;
					ScratchBird::IResultSet* rs = NULL;
					try
					{
						stmt = curAtt->prepare(status, mainTra, 0, disp.c_str(), SQL_DIALECT_V6,
							ScratchBird::IStatement::PREPARE_PREFETCH_METADATA);
						check(status);

						Meta om(stmt, true);
						Message di(om);
						Varfield login(di);
						Varfield first(di), middle(di), last(di);
						Blob comment(di), attr(di);
						Boolean admin(di), active(di);

						Message* par = NULL;
						Meta im(stmt, false);
						Message tm(im);
						if (user->userName()->entered())
						{
							par = &tm;
							Varfield login(*par);
							setField(login, user->userName());
						}

						rs = stmt->openCursor(status, mainTra, (par ? par->getMetadata() : NULL),
							(par ? par->getBuffer() : NULL), om, 0);
						check(status);

						while (rs->fetchNext(status, di.getBuffer()) == ScratchBird::IStatus::RESULT_OK)
						{
							listField(user->userName(), login);
							listField(user->firstName(), first);
							listField(user->middleName(), middle);
							listField(user->lastName(), last);
							listField(user->comment(), comment);
							listField(user->attributes(), attr);
							listField(user->active(), active);
							listField(user->admin(), admin);

							callback->list(status, user);
							check(status);
						}
						check(status);

						rs->close(status);
						check(status);

						stmt->free(status);
						check(status);
					}
					catch (const ScratchBird::Exception&)
					{
						if (stmt)
						{
							stmt->release();
						}
						throw;
					}
				}
				break;

			default:
				return -1;
			}
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(status);
			return -1;
		}

		return 0;
	}

	void commit(ScratchBird::CheckStatusWrapper* status)
	{
		if (ownTra)
		{
			ownTra->commit(status);
			if (!(status->getState() & ScratchBird::IStatus::STATE_ERRORS))
			{
				ownTra.release();
			}
		}
	}

	void rollback(ScratchBird::CheckStatusWrapper* status)
	{
		if (ownTra)
		{
			ownTra->rollback(status);
			if (!(status->getState() & ScratchBird::IStatus::STATE_ERRORS))
			{
				ownTra.release();
			}
		}
	}

private:
	ScratchBird::RefPtr<ScratchBird::IScratchBirdConf> config;

	// attachment(s)
	ScratchBird::AutoRelease<ScratchBird::IAttachment> ownAtt;
	ScratchBird::IAttachment* curAtt;

	// transactions
	ScratchBird::AutoRelease<ScratchBird::ITransaction> ownTra;
	ScratchBird::ITransaction* mainTra;
	ScratchBird::ITransaction* grAdminTra;

	RemotePasswordImpl<ScratchBird::Sha1> server;

	int recordsCount(ScratchBird::CheckStatusWrapper* status, ScratchBird::IStatement* stmt, UCHAR item)
	{
		UCHAR buffer[33];
		const UCHAR count_info[] = { isc_info_sql_records };
		stmt->getInfo(status, sizeof(count_info), count_info, sizeof(buffer), buffer);
		check(status);

		if (buffer[0] == isc_info_sql_records)
		{
			const UCHAR* p = buffer + 3;
			while (*p != isc_info_end)
			{
				const UCHAR count_is = *p++;
				const SSHORT len = gds__vax_integer(p, 2);
				p += 2;
				if (count_is == item)
					return gds__vax_integer(p, len);
				p += len;
			}
		}

		return 0;
	}

	static void check(ScratchBird::CheckStatusWrapper* status)
	{
		if (status->getState() & ScratchBird::IStatus::STATE_ERRORS)
		{
			checkStatusVectorForMissingTable(status->getErrors());
			ScratchBird::status_exception::raise(status);
		}
	}

	static void setField(Varfield& to, ScratchBird::ICharUserField* from)
	{
		if (from->entered())
		{
			to = from->get();
		}
		else
		{
			to.null = FB_TRUE;
		}
	}

	static void setField(Boolean& to, ScratchBird::IIntUserField* from)
	{
		if (from->entered())
		{
			to = from->get() ? FB_TRUE : FB_FALSE;
		}
		else
		{
			to.null = FB_TRUE;
		}
	}

	void setField(ScratchBird::CheckStatusWrapper* st, Blob& to, ScratchBird::ICharUserField* from)
	{
		if (from->entered())
		{
			blobWrite(st, to, from);
		}
		else
		{
			to.null = FB_TRUE;
		}
	}

	static void allocField(ScratchBird::IUserField* value, ScratchBird::string& update, const char* name)
	{
		if (value->entered() || value->specified())
		{
			update += ' ';
			update += name;
			update += "=?,";
		}
	}

	template <typename FT>
	static void allocField(ScratchBird::AutoPtr<FT>& field, Message& up, ScratchBird::IUserField* value)
	{
		if (value->entered() || value->specified())
		{
			field = FB_NEW FT(up);
		}
	}

	static void assignField(ScratchBird::AutoPtr<Varfield>& field, ScratchBird::ICharUserField* name)
	{
		if (field.hasData())
		{
			if (name->entered())
			{
				*field = name->get();
			}
			else
			{
				fb_assert(name->specified());
				field->null = FB_TRUE;
			}
		}
	}

	static void assignField(ScratchBird::AutoPtr<Boolean>& field, ScratchBird::IIntUserField* name)
	{
		if (field.hasData())
		{
			if (name->entered())
			{
				*field = name->get() ? FB_TRUE : FB_FALSE;
			}
			else
			{
				fb_assert(name->specified());
				field->null = FB_TRUE;
			}
		}
	}

	void assignField(ScratchBird::CheckStatusWrapper* st, ScratchBird::AutoPtr<Blob>& field, ScratchBird::ICharUserField* name)
	{
		if (field.hasData())
		{
			if (name->entered())
			{
				blobWrite(st, *field, name);
				field->null = FB_FALSE;
			}
			else
			{
				fb_assert(name->specified());
				field->null = FB_TRUE;
			}
		}
	}

	static void listField(ScratchBird::ICharUserField* to, Varfield& from)
	{
		ScratchBird::LocalStatus st;
		ScratchBird::CheckStatusWrapper statusWrapper(&st);
		to->setEntered(&statusWrapper, from.null ? 0 : 1);
		check(&statusWrapper);
		if (!from.null)
		{
			to->set(&statusWrapper, from);
			check(&statusWrapper);
		}
	}

	static void listField(ScratchBird::IIntUserField* to, Boolean& from)
	{
		ScratchBird::LocalStatus st;
		ScratchBird::CheckStatusWrapper statusWrapper(&st);
		to->setEntered(&statusWrapper, from.null ? 0 : 1);
		check(&statusWrapper);
		if (!from.null)
		{
			to->set(&statusWrapper, from);
			check(&statusWrapper);
		}
	}

	void listField(ScratchBird::ICharUserField* to, Blob& from)
	{
		ScratchBird::LocalStatus st;
		ScratchBird::CheckStatusWrapper statusWrapper(&st);
		to->setEntered(&statusWrapper, from.null ? 0 : 1);
		check(&statusWrapper);
		if (!from.null)
		{
			ScratchBird::string s;
			ScratchBird::IBlob* blob = NULL;
			try
			{
				blob = curAtt->openBlob(&statusWrapper, mainTra, &from, 0, NULL);
				check(&statusWrapper);

				char segbuf[256];
				unsigned len;
				for (;;)
				{
					int cc = blob->getSegment(&statusWrapper, sizeof(segbuf), segbuf, &len);
					check(&statusWrapper);
					if (cc == ScratchBird::IStatus::RESULT_NO_DATA)
						break;
					s.append(segbuf, len);
				}

				blob->close(&statusWrapper);
				check(&statusWrapper);

				to->set(&statusWrapper, s.c_str());
				check(&statusWrapper);
			}
			catch (const ScratchBird::Exception&)
			{
				if (blob)
					blob->release();
				throw;
			}
		}
	}

	void blobWrite(ScratchBird::CheckStatusWrapper* st, Blob& to, ScratchBird::ICharUserField* from)
	{
		to.null = FB_FALSE;
		const char* ptr = from->get();
		unsigned len = static_cast<unsigned>(strlen(ptr));

		ScratchBird::IBlob* blob = NULL;
		try
		{
			blob = curAtt->createBlob(st, mainTra, &to, 0, NULL);
			check(st);

			while (len)
			{
				unsigned seg = len > MAX_USHORT ? MAX_USHORT : len;
				blob->putSegment(st, seg, ptr);
				check(st);
				len -= seg;
				ptr += seg;
			}

			blob->close(st);
			check(st);
		}
		catch (const ScratchBird::Exception&)
		{
			if (blob)
				blob->release();
			throw;
		}
	}
};

// register plugin
static ScratchBird::SimpleFactory<Auth::SrpManagement> factory;

} // namespace Auth

extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(ScratchBird::IMaster* master)
{
	ScratchBird::CachedMasterInterface::set(master);
	ScratchBird::PluginManagerInterfacePtr()->registerPluginFactory(ScratchBird::IPluginManager::TYPE_AUTH_USER_MANAGEMENT, Auth::RemotePassword::plugName, &Auth::factory);
	ScratchBird::getUnloadDetector()->registerMe();
}
