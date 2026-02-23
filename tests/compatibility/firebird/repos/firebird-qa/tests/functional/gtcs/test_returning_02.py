#coding:utf-8

"""
ID:          n/a
TITLE:       Test of RETURNING clause: UPDATE; UPDATE OR INSERT; DELETE.
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_RETURNING_2.script
NOTES:
    [21.09.2025] pzotov
    Versions 3.x and 4.x can not pass this test:
        Statement failed, SQLSTATE 21000
        multiple rows in singleton select
    See: https://github.com/FirebirdSQL/firebird/issues/6815

    Checked on 6.0.0.1277; 5.0.4.1713.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set list on;
    set term ^;

    create table tab (
      n1 integer primary key,
      n2 integer
    )^

    insert into tab values (1, 10) returning n1, n2^
    insert into tab values (2, 20) returning n1, n2^

    update tab set n2 = n2 * 10 returning n1, n2, new.n1, new.n2, old.n1, old.n2^
    select 'point-000' as msg from rdb$database^

    update tab set n2 = n2 * 10 where n1 = 0 returning n1, n2, new.n1, new.n2, old.n1, old.n2^
    select 'point-010' as msg from rdb$database^

    update tab set n2 = n2 * 10 where n1 = 2 returning n1, n2, new.n1, new.n2, old.n1, old.n2^
    select 'point-020' as msg from rdb$database^

    update or insert into tab values (2, 20000) returning n1, n2, new.n1, new.n2, old.n1, old.n2^
    select 'point-030' as msg from rdb$database^

    update or insert into tab values (3, 30000) returning n1, n2, new.n1, new.n2, old.n1, old.n2^
    select 'point-040' as msg from rdb$database^

    delete from tab returning n1, n2^
    select 'point-050' as msg from rdb$database^

    delete from tab where n1 = 1 returning n1, n2^
    select 'point-060' as msg from rdb$database^

    delete from tab where n1 = 1 returning n1, n2^
    select 'point-070' as msg from rdb$database^

    set term ;^

"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('=', ''), ('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    expected_stdout = """
        N1 1
        N2 10
        N1 2
        N2 20
        N1 1
        N2 100
        N1 1
        N2 100
        N1 1
        N2 10
        N1 2
        N2 200
        N1 2
        N2 200
        N1 2
        N2 20
        MSG point-000
        MSG point-010
        N1 2
        N2 2000
        N1 2
        N2 2000
        N1 2
        N2 200
        MSG point-020
        N1 2
        N2 20000
        N1 2
        N2 20000
        CONSTANT 2
        CONSTANT 2000
        MSG point-030
        N1 3
        N2 30000
        N1 3
        N2 30000
        CONSTANT <null>
        CONSTANT <null>
        MSG point-040
        N1 1
        N2 100
        N1 2
        N2 20000
        N1 3
        N2 30000
        MSG point-050
        MSG point-060
        MSG point-070
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
