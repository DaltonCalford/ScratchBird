#coding:utf-8

"""
ID:          issue-1092
ISSUE:       1092
TITLE:       Tricky index names can defeat the parsing logic when generating a human readable plan
DESCRIPTION:
JIRA:        CORE-2508
FBTEST:      bugs.core_2508
NOTES:
    [06.09.2023]
    Replaced query so that DIFFERENT indices are involved (because WHERE-expression refers to diff. columns).
    This is needed in FB 4.x+ after:
    https://github.com/FirebirdSQL/firebird/commit/0493422c9f729e27be0112ab60f77e753fabcb5b
    ("Better processing and optimization if IN <list> predicates (#7707)")
    Old query that did use IN predicate no more applicable here: all occurences of the same index
    that works for mining data are now "collapsed" to the single one, i.e.:
    PLAN (<tabname> INDEX (<idxname>, <idxname>, <idxname>)) ==> PLAN (<tabname> INDEX (<idxname>)).

    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create sequence g;
    create table t(a int, b int);
    insert into t(a,b) select gen_id(g,1),gen_id(g,1) from rdb$types;
    commit;
    create index "abc(" on t(a);
    create descending index "mod(" on t(b);
    set planonly;
    select * from t where a <= 33 and b >= 22;

    -- On 2.5.9.27156 plan for that query was:
    -- PLAN (T INDEX (abc(mod())
    --                  ^^^
    --                   | 
    --                   +---- NO comma here!
    -- Compare with 3.x:
    -- PLAN (T INDEX (abc(, mod())
"""

substitutions = [ ('[ \t]+', ' '), ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN (T INDEX (abc(, mod())
"""

expected_stdout_6x = """
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."abc(", "PUBLIC"."mod("))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
