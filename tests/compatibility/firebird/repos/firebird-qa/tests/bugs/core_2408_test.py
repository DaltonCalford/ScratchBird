#coding:utf-8

"""
ID:          issue-2826
ISSUE:       2826
TITLE:       isql -ex puts default values of sp parameters before the NOT NULL and COLLATE flags
DESCRIPTION:
  Quote from ticket: "make a procedure with NOT NULL and/or COLLATE flags *and* a default
  value on any parameter". Test enchances this by checking not only procedure but also
  function and package. Also, check is performed for table (I've encountered the same for
  TABLES definition in some old databases).

  Algorithm is similar to test for #5374: we create several DB objects which do have
  properties from ticket. Then we extract metadata and save it into file as 'initial' text.
  After this we drop all objects and make attempt to APPLY just extracted metadata script.
  It should perform without errors. Finally, we extract metadata again and do COMPARISON
  of their current content and those which are stored 'initial' file.
JIRA:        CORE-2408
FBTEST:      bugs.core_2408
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

init_script = """
    set bail on;
    set autoddl off;
    commit;

    create collation nums_coll for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create collation name_coll for utf8 from unicode no pad case insensitive accent insensitive;

    create domain dm_test varchar(20) character set utf8 default 'foo' not null collate nums_coll;

    create table test(
        s1 varchar(20) character set utf8 default 'foo' not null collate nums_coll
       ,s2 dm_test
       ,s3 dm_test default 'bar'
       ,s4 dm_test default 'rio' collate name_coll
    );

    set term ^;
    create or alter procedure sp_test(
        p1 varchar(20) character set utf8 not null collate nums_coll default 'foo'
       ,p2 dm_test default 'qwe'
       ,p3 dm_test default 'bar'
       ,p4 dm_test collate name_coll default 'rio'
    ) returns (
        o1 varchar(80)
       ,o2 dm_test collate name_coll
    )
    as
    begin
      o1 = lower(p1 || p2 || p3);
      o2 = upper(p4);
      suspend;
    end
    ^

    create or alter function fn_test(
        p1 varchar(20) character set utf8 not null collate nums_coll default 'foo'
       ,p2 dm_test default 'qwe'
       ,p3 dm_test default 'bar'
       ,p4 dm_test collate name_coll default 'rio'
    ) returns dm_test collate name_coll
    as
    begin
      return lower(left(p1,5) || left(p2,5) || left(p3,5) || left(p4,5));
    end
    ^

    recreate package pg_test as
    begin
        procedure pg_proc(
            p1 varchar(20) character set utf8 not null collate nums_coll default 'foo'
           ,p2 dm_test default 'qwe'
           ,p3 dm_test default 'bar'
           ,p4 dm_test collate name_coll default 'rio'
        ) returns (
            o1 varchar(80)
           ,o2 dm_test collate name_coll
        );
        function pg_func(
            p1 varchar(20) character set utf8 not null collate nums_coll default 'foo'
           ,p2 dm_test default 'qwe'
           ,p3 dm_test default 'bar'
           ,p4 dm_test collate name_coll default 'rio'
        ) returns dm_test collate name_coll;
    end
    ^

    create package body pg_test as
    begin
        procedure pg_proc(
            p1 varchar(20) character set utf8 not null collate nums_coll
           ,p2 dm_test
           ,p3 dm_test
           ,p4 dm_test collate name_coll
        ) returns (
            o1 varchar(80)
           ,o2 dm_test collate name_coll
        ) as
        begin
            o1 = lower(p1 || p2 || p3);
            o2 = upper(p4);
            suspend;
        end

        function pg_func(
            p1 varchar(20) character set utf8 not null collate nums_coll
           ,p2 dm_test
           ,p3 dm_test
           ,p4 dm_test collate name_coll
        ) returns dm_test collate name_coll as
        begin
            return lower(left(p1,5) || left(p2,5) || left(p3,5) || left(p4,5));
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db')

ddl_clear_all = """
    drop package pg_test;
    drop function fn_test;
    drop procedure sp_test;
    drop table test;
    drop domain dm_test;
    drop collation name_coll;
    drop collation nums_coll;
    commit;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    # Extract metadata
    act.isql(switches=['-x'])
    initial_meta = act.stdout
    # Clear all
    act.reset()
    act.isql(switches=[], input=ddl_clear_all)
    # Recreate metadata
    act.reset()
    act.isql(switches=[], input=initial_meta)
    # Extract metadata again
    act.reset()
    act.isql(switches=['-x'])
    current_meta = act.stdout
    # Compare metadata
    meta_diff = '\n'.join(unified_diff(initial_meta.splitlines(), current_meta.splitlines()))
    assert meta_diff == ''
