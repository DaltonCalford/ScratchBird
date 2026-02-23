#coding:utf-8

"""
ID:          issue-6046
ISSUE:       6046
TITLE:       execute statement ignores the text of the SQL-query after a comment of the form "-"
DESCRIPTION:
    We concatenate query from several elements and use <CR> delimiter only to split this query into lines.
    Also, we put single-line comment in SEPARATE line between 'select' and column/value that is obtained from DB.
    Final query will lokk like this (lines are separated only by SINGLE delimiter, ascii_char(13), NO <CR> here!):
    ===
        select
        -- comment N1
        'foo' as msg'
        from
        -- comment N2
        rdb$database
    ===
    This query should NOT raise any exception and must produce normal output (string 'foo').
    Thanks to hvlad for suggestions.
JIRA:        CORE-5783
FBTEST:      bugs.core_5783
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Query line: select
    Query line:  -- comment N1
    Query line:  'foo' as msg
    Query line:  from
    Query line:  -- comment N2
    Query line:  rdb$database
    Query result: foo
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        c = con.cursor()
        sql_expr = "select \r -- comment N1 \r 'foo' as msg \r from \r -- comment N2 \r rdb$database"
        for line in sql_expr.split('\r'):
            print(f'Query line: {line}')
        c.execute(sql_expr)
        for row in c:
            print(f'Query result: {row[0]}')
    #
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
