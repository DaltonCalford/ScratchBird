#coding:utf-8

"""
ID:          1c69be2daf
ISSUE:       https://www.sqlite.org/src/tktview/1c69be2daf
TITLE:       Incorrect GROUP BY when input and output columns have the same name
DESCRIPTION:
    See:
    https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-dml.html#fblangref50-dml-select-groupby
    Check syntax of GROUR BY:
    ==========
    SELECT ... FROM ...
      GROUP BY <grouping-item> [, <grouping-item> ...]
      [HAVING <grouped-row-condition>]
      ...
     
    <grouping-item> ::=
        <non-aggr-select-item>
      | <non-aggr-expression>
     
    <non-aggr-select-item> ::=
        column-copy
      | column-alias
      | column-position
    ==========

    Pay attention to:
      column-copy ==> A literal copy, from the SELECT list, of an expression that contains no aggregate function
      column-alias ==> The alias, from the SELECT list, of an expression (column) that contains no aggregate function
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(m char(2));
    insert into t1 values('ax');
    insert into t1 values('bx');
    insert into t1 values('cy');

    set count on;
    select 'case-0' as msg, m, count(*) from t1 group by m;
    select 'case-1' as msg, substring(m from 2 for 1) as m_alias, count(*) from t1 group by m_alias;
    select 'case-2' as msg, substring(m from 2 for 1) as m, count(*) from t1 group by m;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MSG case-0
    M ax
    COUNT 1
    
    MSG case-0
    M bx
    COUNT 1
    
    MSG case-0
    M cy
    COUNT 1
    Records affected: 3
    
    MSG case-1
    M_ALIAS x
    COUNT 2
    
    MSG case-1
    M_ALIAS y
    COUNT 1
    Records affected: 2
    
    MSG case-2
    M x
    COUNT 2
    
    MSG case-2
    M y
    COUNT 1
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
