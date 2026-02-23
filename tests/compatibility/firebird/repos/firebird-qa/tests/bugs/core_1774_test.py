#coding:utf-8

"""
ID:          issue-2198
ISSUE:       2198
TITLE:       Problem with COLLATE ES_ES_CI_AI
DESCRIPTION:
JIRA:        CORE-1774
FBTEST:      bugs.core_1774
"""

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

test_script = """
    recreate table table_a (
        field_a varchar(100) character set iso8859_1 collate es_es_ci_ai
    );
    commit;
    insert into table_a (field_a) values ('Hace buena noche');
    insert into table_a (field_a) values ('Voy a hacer de comer');
    insert into table_a (field_a) values ('The web is hacked');
    commit;

    set list on;
    select * from table_a where field_a like '%HACK%';
"""

act = isql_act('db', test_script)

expected_stdout = """
    FIELD_A                         The web is hacked
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

