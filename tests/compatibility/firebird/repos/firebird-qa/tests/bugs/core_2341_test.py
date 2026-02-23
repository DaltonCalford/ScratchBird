#coding:utf-8

"""
ID:          issue-2765
ISSUE:       2765
TITLE:       Hidden variables conflict with output parameters, causing assertions, unexpected errors or possibly incorrect results
DESCRIPTION:
JIRA:        CORE-2341
FBTEST:      bugs.core_2341
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_sql = """
    execute block (i varchar(10) = ?) returns (out_arg varchar(10)) as
    begin
      out_arg = coalesce(cast(out_arg as date), current_date);
      out_arg = i;
      suspend;
    end
"""
INPUT_ARG = 'QweRty'

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare(test_sql)
            rs = cur.execute(ps, (INPUT_ARG,))

            cur_cols = cur.description
            for r in cur:
                for i in range(0,len(cur_cols)):
                    print( cur_cols[i][0], ':', r[i] )
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close()
            if ps:
                ps.free()

        expected_stdout = f"""
            OUT_ARG : {INPUT_ARG}
        """
        act.expected_stdout = expected_stdout
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
