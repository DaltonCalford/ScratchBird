#coding:utf-8

"""
ID:          issue-2714
ISSUE:       2714
TITLE:       Wrong (primary) constraint name is reported for the foreign key violation during FK creation
DESCRIPTION:
JIRA:        CORE-2289
FBTEST:      bugs.core_2289
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table packet_detail(id int, packet_id int);
    recreate table packet(id int, constraint packet_pk primary key(id) using index packet_idx);
    commit;
    insert into packet_detail(id, packet_id) values(1, 753);
    commit;

    alter table packet_detail
    add constraint packet_detail_fk
    foreign key (packet_id)
    references packet(id)
    using index packet_detail_idx
    ;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint "PACKET_DETAIL_FK" on table "PACKET_DETAIL"
    -Foreign key reference target does not exist
    -Problematic key value is ("PACKET_ID" = 753)
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint "PACKET_DETAIL_FK" on table "PUBLIC"."PACKET_DETAIL"
    -Foreign key reference target does not exist
    -Problematic key value is ("PACKET_ID" = 753)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
