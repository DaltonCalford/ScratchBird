#coding:utf-8

"""
ID:          issue-8768
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8768
TITLE:       Support bitwise aggregate functions (BIN_AND_AGG, BIN_OR_AGG, BIN_XOR_AGG)
DESCRIPTION:
    Test generates <ROWS_CNT> random integer values (within several scopes for smallint, int, int64 and int128).
    These values are added into the 'TEST' table which will be subject for applying bitwise aggregation.
    Also these values which be passed as arguments for scalar bitwise functions.
    Results of bitwise scalar and aggregate functions must be equal (see query with 'select count(*) from ...').

    Finally, we try to evaluate aggregate functions using arguments of invalid data types (blob, date etc) and
    check that raising errror will contain exactly those GDS codes that are specified in <INVALID_ARG_GDS_LIST>.

    Output of this test when it passes must be EMPTY.
NOTES:
    [21.10.2025] pzotov
    Checked on 6.0.0.1312.
"""
import random
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

ROWS_CNT = 255
INVALID_ARG_GDS_LIST = (335544569, 335544606, 336397242)

init_script = """
    recreate table test(n int128);
"""

db = db_factory(init = init_script)

act = python_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    scopes_lst = (
        ( -2**15, -1), (0,  2**15-1),
        ( -2**31, -1), (0,  2**31-1),
        ( -2**63, -1), (0,  2**63-1),
        (-2**127, -1), (0, 2**127-1),
        (     -1,  0), (0,        1),
        ( -2**15, -1), (0, 2**127-1),
    )

    with act.db.connect() as con:
        cur = con.cursor()
        ps=cur.prepare('insert into test(n) values(?) returning n')
        for scope_i in scopes_lst:
            con.execute_immediate('delete from test')
            chk_values_lst = []
            for i in range(ROWS_CNT):
                cur.execute(ps, ( random.randint(scope_i[0],scope_i[1],), ))
                n = cur.fetchone()[0]
                chk_values_lst.append(str(n))

            #print(f'{chk_values_lst=}')
            bin_and_scalar = 'bin_and(' + ','.join(chk_values_lst) + ') as bin_and_scalar'
            bin_or_scalar = 'bin_or(' + ','.join(chk_values_lst) + ') as bin_or_scalar'
            bin_xor_scalar = 'bin_xor(' + ','.join(chk_values_lst) + ') as bin_xor_scalar'

            bin_scalar_sttm = ''.join( ('select ', bin_and_scalar, ',', bin_or_scalar, ',', bin_xor_scalar, ' from rdb$database')) 
            bin_aggegate_sttm = 'select bin_and_agg(n) as bin_and_agg, bin_or_agg(n) as bin_or_agg, bin_xor_agg(n) as bin_xor_agg from test'

            check_sql = 'select count(*) from (' + bin_scalar_sttm + ' UNION DISTINCT ' + bin_aggegate_sttm + ')'
            cur.execute(check_sql)
            check_cnt = cur.fetchone()[0]

            if check_cnt ==  1:
                pass
            else:
                print('Mismatch detected between scalar and aggregate results.')
                print(f'Check output of {bin_scalar_sttm=}')
                cur.execute(bin_scalar_sttm)
                for r in cur:
                    for i,col in enumerate(cur.description):
                        print((col[0] +':').ljust(32), r[i])

                print(f'Check output of {bin_aggegate_sttm=}')
                cur.execute(bin_aggegate_sttm)
                for r in cur:
                    for i,col in enumerate(cur.description):
                        print((col[0] +':').ljust(32), r[i])
                
                break
        # < for scope_i in scopes_lst

        agg_funcs_lst = ['bin_and_agg', 'bin_or_agg', 'bin_xor_agg']
        invalid_args_lst = ["cast('foo' as blob)", 'gen_uuid()', 'true', 'current_date', 'current_time', 'pi()', 'cast(1 as decfloat(34))']
        for f in agg_funcs_lst:
            for a in invalid_args_lst:
                try:
                    invalid_arg_sttm = f'select {f}({a}) as invalid_agg from rdb$types'
                    cur.execute(invalid_arg_sttm)
                    for r in cur:
                        print(r)
                except DatabaseError as e:
                    if e.gds_codes == INVALID_ARG_GDS_LIST:
                        pass
                    else:
                        print(f'Unexpected list of GDS coded for {invalid_arg_sttm=}:')
                        print(e.__str__())
                        print(e.gds_codes)
                        print('Expected gdscodes:')
                        print(INVALID_ARG_GDS_LIST)

    expected_stdout = f"""
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
