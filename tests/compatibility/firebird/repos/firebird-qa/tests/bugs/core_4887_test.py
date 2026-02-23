#coding:utf-8

"""
ID:          issue-5181
ISSUE:       5181
TITLE:       AFTER CREATE/ALTER PACKAGE DDL triggers runs in incorrectly moment
DESCRIPTION:
JIRA:        CORE-4887
FBTEST:      bugs.core_4887
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create exception exc_empty_pkg 'Empty package';
    create sequence g;
    commit;

    set term ^;
    create or alter trigger t_trig_pkg_head after create package as
    begin
      rdb$set_context('USER_SESSION','DDL_TRIGGER_ON_PKG_HEAD_FIRING', gen_id(g,1));
      if ( not exists(select * from rdb$functions where rdb$package_name = rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME') ) ) then
          exception exc_empty_pkg;
    end
    ^
    create or alter trigger t_trig_pkg_body after create package body as
    begin
      rdb$set_context('USER_SESSION','DDL_TRIGGER_ON_PKG_BODY_FIRING', gen_id(g,1));
    end
    ^
    set term ;^
    commit;

    set term ^;
    execute block as
    begin
      rdb$set_context('USER_SESSION','PACKAGE_HEADER_CREATION_START',  gen_id(g,1));
    end
    ^
    create package xpk1 as
    begin
      function f1 returns integer;
    end
    ^
    execute block as
    begin
      rdb$set_context('USER_SESSION','PACKAGE_HEADER_CREATION_FINISH',  gen_id(g,1));
    end
    ^
    commit
    ^

    execute block as
    begin
      rdb$set_context('USER_SESSION','PACKAGE_BODY_CREATION_START',  gen_id(g,1));
    end
    ^
    create package body xpk1 as
    begin
      function f1 returns integer as
      begin
         return 12345;
      end
    end
    ^
    execute block as
    begin
      rdb$set_context('USER_SESSION','PACKAGE_BODY_CREATION_FINISH',  gen_id(g,1));
    end
    ^
    set term ;^

    select event_seq, event_name
    from(
        select
             cast( rdb$get_context('USER_SESSION','PACKAGE_HEADER_CREATION_START') as int) event_seq
            ,'pkg_header_start' event_name
        from rdb$database
        union all
        select
             cast( rdb$get_context('USER_SESSION','PACKAGE_HEADER_CREATION_FINISH') as int)
            ,'pkg_header_finish'
        from rdb$database
        union all
        select
            cast( rdb$get_context('USER_SESSION','PACKAGE_BODY_CREATION_START') as int)
            ,'pkg_body_start'
        from rdb$database
        union all
        select
            cast( rdb$get_context('USER_SESSION','PACKAGE_BODY_CREATION_FINISH') as int)
            ,'pkg_body_finish'
        from rdb$database
        union all
        select
            cast( rdb$get_context('USER_SESSION','DDL_TRIGGER_ON_PKG_HEAD_FIRING') as int)
            ,'trg_pkg_head_firing'
        from rdb$database
        union all
        select
            cast( rdb$get_context('USER_SESSION','DDL_TRIGGER_ON_PKG_BODY_FIRING') as int)
            ,'trg_pkg_body_firing'
        from rdb$database
    )
    order by event_seq;
"""

act = isql_act('db', test_script)

expected_stdout = """
    EVENT_SEQ                       1
    EVENT_NAME                      pkg_header_start

    EVENT_SEQ                       2
    EVENT_NAME                      trg_pkg_head_firing

    EVENT_SEQ                       3
    EVENT_NAME                      pkg_header_finish

    EVENT_SEQ                       4
    EVENT_NAME                      pkg_body_start

    EVENT_SEQ                       5
    EVENT_NAME                      trg_pkg_body_firing

    EVENT_SEQ                       6
    EVENT_NAME                      pkg_body_finish
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

