#coding:utf-8

"""
ID:          issue-2563
ISSUE:       2563
TITLE:       Indexed retrieval cannot be chosen if a stored procedure is used inside the comparison predicate
DESCRIPTION:
JIRA:        CORE-2132
FBTEST:      bugs.core_2132
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1 (col int primary key);
    set term ^ ;
    create procedure p1 returns (ret int) as begin ret = 0; suspend; end ^
    create procedure p2 (prm int) returns (ret int) as begin ret = prm; suspend; end ^
    set term ; ^
    commit;
    insert into t1 (col) values (0);
    commit;

    set plan on;
    -- index
    select 'point-01' msg, t1.* from t1 where col = 0;

    -- natural
    select 'point-02' msg, t1.* from t1 where col = col;
    
    -- index
    select 'point-03' msg, t1.* from t1 where col = ( select 0 from rdb$database );
    
    -- natural
    select 'point-04' msg, t1.* from t1 where col = ( select col from rdb$database );
    
    -- index (currently natural)
    select 'point-05' msg, t1.* from t1 where col = ( select 0 from p1 );
    
    -- index (currently natural)
    select 'point-06' msg, t1.* from t1 where col = ( select ret from p1 );
    
    -- natural
    select 'point-07' msg, t1.* from t1 where col = ( select col from p1 );
    
    -- index (currently natural)
    select 'point-08' msg, t1.* from t1 where col = ( select 0 from p2(0) );
    
    -- index (currently natural)
    select 'point-09' msg, t1.* from t1 where col = ( select ret from p2(0) );
    
    -- natural
    select 'point-10' msg, t1.* from t1 where col = ( select col from p2(0) );
    
    -- natural
    select 'point-11' msg, t1.* from t1 where col = ( select 0 from p2(col) );
    
    -- natural
    select 'point-12' msg, t1.* from t1 where col = ( select ret from p2(col) );
    
    -- natural
    select 'point-13' msg, t1.* from t1 where col = ( select col from p2(col) );
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-01
        COL 0
        PLAN (T1 NATURAL)
        MSG point-02
        COL 0
        PLAN (RDB$DATABASE NATURAL)
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-03
        COL 0
        PLAN (RDB$DATABASE NATURAL)
        PLAN (T1 NATURAL)
        MSG point-04
        COL 0
        PLAN (P1 NATURAL)
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-05
        COL 0
        PLAN (P1 NATURAL)
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-06
        COL 0
        PLAN (P1 NATURAL)
        PLAN (T1 NATURAL)
        MSG point-07
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-08
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 INDEX (RDB$PRIMARY1))
        MSG point-09
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 NATURAL)
        MSG point-10
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 NATURAL)
        MSG point-11
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 NATURAL)
        MSG point-12
        COL 0
        PLAN (P2 NATURAL)
        PLAN (T1 NATURAL)
        MSG point-13
        COL 0
"""

expected_stdout_6x = """
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-01
        COL 0
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-02
        COL 0
        PLAN ("SYSTEM"."RDB$DATABASE" NATURAL)
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-03
        COL 0
        PLAN ("SYSTEM"."RDB$DATABASE" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-04
        COL 0
        PLAN ("PUBLIC"."P1" NATURAL)
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-05
        COL 0
        PLAN ("PUBLIC"."P1" NATURAL)
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-06
        COL 0
        PLAN ("PUBLIC"."P1" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-07
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-08
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" INDEX ("PUBLIC"."RDB$PRIMARY1"))
        MSG point-09
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-10
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-11
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-12
        COL 0
        PLAN ("PUBLIC"."P2" NATURAL)
        PLAN ("PUBLIC"."T1" NATURAL)
        MSG point-13
        COL 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
