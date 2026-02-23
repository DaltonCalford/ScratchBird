#coding:utf-8

"""
ID:          issue-6520
ISSUE:       6520
TITLE:       Efficient table scans for DBKEY-based range conditions
DESCRIPTION:
JIRA:        CORE-6278
FBTEST:      bugs.core_6278
NOTES:
    [07.05.2024] pzotov
    Test has been fully re-implemented.
    We can NOT assume that rdb$db_key values will be increased (in ASCII representation) while adding data
    into a table: smaller values of RDB$DB_KEY can appear *after* bigger ones (i.e. smaller RDB$DB_KEY will
    be physically closer to the end of table than bigger).
    Because of that, we check only EXPLAINED PLAN, without runtime statistics from trace log before.
    On build 4.0.0.1865 (07-apr-2020) explained plan for scoped query (like 'rdb$db_key between ? and ?')
    returned "Table ... Full Scan" - WITHOUT "(lower bound, upper bound)".
    Since build 4.0.0.1869 (08-apr-2020) this opewration is: "Table "TEST" Full Scan (lower bound, upper bound)".
    See commit:
    https://github.com/FirebirdSQL/firebird/commit/3ce4605e3cc9960afcf0224ea40e04f508669eca
    Checked on 5.0.1.1394, 6.0.0.345.

    [03.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214.
"""
from firebird.driver import DatabaseError

import pytest
import re
from firebird.qa import *

init_sql = f"""
    create table test (s varchar(256));
    commit;
    insert into test select lpad('', 256, uuid_to_char(gen_uuid())) from rdb$types a;
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db')

#---------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#---------------------------------------------------------

@pytest.mark.version('>=4.0.0')
def test_1(act: Action, capsys):

    scoped_expr_lst = ('rdb$db_key > ? and rdb$db_key < ?', 'rdb$db_key >= ? and rdb$db_key <= ?', 'rdb$db_key between ? and ?', 'rdb$db_key > ?', 'rdb$db_key >= ?', 'rdb$db_key < ?', 'rdb$db_key <= ?')
    with act.db.connect() as con:
        cur = con.cursor()
        for x in scoped_expr_lst:
            ps = None
            try:
                ps = cur.prepare(f'select count(s) from test where {x}')
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan .split('\n')]) )
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    act.expected_stdout = f"""
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (lower bound, upper bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (lower bound, upper bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (lower bound, upper bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (lower bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (lower bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (upper bound)

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {SQL_SCHEMA_PREFIX}"TEST" Full Scan (upper bound)
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
