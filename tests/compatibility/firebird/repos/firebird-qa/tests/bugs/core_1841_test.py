#coding:utf-8

"""
ID:          issue-2270
ISSUE:       2270
TITLE:       Possible overflow in RDB$VIEW_RELATIONS.RDB$CONTEXT_NAME
DESCRIPTION:
    Original titte is: If some VIEW used derived tables and long table names/aliases, it is possible to overflow RDB$VIEW_RELATIONS.RDB$CONTEXT_NAME
JIRA:        CORE-1841
FBTEST:      bugs.core_1841
NOTES:
    [26.06.2025] pzotov
    Re-implemented: use f-notation in order to remove hard-coded DDL string from test_script and expected_out.
    Removed 'SHOW VIEW'command because its output can change in any intensive developing FB version.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

VIEW_DDL = 'select rdb$relation_id from (select * from rdb$database long_alias_long_alias_1) long_alias_long_alias_2'
test_script = f"""
    set list on;
    set blob all;
    set count on;
    create view v_test (id) as
    {VIEW_DDL};
    commit;
    select r.rdb$view_source as blob_id from rdb$relations r where r.rdb$relation_name = upper('v_test');
"""

substitutions = [('BLOB_ID .*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    {VIEW_DDL}
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

