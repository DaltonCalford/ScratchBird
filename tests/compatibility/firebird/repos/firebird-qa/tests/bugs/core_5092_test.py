#coding:utf-8

"""
ID:          issue-5377
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5377
TITLE:       ISQL extract command looses COMPUTED BY field types
DESCRIPTION:
  Test creates database with empty table T1 that has computed by fileds with DDL appopriate to the ticket issues.
  Then we:
  1) extract metadata from database and store it to <init_meta.sql>;
  2) run query to the table T1 with setting sqlda_display = ON, and store output to <init_sqlda.log>;
  3) DROP table T1;
  4) try to apply script with extracted metadata (see step "1") -  it should pass without errors;
  5) AGAIN extract metadata and store it to <last_meta.sql>;
  6) AGAIN run query to T1 with set sqlda_display = on, and store output to <last_sqlda.log>;
  7) compare text files:
     <init_meta.sql> vs <last_meta.sql>
     <init_sqlda.log> vs <last_sqlda.log>
  8) Check that result of comparison is EMPTY (no rows).
JIRA:        CORE-5092
FBTEST:      bugs.core_5092
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

init_script = """
    recreate table t1 (
         n0 int

        ,si smallint computed by( 32767 )
        ,bi bigint computed by ( 2147483647 )
        ,s2 smallint computed by ( mod(bi, nullif(si,0)) )

        ,dv double precision computed by (pi())
        ,fv float computed by (dv*dv)
        ,nv numeric(3,1) computed by (sqrt(fv))

        ,dt date computed by ('now')
        ,tm time computed by ('now')
        ,dx timestamp computed by ( dt )
        ,tx timestamp computed by ( tm )

        ,c1 char character set win1251 computed by ('ы')
        ,c2 char character set win1252 computed by ('å')
        ,cu char character set utf8 computed by ('∑')

        ,c1x char computed by(c1)
        ,c2x char computed by(c2)
        ,cux char computed by(cu)

        ,b1 blob character set win1251 computed by ('ы')
        ,b2 blob character set win1252 computed by ('ä')
        ,bu blob character set utf8 computed by ('∑')
        ,bb blob computed by ('∞')

        ,b1x blob computed by (b1)
        ,b2x blob computed by (b2)
        ,bux blob computed by (bu)
        ,bbx blob computed by (bb)
    );
    --insert into t1 values(null);
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    # Initial metadata
    act.isql(switches=['-x'])
    initial_metadata = act.stdout.splitlines()
    # SQLDA initial
    sqlda_check = 'set list on; set sqlda_display on; select * from t1; commit; drop table t1; exit;'
    act.reset()
    act.isql(switches=['-q', '-m'], input=sqlda_check)
    initial_sqlda = act.stdout.splitlines()
    # Apply extracted metadata
    act.reset()
    act.isql(switches=[], input='\n'.join(initial_metadata), combine_output = True)
    # New metadata
    act.reset()
    act.isql(switches=['-x'])
    new_metadata = act.stdout.splitlines()
    # SQLDA new
    act.reset()
    act.isql(switches=['-q', '-m'], input=sqlda_check, combine_output = True)
    new_sqlda = act.stdout.splitlines()
    # Check
    assert list(unified_diff(initial_sqlda, new_sqlda)) == []
    assert list(unified_diff(initial_metadata, new_metadata)) == []
