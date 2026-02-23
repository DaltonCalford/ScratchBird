#coding:utf-8

"""
ID:          b50528af44
ISSUE:       https://www.sqlite.org/src/tktview/b50528af44
TITLE:       "WHERE a=? AND b IN (?,?,...) AND c>?" query using the seekscan optimization sometimes returns extra rows.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a char(10), b int, c int not null, primary key(a,b,c) using index t1_pk);

    insert into t1(a,b,c)
    select 'xyz' || (r/10), r/6, r
    from (
        select row_number()over()-1 as r
        from rdb$types,rdb$types
        rows 1997
    );
    insert into t1 values('abc',234,6);
    insert into t1 values('abc',345,7);

    set count on;
    set plan on;
    select a,b,c from t1 
    where 
        b in (235, 345) 
        and c<=3 
        and a='abc' 
    order by a, b;
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN (T1 ORDER T1_PK)
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
