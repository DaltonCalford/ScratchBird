#coding:utf-8

"""
ID:          issue-8185
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8185
TITLE:       SIGSEGV in Firebird 5.0.0.1306 embedded during update on cursor
DESCRIPTION:
    Test implements sequence of actions described by Dimitry Sibiryakov in the ticket,
    see: https://github.com/FirebirdSQL/firebird/issues/8185#issuecomment-2258598579
NOTES:
    [01.11.2024] pzotov
    1. Bug was fixed on following commits:
        5.x: 27.07.2024 11:48, 08dc25f8c45342a73c786bc60571c8a5f2c8c6e3
        ("Simplest fix for #8185: SIGSEGV in Firebird 5.0.0.1306 embedded during update on cursor - disallow caching for positioned updates/deletes")
        6.x: 29.07.2024 00:53, a7d10a40147d326e56540498b50e40b2da0e5850
        ("Fix #8185 - SIGSEGV with WHERE CURRENT OF statement with statement cache turned on")
    2. In current version of firebird-driver we can *not* set cursor name without executing it first.
       But such execution leads to 'update conflict / deadlock' for subsequent UPDATE statement.
       Kind of 'hack' is used to solve this problem: ps1._istmt.set_cursor_name(CURSOR_NAME)
    3. GREAT thanks to:
        * Vlad for providing workaround and explanation of problem with AV for code like this:
             with connect(f'localhost:{DB_NAME}', user = DBA_USER, password = DBA_PSWD) as con:
                 cur1 = con.cursor()
                 ps1 = cur1.prepare('update test set id = -id rows 0 returning id')
                 cur1.execute(ps1)
                 ps1.free()
          It is mandatory to store result of cur1.eecute in some variable, i.e. rs1 = cur1.execute(ps1),
          and call then rs1.close() __BEFORE__ ps1.free().
          Discussed 26.10.2024, subj:
          "Oddities when using instance of selectable Statement // related to interfaces, VTable, iResultSet, iVersioned , CLOOP"
        * Dimitry Sibiryakov for describe the 'step-by-step' algorithm for reproducing problem and providing working example in .cpp

    Confirmed problem on 5.0.1.1452-b056f5b (last snapshot before it was fixed).
    Checked on 5.0.1.1452-08dc25f (27.07.2024 11:50); 6.0.0.401-a7d10a4 (29.07.2024 01:33) -- all OK.
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, tpb, TraAccessMode, Isolation, DatabaseError

init_sql = """
    set bail on;
    recreate table test(id int, f01 int);
    commit;
    insert into test(id, f01) select row_number()over(), row_number()over() * 10 from rdb$types rows 3;
    commit;
