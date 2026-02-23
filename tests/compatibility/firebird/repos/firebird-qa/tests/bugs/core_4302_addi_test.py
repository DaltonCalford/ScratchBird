#coding:utf-8

"""
ID:          issue-4625-B
ISSUE:       4625
TITLE:       Lookup (or scan) in descending index could be very inefficient for some keys
DESCRIPTION:
  This is ADDITIONAL test for issue in ticket: "Pavel Zotov added a comment - 23/Dec/13 04:02 PM".
  Separate fix was required for this issue, see comments in
  https://sourceforge.net/p/firebird/mailman/message/31785278/
  ===
   + 2013-12-25 10:57  hvlad
   +   M src/jrd/btr.cpp
   +Additional fix for bug CORE-4302 : Lookup (or scan) in descending index could be very inefficient...
  ===
  Excessive fetches count reproduced on WI-T3.0.0.30566 (Alpha1 release).
  Current results were checked on Windows (2.5.6.26994, 3.0.0.32484, 4.0.0.138) and POSIX (4.0.0.138)
NOTES:
[29.07.2016]
  On 4.0.0.316 number of fetches is ~99 thus new threshold was added for engine = 4.0.
[18.08.2020]
  FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
  statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
  gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
  See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
  This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt

  Because of this, it was decided to replace 'alter sequence restart...' with subtraction of two gen values:
  c = gen_id(<g>, -gen_id(<g>, 0)) -- see procedure sp_restart_sequences.
JIRA:        CORE-4302
FBTEST:      bugs.core_4302_addi
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_restart_sequences as begin end;
    recreate table td(id int, s varchar(50));
    commit;
    set term ^;
    execute block as
    begin
      begin
        execute statement 'drop sequence g';
        when any do begin end
      end
    end^
    set term ;^
    commit;
    create sequence g;
    commit;

    set term ^;
    create or alter procedure sp_restart_sequences as
        declare c bigint;
    begin
        c = gen_id(g, -gen_id(g, 0));
    end
    ^
    set term ;^
    commit;

    recreate table t_mon(rn smallint, pg_fetches bigint);

    recreate view v_mon as
    select i.mon$page_fetches as pg_fetches
    from mon$attachments a
    left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id
    where a.mon$attachment_id = current_connection;

    set term ^;
    execute block as
    declare n int = 10000;
    declare m int;
    begin
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'q' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qw' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwe' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwer' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwert' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwerty' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwertyu' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwertyui' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwertyuio' ) returning :m-1 into m;
        m=n; while(m>0) do insert into td(id, s) values( gen_id(g,1), 'qwertyuiop' ) returning :m-1 into m;
    end^ set term ;^
    commit;

    create descending index td_s_des on td(s); commit;
    execute procedure sp_restart_sequences;
    commit;

    insert into t_mon(rn, pg_fetches) values( gen_id(g,1), (select pg_fetches from v_mon));

    set term ^;
    execute block as
        declare c int;
    begin
        select count(*) from rdb$database where exists(select * from td where s='qwertyuiop') into c;
    end
    ^
    set term ;^
    commit;

    insert into t_mon(rn, pg_fetches) values( gen_id(g,1), (select pg_fetches from v_mon));
    commit;

    insert into t_mon(rn, pg_fetches) values( gen_id(g,1), (select pg_fetches from v_mon));

    set term ^;
    execute block as
        declare c int;
    begin
        select count(*) from rdb$database where exists(select * from td where s='qwertyuioo') into c; -- note: `o` duplicated at the end of key
    end
    ^
    set term ;^
    commit;

    insert into t_mon(rn, pg_fetches) values( gen_id(g,1), (select pg_fetches from v_mon));
    commit;

    set list on;
    select
         rn as measure
        ,iif( fetches_diff < max_allowed,
              'OK, less then max_allowed',
              'BAD: '|| fetches_diff ||' - greater then ' || max_allowed || ' for engine = ' || rdb$get_context('SYSTEM','ENGINE_VERSION')
            ) as fetches_count
    from (
        select
            rn
           ,fetches_at_end - fetches_at_beg as fetches_diff
           ,max_for_25
           ,max_for_30
           ,cast( iif( rdb$get_context('SYSTEM','ENGINE_VERSION') >= '4.0',
                       max_for_40,
                       iif( rdb$get_context('SYSTEM','ENGINE_VERSION') >= '3.0',
                            max_for_30,
                            max_for_25
                          )
                     )
                  as int
                ) as max_allowed
        from (
          select
               rn
              ,max(iif(bg=1, pg_fetches, null)) as fetches_at_beg
              ,max(iif(bg=1, null, pg_fetches)) as fetches_at_end
              ,140 as max_for_25
              ,70  as max_for_30
              ,110  as max_for_40 -- 29.07.2016
           --  ^        ####################################################
           --  |        #                                                  #
           --  +------- #   T H R E S H O L D S    F O R    F E T C H E S  #
           --           #                                                  #
           --           ####################################################
          from (
              select 1+(rn-1)/2 as rn, mod(rn,2) as bg, pg_fetches
              from t_mon
          )
          group by rn
        )
    )
    order by measure;
"""

act = isql_act('db', test_script, substitutions=[('[ ]+', ' ')])

expected_stdout = """
    MEASURE                         1
    FETCHES_COUNT                   OK, less then max_allowed

    MEASURE                         2
    FETCHES_COUNT                   OK, less then max_allowed
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

