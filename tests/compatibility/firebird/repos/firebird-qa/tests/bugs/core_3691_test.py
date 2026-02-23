#coding:utf-8

"""
ID:          issue-4039
ISSUE:       4039
TITLE:       Missing constraint name in foreign key error message in FB 2.1.4
DESCRIPTION:
JIRA:        CORE-3691
FBTEST:      bugs.core_3691
NOTES:
    [27.06.2025] pzotov
    Added 'SCHEMA_PREFIX' to be substituted in expected_out on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    --  Note. Info about problematic key exists since 2.5.3
    recreate table tmain(id int primary key using index tmain_pk);
    commit;
    insert into tmain values(1);
    commit;
    recreate table tdetl(id int primary key using index tdetl_pk, pid int);
    commit;
    insert into tdetl values(1,2);
    commit;
    alter table tdetl add constraint tdetl_fk foreign key(pid) references tmain(id);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):

    SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    
    expected_stdout = f"""
        Statement failed, SQLSTATE = 23000
        violation of FOREIGN KEY constraint "TDETL_FK" on table {SCHEMA_PREFIX}"TDETL"
        -Foreign key reference target does not exist
        -Problematic key value is ("PID" = 2)
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

