#coding:utf-8

"""
ID:          issue-4786
ISSUE:       4786
TITLE:       Add DROP NOT NULL to ALTER COLUMN
DESCRIPTION:
  This test does NOT verifies work when table has some data. It only checks ability to
  issue DDL statement.
JIRA:        CORE-4466
FBTEST:      bugs.core_4466
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    commit;

    create or alter view v_test_fields as
    select rf.rdb$field_name fld_name, coalesce(rf.rdb$null_flag, 0) is_NOT_null
    from rdb$relation_fields rf
    where rf.rdb$relation_name = upper('TEST')
    order by rf.rdb$field_position;
    commit;

    set width fld_name 10;

    recreate table test(
      si smallint not null
     ,ni int not null
     ,bi bigint not null
     ,nf numeric(18,0) not null
     ,dp double precision not null
     ,dt date not null
     ,tm time not null
     ,ts timestamp not null
     ,ss varchar(100) not null
     ,bs blob not null
    );
    commit;

    select 'init metadata' msg, v.* from v_test_fields v;

    alter table test
      alter si drop not null
     ,alter ni drop not null
     ,alter bi drop not null
     ,alter nf drop not null
     ,alter dp drop not null
     ,alter dt drop not null
     ,alter tm drop not null
     ,alter ts drop not null
     ,alter ss drop not null
     ,alter bs drop not null
    ;

    select 'drop not null' msg,v.* from v_test_fields v;

    alter table test
      alter si set not null
     ,alter ni set not null
     ,alter bi set not null
     ,alter nf set not null
     ,alter dp set not null
     ,alter dt set not null
     ,alter tm set not null
     ,alter ts set not null
     ,alter ss set not null
     ,alter bs set not null
    ;

    select 'reset not null' msg, v.* from v_test_fields v;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    MSG           FLD_NAME    IS_NOT_NULL
    ============= ========== ============
    init metadata SI                    1
    init metadata NI                    1
    init metadata BI                    1
    init metadata NF                    1
    init metadata DP                    1
    init metadata DT                    1
    init metadata TM                    1
    init metadata TS                    1
    init metadata SS                    1
    init metadata BS                    1


    MSG           FLD_NAME    IS_NOT_NULL
    ============= ========== ============
    drop not null SI                    0
    drop not null NI                    0
    drop not null BI                    0
    drop not null NF                    0
    drop not null DP                    0
    drop not null DT                    0
    drop not null TM                    0
    drop not null TS                    0
    drop not null SS                    0
    drop not null BS                    0


    MSG            FLD_NAME    IS_NOT_NULL
    ============== ========== ============
    reset not null SI                    1
    reset not null NI                    1
    reset not null BI                    1
    reset not null NF                    1
    reset not null DP                    1
    reset not null DT                    1
    reset not null TM                    1
    reset not null TS                    1
    reset not null SS                    1
    reset not null BS                    1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

