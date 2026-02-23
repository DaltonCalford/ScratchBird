#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8891
TITLE:       Presense of DML between 'CREATE <LTT>' and 'CREATE INDEX' (for <LTT>) blocks usage of index
DESCRIPTION:
NOTES:
    [10.02.2025] pzotov
    Letter to Adriano 09.02.2026 15:39
    Confirmed problem on 6.0.0.1403.
    Checked on 6.0.0.1414-a6d9245
"""

import pytest
from firebird.qa import *

db = db_factory()

substitutions = [('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

expected_stdout = """
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    qry_map = {
        1000 : 'select id from ltt_test where id = 50'
    }

    ddl_text = """
        create local temporary table ltt_test(id int) on commit preserve rows
        ^
        commit
        ^
        insert into ltt_test(id) select n from generate_series(1, 100) as s(n)
        ^
        commit
        ^
        create index ltt_test_id on ltt_test(id)
        ^
    """

    with act.db.connect() as con:
        cur = con.cursor()

        for x in ddl_text.split('^'):
            if (s := x.strip()):
                if s.lower() == 'commit':
                    con.commit()
                else:
                    con.execute_immediate(s)
        con.commit()
        
        cur.execute("select t.mon$table_id from mon$local_temporary_tables t where t.mon$table_name = upper('ltt_test')")
        ltt_rel_id = cur.fetchone()[0]

        nat_reads = {}
        idx_reads = {}
        for k, v in qry_map.items():
            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == ltt_rel_id:
                    nat_reads[k] = -x_table.sequential if x_table.sequential is not None else 0
                    idx_reads[k] = -x_table.indexed if x_table.indexed is not None else 0

            cur.execute(v)
            cur.fetchall()
            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == ltt_rel_id:
                    nat_reads[k] += x_table.sequential if x_table.sequential is not None else 0
                    idx_reads[k] += x_table.indexed if x_table.indexed is not None else 0

            print(k, nat_reads[k], idx_reads[k])

    expected_stdout = f"""
        1000 0 1
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
