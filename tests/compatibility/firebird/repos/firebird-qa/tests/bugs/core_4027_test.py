#coding:utf-8

"""
ID:          issue-4357
ISSUE:       4357
TITLE:       Creating table with computed fields containing "SELECT FIRST" produces corrupted result
DESCRIPTION: Broken output in ISQL command SHOW TABLE <T> for computed-by field(s).
JIRA:        CORE-4027
FBTEST:      bugs.core_4027
NOTES:
    [28.06.2025] pzotov
    See also test for https://github.com/FirebirdSQL/firebird/issues/4402
    Replaced 'SHOW' command with query to RDB tables. Bug still can be seen in 2.5.9.27156.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

COMPUTED_EXPR_1 = """
    (
        select first 1 t.po_number
        from turnovers t
        where t.agent_id=test.agent_id
        order by t.order_date
    )
"""

COMPUTED_EXPR_2 = """
    (
        select agent_name
        from contragents a
        where a.agent_id = test.agent_id
    )
"""

test_script = f"""
    set list on;
    set blob all;
    set count on;

    -- NB: fixed only in 3.0 (checked 30.03.2015)
    recreate table test (id int);
    commit;

    recreate table contragents (
        agent_id   int not null
        ,agent_name varchar(25) not null
    );
    commit;

    recreate table turnovers(
        po_number  char(8) not null
        ,agent_id   int not null
        ,order_date timestamp default 'now' not null
    );
    commit;

    recreate table test (
        agent_id integer not null,
        first_po_number computed by ( {COMPUTED_EXPR_1} ),
        agent_name computed by ( {COMPUTED_EXPR_2} )
    );
    commit;

    select
        rf.rdb$field_name fld_name
        ,f.rdb$field_type fld_type
        ,f.rdb$field_length fld_length
        ,f.rdb$field_scale fld_scale
        ,f.rdb$computed_source as rdb_blob_id
    from rdb$relation_fields rf
    left join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
    where rf.rdb$relation_name = 'TEST';
"""

substitutions = [('[ \t]+', ' '), ('RDB_BLOB_ID.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = f"""
    FLD_NAME                        AGENT_ID
    FLD_TYPE                        8
    FLD_LENGTH                      4
    FLD_SCALE                       0
    RDB_BLOB_ID                     <null>
    FLD_NAME                        FIRST_PO_NUMBER
    FLD_TYPE                        14
    FLD_LENGTH                      8
    FLD_SCALE                       0
    RDB_BLOB_ID                     2:1e4
    (
    {COMPUTED_EXPR_1}
    )
    FLD_NAME                        AGENT_NAME
    FLD_TYPE                        37
    FLD_LENGTH                      25
    FLD_SCALE                       0
    RDB_BLOB_ID                     2:1e6
    (
    {COMPUTED_EXPR_2}
    )
    Records affected: 3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

