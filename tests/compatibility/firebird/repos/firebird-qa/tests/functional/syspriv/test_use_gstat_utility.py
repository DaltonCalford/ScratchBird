#coding:utf-8

"""
ID:          syspriv.use-gstat-utility
TITLE:       Check ability to obtain database statistics
DESCRIPTION:
  We create user and grant system privileges USE_GSTAT_UTILITY and IGNORE_DB_TRIGGERS to him.
  NB: pivilege 'IGNORE_DB_TRIGGERS' is required for get full db statistics, otherwise we get:
  Unable to perform operation: system privilege IGNORE_DB_TRIGGERS is missing

  Then we check that this user can extract DB statistics in TWO ways:
  1) common data except encryption info (it is called here 'base "sts_" output')
  2) only encryption info (I don't know why "sts_encryption" can not be used together with other switches...)
  Both these actions should not produce any error.
  Also, logs of them should contain all needed 'check words' and patterns - and we check this.
  Finally, we ensure that when user U01 gathered DB statistics then db-level trigger did NOT fire.

FBTEST:      functional.syspriv.use_gstat_utility

NOTES:
[31.10.2019] added check for generator pages in encryption block.

[18.05.2022] refactored to be used in firebird-qa suite.
Trace must have following lines when NON-privileged user gathers DB statistics:
    1) for common DB stat:
    ... START_SERVICE
    	service_mgr, (Service 000000000FE21DC0, TMP_SYSPRIV_USER:TMP_ROLE_FOR_USE_GSTAT_UTILITY, TCPv6:::1/55587, ...python.exe:16972)
    	"Database Stats"
    	-role TMP_ROLE_FOR_USE_GSTAT_UTILITY ...test.fdb -DATA -INDEX -SYSTEM -RECORD
    2) for encryption-related:
    ... START_SERVICE
    	service_mgr, (Service 000000000FE21DC0, TMP_SYSPRIV_USER:TMP_ROLE_FOR_USE_GSTAT_UTILITY, TCPv6:::1/55588, ...python.exe:16972)
    	"Database Stats"
    	-role TMP_ROLE_FOR_USE_GSTAT_UTILITY ...test.fdb -ENCRYPTION

Checked on 4.0.1.2692, 5.0.0.489.
"""

import re
import pytest
from firebird.qa import *
from firebird.driver import SrvStatFlag as stf
from firebird.driver.types import DatabaseError

substitutions = [('[ \t]+', ' ')]
db = db_factory()
tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_for_use_gstat_utility')

act = python_act('db', substitutions=substitutions)

dbstat_common_expected_output="True"
dbstat_encryption_expected_output="True"
final_chk_expected_stdout = "ATT_LOG_COUNT 0"

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, capsys):

    init_script = f"""
        set wng off;
        set bail on;
        set list on;
        set count on;

        create or alter view v_check as
        select
            mon$database_name
            ,current_user as who_ami
            ,r.rdb$role_name
            ,rdb$role_in_use(r.rdb$role_name) as RDB_ROLE_IN_USE
            ,r.rdb$system_privileges
        from mon$database m cross join rdb$roles r;
        commit;

        alter user {tmp_user.name} revoke admin role;
        revoke all on all from {tmp_user.name};
        commit;

        create or alter trigger trg_connect active on connect as
        begin
        end;
        commit;

        recreate table att_log (
            att_user varchar(255),
            att_prot varchar(255)
        );

        commit;


        recreate table test(s char(1000) unique using index test_s_unq);
        commit;

        insert into test select rpad('', 1000, uuid_to_char(gen_uuid()) ) from rdb$types;
        commit;

        grant select on v_check to public;
        grant select on att_log to public;
        --------------------------------- [ !! ] -- do NOT: grant select on test to {tmp_user.name}; -- [ !! ]
        commit;

        set term ^;
        create or alter trigger trg_connect active on connect as
        begin
          if ( upper(current_user) <> upper('SYSDBA') ) then
             in autonomous transaction do
             insert into att_log( att_user, att_prot )
             select
                  mon$user
                 ,mon$remote_protocol
             from mon$attachments
             where mon$user = current_user
             ;
        end
        ^
        set term ;^
        commit;

        -- Ability to get database statistics.
        -- NB: 'IGNORE_DB_TRIGGERS' - required for get full db statistics, otherwise:
        --  Unable to perform operation: system privilege IGNORE_DB_TRIGGERS is missing
        alter role {tmp_role.name}
            set system privileges to USE_GSTAT_UTILITY, IGNORE_DB_TRIGGERS;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
      """

    act.isql(switches=['-q'], input=init_script)

    with act.connect_server(user = tmp_user.name, password = tmp_user.password, role = tmp_role.name) as srv:
        try:
            srv.database.get_statistics(database = act.db.db_path, flags = stf.DATA_PAGES | stf.IDX_PAGES | stf.RECORD_VERSIONS | stf.SYS_RELATIONS, callback=act.print_callback)
        except DatabaseError as e:
            print(e.__str__())

    # Common DB statistics (WITHOUT encryption process details) must contain following phrases:
    dbstat_common_mandatory_phrases=[
        "rdb$database"
       ,"rdb$index"
       ,"primary pointer page"
       ,"index root page"
       ,"total formats"
       ,"total records"
       ,"total versions"
       ,"total fragments"
       ,"compression ratio"
       ,"pointer pages"
       ,"data pages"
       ,"primary pages"
       ,"empty pages"
       ,"blobs"
       ,"swept pages"
       ,"full pages"
       ,"fill distribution"
       ,"0 - 19%"
       ,"80 - 99%"
    ]

    act.expected_stdout = dbstat_common_expected_output

    out_lines = capsys.readouterr().out.lower()
    all_found = True
    for c in dbstat_common_mandatory_phrases:
        if c not in out_lines:
            all_found = False
            print( f'ERROR. Phrase "{c}" not found in the statistics output' )
            break
    print(all_found)

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ----------------------------------------------------------------------------

    with act.connect_server(user = tmp_user.name, password = tmp_user.password, role = tmp_role.name) as srv:
        try:
            srv.database.get_statistics(database = act.db.db_path, flags = stf.ENCRYPTION , callback=act.print_callback)
        except DatabaseError as e:
            print(e.__str__())

    # Every of following PATTERNS must be found in the encryption-related DB statistics output:
    dbstat_encryption_mandatory_patterns=[
         "data pages: total \\d+, encrypted \\d+, non-crypted \\d+"
        ,"index pages: total \\d+, encrypted \\d+, non-crypted \\d+"
        ,"blob pages: total \\d+, encrypted \\d+, non-crypted \\d+"
        ,"generator pages: total \\d+, encrypted \\d+, non-crypted \\d+" # this was added in octomer-2019
    ]
    dbstat_encryption_mandatory_patterns = [re.compile(s) for s in dbstat_encryption_mandatory_patterns]

    act.expected_stdout = dbstat_encryption_expected_output
    out_lines = capsys.readouterr().out.lower()
    all_found = True
    for p in dbstat_encryption_mandatory_patterns:
        if not p.search(out_lines):
            all_found = False
            print(f'ERROR. Pattern {p} not found in the ENCRYPTION-related output:')
            print(out_lines)
            break
    print(all_found)

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ----------------------------------------------------------------------------

    # Finally, we check that connections of tmp_user were not logged
    # because he has granted privilege IGNORE_DB_TRIGGERS:

    act.isql(switches=['-q'], input='set list on;select count(*) as att_log_count from att_log;')
    act.expected_stdout = final_chk_expected_stdout
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
