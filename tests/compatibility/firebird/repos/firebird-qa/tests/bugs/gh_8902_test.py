#coding:utf-8

"""
ID:          issue-8902
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8902
TITLE:       FULL OUTER JOIN does not use indexes with the derived table
DESCRIPTION:
    Test uses DDL and data from files/standard_sample_databases.zip (file: "sample-DB_-_firebird.sql").
NOTES:
    [19.02.2026] pzotov
    Confirmed bug on 6.0.0.1454.
    Checked on 6.0.0.1458-6a76c1d.
"""
import zipfile
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()
act = python_act('db')

tmp_init_sql = temp_file('gh_8902.tmp.sql')

query_map = {
    1000 : (
              """
                select
                    p.team_leader,
                    e.emp_no
                from employee e
                full join (
                    select project.team_leader
                    from project
                ) p on e.emp_no = p.team_leader
              """
             ,'Put one of streams into derived table'
           )
}

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_init_sql: Path, capsys):
    employee_data_sql = zipfile.Path(act.files_dir / 'standard_sample_databases.zip', at='sample-DB_-_firebird.sql')
    tmp_init_sql.write_bytes(employee_data_sql.read_bytes())

    act.isql(switches = ['-q'], charset='utf8', input_file = tmp_init_sql, combine_output = True)

    if act.return_code == 0:
        pass
    else:
        # If retcode !=0 then we can print the whole output of failed gbak:
        print('Initial script failed, check output:')
        for line in act.clean_stdout.splitlines():
            print(line)
    act.reset()

    with act.db.connect() as con:
        cur = con.cursor()
        for q_idx, q_tuple in query_map.items():
            test_sql, qry_comment = q_tuple[:2]
            ps = None
            try:
                ps = cur.prepare(test_sql)
                print(q_idx)
                print(test_sql)
                print(qry_comment)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

    expected_stdout = f"""
        1000
        {query_map[1000][0]}
        {query_map[1000][1]}
        Select Expression
        ....-> Full Outer Join
        ........-> Nested Loop Join (outer)
        ............-> Table "PUBLIC"."EMPLOYEE" as "E" Full Scan
        ............-> Filter
        ................-> Table "PUBLIC"."PROJECT" as "P" "PUBLIC"."PROJECT" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."PROJECT_EMPLOYEE_FK_TEAM_LEADER" Range Scan (full match)
        ........-> Nested Loop Join (outer)
        ............-> Table "PUBLIC"."PROJECT" as "P" "PUBLIC"."PROJECT" Full Scan
        ............-> Filter
        ................-> Table "PUBLIC"."EMPLOYEE" as "E" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."EMPLOYEE_PK" Unique Scan
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
