#coding:utf-8

"""
ID:          issue-4297
ISSUE:       4297
TITLE:       It is not possible to create a ddl-trigger with "any DDL statement" clause
DESCRIPTION:
JIRA:        CORE-3964
FBTEST:      bugs.core_3964
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table mp$modified_tables (relation_name char(31));
    commit;
    create index mp$modified_tables_idx on mp$modified_tables (relation_name);
    commit;

    set term ^;
    create trigger taa_sql1
    active after any ddl statement position 0 as
    begin
        if (
            rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE') = 'TABLE'
            and
            (
                rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE') in ('CREATE', 'DROP')
                or
                rdb$get_context('DDL_TRIGGER', 'SQL_SOURCE') containing 'FIELD'
            )
        ) then
            insert into mp$modified_tables (relation_name)
            values (rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME'));
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
