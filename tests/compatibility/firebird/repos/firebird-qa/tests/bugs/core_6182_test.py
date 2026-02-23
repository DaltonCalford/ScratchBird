#coding:utf-8

"""
ID:          issue-6427
ISSUE:       6427
TITLE:       Firebird's internal timer incorrectly resets existing timer entries
DESCRIPTION:
  ExtConnPoolLifeTime acts as countdown for activity in MOST RECENT database (of several)
  rather then separate for each of used databases

  We create one main ('head') DB (only for make single attach to it) and four test databases for making EDS connections to each of them from main DB.
  Special user is created using LEGACY plugin because of comment in the ticket by hvlad 06/Nov/19 01:36 PM.
                                ~~~~~~
  Then we do subsequent connections to each of these databases using EDS mechanism, with delay between them (and also with final delay).
  Total sum of seconds that execution was paused is 4 * TDELAY - must be GREATER than config parameter ExtConnPoolLifeTime.
  After last delay will elapsed, we again establish connections to each of these databases and try to execute DROP DATABASE statements.

  Main idea: FIRST of this databases (which was firstly used to EDS connection) must have NO any attachments in its ExtPool and DROP must pass w/o any problems.

  ::: NOTE ::: ATTENTION ::: ACHTUNG :::

  We can not issue 'DROP DATABASE' immediately becase if some connection remains in ExtPool then FDB will raise exception that can not be catched.
  For this reason we have to kill all attachments using 'DELETE FROM MON$ATTACHMENTS' statement. Number of attachments that were deleted will show
  whether there was some 'auxiliary' connections or not. For the first of checked databases this number must be 0 (zero).
  Otherwise one can state that the problem with ExtPool still exists.

  Checked on:
    4.0.0.1646 SS/SC: ~19s (most of time is idle because of delays that is necessary for check that connections disappeared from ExtPool)
    4.0.0.1646 CS: 21.339s - but this test is not needed for this acrh.
JIRA:        CORE-6182
FBTEST:      bugs.core_6182
"""

import pytest
import time
from firebird.qa import *

db = db_factory(filename='core_6182-main.fdb')
db_a = db_factory(filename='core_6182-1.fdb')
db_b = db_factory(filename='core_6182-2.fdb')
db_c = db_factory(filename='core_6182-3.fdb')
db_d = db_factory(filename='core_6182-4.fdb')

act = python_act('db')

expected_stdout = """
    Number of deleted attachments for DB # 1: 0
"""

ddl_eds = """
set bail on;
set term ^;
-- create procedure sp_del_att returns( list_of_killed varchar(255) ) as
create procedure sp_del_att returns( num_of_killed smallint ) as
begin
    num_of_killed = 0;
    for
        select mon$attachment_id as killed_att
        from mon$attachments
        where mon$system_flag is distinct from 1 and mon$attachment_id != current_connection
    as cursor c
    do
    begin
        -- list_of_killed = list_of_killed || c.killed_att || ',';
        delete from mon$attachments where current of c;
        num_of_killed = num_of_killed + 1;
    end
    suspend;
end
^
set term ;^
commit;
grant execute on procedure sp_del_att to public;
commit;
"""

eds_user = user_factory('db', name='tmp$c6182_leg', password='123', plugin='Legacy_UserManager')

def init_main_db(act: Action, eds_user: User):
    ddl_script = f"""
        set bail on;
        set term ^;
        create procedure sp_do_eds ( a_target_db varchar(128) ) returns( source_dts timestamp, who_is_connecting varchar(128), source_db varchar(128), target_db varchar(128), target_dts timestamp ) as
        begin
            source_dts = cast('now' as timestamp);
            for
                execute statement
                    ('select cast(? as varchar(128)) as connect_from_db, current_user, mon$database_name as connect_to_db, cast(''now'' as timestamp) from mon$database')
                    ( rdb$get_context('SYSTEM', 'DB_NAME') )
                    on external
                        'localhost:' || a_target_db
                    as
                        user '{eds_user.name}'
                        password '{eds_user.password}'
                into who_is_connecting, source_db, target_db, target_dts
           do
               suspend;
        end
        ^
        set term ;^
        commit;
        grant execute on procedure sp_do_eds to {eds_user.name};
        grant drop database to {eds_user.name};
        commit;
"""
    act.isql(switches=[], input=ddl_script)

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')
def test_1(act: Action, db_a: Database, db_b: Database, db_c: Database,
           db_d: Database, eds_user: User, capsys):
    pool_life = act.get_config('ExtConnPoolLifeTime')
    if pool_life is None:
        # It's pointless to run it
        pytest.skip("ExtConnPoolLifeTime = 0")
    pool_life = int(pool_life)
    TDELAY = (pool_life / 4) + 1
    if TDELAY > 60:
        # It will take more than 4 minutes to run, so skip it
        pytest.skip("ExtConnPoolLifeTime > 240")
    init_main_db(act, eds_user)
    for db in [db_a, db_b, db_c, db_d]:
        act.reset()
        act.isql(switches=[], input=ddl_eds, use_db=db)
    #
    with act.db.connect(user=eds_user.name, password=eds_user.password) as con:
        with con.cursor() as c:
            for db in [db_a, db_b, db_c, db_d]:
                c.execute('select * from sp_do_eds(?)', [db.db_path]).fetchall()
                con.commit()
                time.sleep(TDELAY)
    #
    for db in [db_a, db_b, db_c, db_d]:
        with db.connect() as con:
            c = con.cursor()
            row = c.call_procedure('sp_del_att')
            if db is db_a:
                print(f'Number of deleted attachments for DB # 1: {row[0]}')
    #
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
