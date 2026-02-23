#coding:utf-8

"""
ID:          issue-4324
ISSUE:       4324
TITLE:       No records in the table rdb$dependencies for ddl trigger
DESCRIPTION:
JIRA:        CORE-3992
FBTEST:      bugs.core_3992
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table ddl_log (
        id integer,
        ddl_event varchar(25),
        sql blob sub_type text
    );

    set term ^;
    create or alter trigger ddl_log_trigger after any ddl statement
    as
    begin
      insert into ddl_log(sql, ddl_event)
        values (rdb$get_context('DDL_TRIGGER', 'SQL_TEXT'),
                rdb$get_context('DDL_TRIGGER', 'DDL_EVENT') );
    end
    ^
    set term ;^
    commit;

    set list on;

    select sign(count(*)) "is_any_rows_there ?"
    from rdb$dependencies d
    where upper('ddl_log_trigger') in (d.rdb$dependent_name, d.rdb$depended_on_name);
"""

act = isql_act('db', test_script)

expected_stdout = """
    is_any_rows_there ?             1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

