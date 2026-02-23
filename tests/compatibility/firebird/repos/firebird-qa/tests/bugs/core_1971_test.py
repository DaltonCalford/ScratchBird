#coding:utf-8

"""
ID:          issue-2409
ISSUE:       2409
TITLE:       Set the fixed and documented check order for WHERE clause and other conditional sentences
DESCRIPTION:
JIRA:        CORE-1971
FBTEST:      bugs.core_1971
"""

import pytest
from firebird.qa import *

init_script = """
    create table t_links (
      link_type integer,
      right_id integer,
      prop_value varchar(1024)
    );

    insert into t_links (link_type,right_id,prop_value) values(2,161,'2001');
    insert into t_links (link_type,right_id,prop_value) values(2,161,'2002');
    insert into t_links (link_type,right_id,prop_value) values(2,161,'2003');
    insert into t_links (link_type,right_id,prop_value) values(10,161,'any string');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select * from t_links
    where (right_id=161 and link_type=2) and cast(prop_value as integer)<>2001;

    select * from t_links
    where cast(prop_value as integer)<>2001 and (right_id=161 and link_type=2);
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
	LINK_TYPE                       2
	RIGHT_ID                        161
	PROP_VALUE                      2002

	LINK_TYPE                       2
	RIGHT_ID                        161
	PROP_VALUE                      2003

	LINK_TYPE                       2
	RIGHT_ID                        161
	PROP_VALUE                      2002

	LINK_TYPE                       2
	RIGHT_ID                        161
	PROP_VALUE                      2003
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "any string"
"""

@pytest.mark.version('>=2.5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

