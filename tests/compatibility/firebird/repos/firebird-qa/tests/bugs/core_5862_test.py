#coding:utf-8

"""
ID:          issue-6122
ISSUE:       6122
TITLE:       Varchar computed column without explicit type does not populate RDB$CHARACTER_LENGTH
DESCRIPTION:
JIRA:        CORE-5862
FBTEST:      bugs.core_5862
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    recreate table test (
        id int
        ,vc_default_user varchar(100) default user
        ,vc_default_literal varchar(100) default 'literal'
        ,vc_generated_explicit varchar(201) computed by (vc_default_user || ' ' || vc_default_literal)
        ,vc_generated_implicit computed by (vc_default_user || ' ' || vc_default_literal)
    );
    commit;

    set list on;
    select
         rf.rdb$field_name
        ,ff.rdb$field_length
        ,ff.rdb$character_length
    from rdb$relation_fields rf
    join rdb$fields ff on rf.rdb$field_source = ff.rdb$field_name
    where
        upper(rf.rdb$relation_name) = upper('test')
        and upper(rf.rdb$field_name) starting with upper('vc_generated_')
    order by rf.rdb$field_position
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FIELD_NAME                  VC_GENERATED_EXPLICIT

    RDB$FIELD_LENGTH                804
    RDB$CHARACTER_LENGTH            201

    RDB$FIELD_NAME                  VC_GENERATED_IMPLICIT

    RDB$FIELD_LENGTH                804
    RDB$CHARACTER_LENGTH            201
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