"""
db = db_factory(init = init_sql)
act = python_act('db')

@pytest.mark.version('>=5.0.1')
def test_1(act: Action, capsys):

    srv_cfg = driver_config.register_server(name = 'test_srv_gh_8185', config = '')
    db_cfg_name = f'db_cfg_8185'
    db_cfg_object = driver_config.register_database(name = db_cfg_name)
    db_cfg_object.server.value = srv_cfg.name
    db_cfg_object.database.value = str(act.db.db_path)
    db_cfg_object.config.value = f"""
        MaxStatementCacheSize = 1M
    """

    # Pre-check:
    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:
        cur = con.cursor()
        cur.execute("select a.mon$remote_protocol, g.rdb$config_value from mon$attachments a left join rdb$config g on g.rdb$config_name = 'MaxStatementCacheSize' where a.mon$attachment_id = current_connection")
        for r in cur:
            conn_protocol = r[0]
            db_sttm_cache_size = int(r[1])
        assert conn_protocol is None, "Test must use LOCAL protocol."
        assert db_sttm_cache_size > 0, "Parameter 'MaxStatementCacheSize' (per-database) must be greater than zero for this test."

    #---------------------------------------------

    CURSOR_NAME = 'k1'
    SELECT_STTM = 'select /* ps-1*/ id, f01 from test where id > 0 for update'
    UPDATE_STTM = f'update /* ps-2 */ test set id = -id where current of {CURSOR_NAME} returning id'

    update_tpb = tpb( access_mode = TraAccessMode.WRITE,
                      isolation = Isolation.READ_COMMITTED_RECORD_VERSION,
                      lock_timeout = 1)

    with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:

        tx2 = con.transaction_manager(update_tpb)
        tx2.begin()

        with con.cursor() as cur1, tx2.cursor() as cur2, con.cursor() as cur3:

            ps1, rs1, ps2, rs2, ps3, rs3 = None, None, None, None, None, None
            try:
                ps1 = cur1.prepare(SELECT_STTM)         # 1. [ticket, DS] Prepare statement 1 "select ... for update"
                ps1._istmt.set_cursor_name(CURSOR_NAME) # 2. [ticket, DS] Set cursor name for statement 1 // ~hack.

                # DO NOT use it because subsequent update statement will get 'deadlock /  update conflict' and not able to start:
                #rs1 = cur1.execute(ps1)
                #cur1.set_cursor_name(CURSOR_NAME)

                # DS example: "// Prepare positioned update statement"
                ps2 = cur2.prepare(UPDATE_STTM)         # 3. [ticket, DS] Prepare statement 2 "update ... where current of <cursor name from step 2>"

                # DS .cpp: // fetch records from cursor and print them
                rs1 = cur1.execute(ps1)
                rs1.fetchall()
                
                # DS .cpp: // IStatement* stmt2 = att->prepare(&status, tra, 0, "select * from pos where a > 1 for update",
                ps3 = cur3.prepare(SELECT_STTM)         # 4. [ticket, DS] Prepare statement 3 similar to statement 1

                rs1.close()                             # 5. [ticket, DS] Release statement 1 // see hvlad recipe, 26.10.2024
                ps1.free()

                # DS .cpp: updStmt->free(&status);
                ps2.free()                              # 6. [ticket, DS] Release statement 2 // see hvlad recipe, 26.10.2024

                # DS .cpp: stmt = stmt2
                ps3._istmt.set_cursor_name(CURSOR_NAME) # 7. [ticket, DS] Set cursor name to statement 3 as in step 2

                ps2 = cur2.prepare(UPDATE_STTM)         # 8. [ticket, DS] Prepare statement 2 again (it will be got from cache keeping reference to statement 1)

                rs3 = cur3.execute(ps3)
                rs3.fetchone()                          # 9. [ticket, DS] Run statement 3 and fetch one record

                # At step 10 you can get "Invalid handle" error or a crash if you swap steps 5 and 6.
                rs2 = cur2.execute(ps2)                 # 10. [ticket, DS] Execute statement 2
                data2 = rs2.fetchone()
                print('Changed ID:', data2[0])
                # print(f'{rs2.rowcount=}')

            except DatabaseError as e:
                print(e.__str__())
                print('gds codes:')
                for i in e.gds_codes:
                    print(i)
            
            finally:
                if rs1:
                    rs1.close()
                if ps1:
                    ps1.free()

                if rs2:
                    rs2.close()
                if ps2:
                    ps2.free()

                if rs3:
                    rs3.close()
                if ps3:
                    ps3.free()

    #---------------------------------------------

    act.expected_stdout = 'Changed ID: -1'
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout


# Example in .cpp (provided by Dimitry Sibiryakov):
###################################################
#
# #include <memory>
# #include "ifaceExamples.h"
# static IMaster* master = fb_get_master_interface();
#
# int main()
# {
# 	int rc = 0;
#
# 	// status vector and main dispatcher
# 	ThrowStatusWrapper status(master->getStatus());
# 	IProvider* prov = master->getDispatcher();
# 	IUtil* utl = master->getUtilInterface();
#
# 	// declare pointers to required interfaces
# 	IAttachment* att = NULL;
# 	ITransaction* tra = NULL;
# 	IStatement* stmt = NULL;
# 	IMessageMetadata* meta = NULL;
# 	IMetadataBuilder* builder = NULL;
# 	IXpbBuilder* tpb = NULL;
#
# 	// Interface provides access to data returned by SELECT statement
# 	IResultSet* curs = NULL;
#
# 	try
# 	{
# 		// IXpbBuilder is used to access various parameters blocks used in API
# 		IXpbBuilder* dpb = NULL;
#
# 		// create DPB - use non-default page size 4Kb
# 		dpb = utl->getXpbBuilder(&status, IXpbBuilder::DPB, NULL, 0);
# 		dpb->insertString(&status, isc_dpb_user_name, "sysdba");
# 		dpb->insertString(&status, isc_dpb_password, "masterkey");
#
# 		// create empty database
# 		att = prov->attachDatabase(&status, "ctest", dpb->getBufferLength(&status),
# 			dpb->getBuffer(&status));
#
# 		dpb->dispose();
#
# 		printf("database attached.\n");
#
# 		att->execute(&status, nullptr, 0, "set debug option dsql_keep_blr = true", SAMPLES_DIALECT, nullptr, nullptr, nullptr, nullptr);
# 		// start read only transaction
# 		tpb = utl->getXpbBuilder(&status, IXpbBuilder::TPB, NULL, 0);
# 		tpb->insertTag(&status, isc_tpb_read_committed);
# 		tpb->insertTag(&status, isc_tpb_no_rec_version);
# 		tpb->insertTag(&status, isc_tpb_wait);
# 		tra = att->startTransaction(&status, tpb->getBufferLength(&status), tpb->getBuffer(&status));
#
# 		// prepare statement
# 		stmt = att->prepare(&status, tra, 0, "select * from pos where a > 1 for update",
# 			SAMPLES_DIALECT, IStatement::PREPARE_PREFETCH_METADATA);
#
# 		// get list of columns
# 		meta = stmt->getOutputMetadata(&status);
# 		unsigned cols = meta->getCount(&status);
# 		unsigned messageLength = meta->getMessageLength(&status);
#
# 		std::unique_ptr<char[]> buffer(new char[messageLength]);
#
# 		stmt->setCursorName(&status, "abc");
#
# 		// open cursor
# 		printf("Opening cursor...\n");
# 		curs = stmt->openCursor(&status, tra, NULL, NULL, meta, 0);
#
# 		// Prepare positioned update statement
# 		printf("Preparing update statement...\n");
# 		IStatement* updStmt = att->prepare(&status, tra, 0, "update pos set b=b+1 where current of abc",
# 			SAMPLES_DIALECT, 0);
#
# 		const unsigned char items[] = {isc_info_sql_exec_path_blr_text, isc_info_sql_explain_plan};
# 		unsigned char infoBuffer[32000];
# 		updStmt->getInfo(&status, sizeof items, items, sizeof infoBuffer, infoBuffer);
#
# 		IXpbBuilder* pb = utl->getXpbBuilder(&status, IXpbBuilder::INFO_RESPONSE, infoBuffer, sizeof infoBuffer);
# 		for (pb->rewind(&status); !pb->isEof(&status); pb->moveNext(&status))
# 		{
# 			switch (pb->getTag(&status))
# 			{
# 			case isc_info_sql_exec_path_blr_text:
# 				printf("BLR:\n%s\n", pb->getString(&status));
# 				break;
# 			case isc_info_sql_explain_plan:
# 				printf("Plan:\n%s\n", pb->getString(&status));
# 				break;
# 			case isc_info_truncated:
# 				printf("  truncated\n");
# 				// fall down...
# 			case isc_info_end:
# 				break;
# 			default:
# 				printf("Unexpected item %d\n", pb->getTag(&status));
# 			}
# 		}
# 		pb->dispose();
#
# 		// fetch records from cursor and print them
# 		for (int line = 0; curs->fetchNext(&status, buffer.get()) == IStatus::RESULT_OK; ++line)
# 		{
# 			printf("Fetched record %d\n", line);
# 			updStmt->execute(&status, tra, nullptr, nullptr, nullptr, nullptr);
# 			printf("Update executed\n");
# 		}
#
# 		IStatement* stmt2 = att->prepare(&status, tra, 0, "select * from pos where a > 1 for update",
# 			SAMPLES_DIALECT, IStatement::PREPARE_PREFETCH_METADATA);
#
# 		// close interfaces
# 		curs->close(&status);
# 		curs = NULL;
#
# 		stmt->free(&status);
# 		stmt = NULL;
#
# 		updStmt->free(&status);
#
# 		stmt = stmt2;
# 		stmt->setCursorName(&status, "abc");
#
# 		// open cursor
# 		printf("Opening cursor2...\n");
# 		curs = stmt->openCursor(&status, tra, NULL, NULL, meta, 0);
#
# 		// Prepare positioned update statement
# 		printf("Preparing update statement again...\n");
# 		updStmt = att->prepare(&status, tra, 0, "update pos set b=b+1 where current of abc",
# 			SAMPLES_DIALECT, 0);
#
# 		// fetch records from cursor and print them
# 		for (int line = 0; curs->fetchNext(&status, buffer.get()) == IStatus::RESULT_OK; ++line)
# 		{
# 			printf("Fetched record %d\n", line);
# 			updStmt->execute(&status, tra, nullptr, nullptr, nullptr, nullptr);
# 			printf("Update executed\n");
# 		}
#
# 		curs->close(&status);
# 		curs = NULL;
#
# 		stmt->free(&status);
# 		stmt = NULL;
#
# 		updStmt->free(&status);
#
# 		meta->release();
# 		meta = NULL;
#
# 		tra->commit(&status);
# 		tra = NULL;
#
# 		att->detach(&status);
# 		att = NULL;
# 	}
# 	catch (const FbException& error)
# 	{
# 		// handle error
# 		rc = 1;
#
# 		char buf[256];
# 		master->getUtilInterface()->formatStatus(buf, sizeof(buf), error.getStatus());
# 		fprintf(stderr, "%s\n", buf);
# 	}
#
# 	// release interfaces after error caught
# 	if (meta)
# 		meta->release();
# 	if (builder)
# 		builder->release();
# 	if (curs)
# 		curs->release();
# 	if (stmt)
# 		stmt->release();
# 	if (tra)
# 		tra->release();
# 	if (att)
# 		att->release();
# 	if (tpb)
# 		tpb->dispose();
#
# 	prov->release();
# 	status.dispose();
#
# 	return rc;
# }
