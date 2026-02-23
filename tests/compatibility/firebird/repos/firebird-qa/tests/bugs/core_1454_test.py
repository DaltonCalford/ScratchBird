#coding:utf-8

"""
ID:          issue-1872
ISSUE:       1872
TITLE:       ALTER mechanism for computed fields
DESCRIPTION:
  Computed field had a lot of inconsistencies and problems
  It's possible to use a explicit type, but only together with a (new) computed expression.
JIRA:        CORE-1454
FBTEST:      bugs.core_1454
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t (
      f1 varchar(10),
      f2 varchar(10),
      cf computed by (f1 || ' - ' || f2)
    );

    insert into t (f1,f2) values ('0123456789','abcdefghij');
    commit;
  """

db = db_factory(init=init_script, charset='win1252')

test_script = """
    set list on;
    set width fld_name 31;
    set width fld_expr 80;

    select f1,f2,cf from t;

    select b.rdb$field_name fld_name, cast(a.rdb$computed_source as varchar(80)) fld_expr, a.rdb$field_length fld_length
    from rdb$fields a
    join rdb$relation_fields b
        on a.rdb$field_name = b.rdb$field_source
    where b.rdb$field_name = upper('cf');

    alter table t alter cf type varchar(30) computed by (f1 || ' - ' || f2 || ' - more');
    commit;

    select f1,f2,cf from t;

    select b.rdb$field_name fld_name, cast(a.rdb$computed_source as varchar(80)) fld_expr, a.rdb$field_length fld_length
    from rdb$fields a
    join rdb$relation_fields b
        on a.rdb$field_name = b.rdb$field_source
    where b.rdb$field_name = upper('cf');
"""

act = isql_act('db', test_script)

expected_stdout = """
    F1                              0123456789
    F2                              abcdefghij
    CF                              0123456789 - abcdefghij
    FLD_NAME                        CF
    FLD_EXPR                        (f1 || ' - ' || f2)
    FLD_LENGTH                      23
    F1                              0123456789
    F2                              abcdefghij
    CF                              0123456789 - abcdefghij - more
    FLD_NAME                        CF
    FLD_EXPR                        (f1 || ' - ' || f2 || ' - more')
    FLD_LENGTH                      30
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
