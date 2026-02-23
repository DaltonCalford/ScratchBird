#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8494
TITLE:       Fix issues with longish concatenated context aliases
DESCRIPTION:
    Test builds query with <JOIN_CNT> nested derived tables as show below (example for JOIN_CNT=3):
    =============
    select 0 as id
    from rdb$database r0                                 
    join (                                               --- join N 1
            select 1 as id
            from rdb$database r0
            join (                                       --- join N 2
                    select 2 as id
                    from rdb$database r1
                    join (                               --- join N 3
                            select 3 as id
                            from rdb$database r2
                            order by rdb$relation_id
                          ) r1 on id = id
                    where 1=1
                    order by rdb$relation_id
                 ) r2 on id = id
             where 1=1
             order by rdb$relation_id
         ) r3 on id = id
    where 1=1
    =============

    This query must have following explained plan:
    =============
    Select Expression
    ....-> Filter (preliminary)
    ........-> Nested Loop Join (inner)
    ............-> Filter
    ................-> Filter (preliminary)
    ....................-> Sort (record length: <L>, key length: <K>)
    ........................-> Nested Loop Join (inner)
    ............................-> Filter
    ................................-> Filter (preliminary)
    ....................................-> Sort (record length: <L>, key length: <K>)
    ........................................-> Nested Loop Join (inner)
    ............................................-> Sort (record length: <L>, key length: <K>)
    ................................................-> Filter
    ....................................................-> Table "SYSTEM"."RDB$DATABASE" as "R3" "R2" "R1" "R2" Full Scan --- data source N 1
    ............................................-> Table "SYSTEM"."RDB$DATABASE" as "R3" "R2" "R1" Full Scan              --- data source N 2
    ............................-> Table "SYSTEM"."RDB$DATABASE" as "R3" "R0" Full Scan                                   --- data source N 3
    ............-> Table "SYSTEM"."RDB$DATABASE" as "R0" Full Scan                                                        --- data source N 4
    =============
    Test verifies that number of lines which ends with '-> Table "SYSTEM"."RDB$DATABASE" as ... Full Scan' equals to JOIN_CNT + 1.

NOTES:
    [24.09.2025] pzotov
    1. For JOIN_CNT = 36 test must pass
    1. 6.0.0.797 (before SQL-schema introduction): test passed for JOIN_CNT = 37. When JOIN_CNT = 38 then required_pattern_found_cnt = 23
    6.0.0.1277. When JOIN_CNT = 37 then required_pattern_found_cnt = 28

    1. Max allowed value of JOINT_CNT for this test depends on whether SQL-schemas are supported by appropriate snapshot or no.
       On 6.0.0.797 (before SQL-schema introduction) test passed for JOIN_CNT = 37 and failed with JOIN_CNT = 38 (required_pattern_found_cnt = 23).
       On 6.0.0.834 (1st snapshot with SQL-schemas) test passed for JOIN_CNT = 36 and failed with JOIN_CNT = 37 (required_pattern_found_cnt = 28):
       -------------------------------------------------------------
       |                           |            build              |
       |                           |   6.0.0.797  |    6.0.0.834   |
       |---------------------------|--------------|----------------|
       |JOIN_CNT                   | 37        38 |  36        37  |
       |required_pattern_found_cnt | 38        23 |  37        28  |
       -------------------------------------------------------------
    2. For values greater than 37 explained plan is truncated and its last line looks like this:
       '...........' or  '-> Table "SYSTEM"."RDB$DATABASE" as "R37" "R36"...' etc
        (i.e. in all cases it does not finish with "Full Scan")
    3. When JOIN_CNT >=256 then test will fail with "-Too many Contexts . Maximum allowed is 256" (335544569, 335544800).

    Thanks to dimitr for providing test prototype and explanations.
    Discussed with dimitr 18.06.2025.
    Checked on 6.0.0.1042-992bccd; 6.0.0.1277-721329f
"""
import re

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

#############
JOIN_CNT = 36
#############

db = db_factory()
substitutions = [ ('record length: \\d+', 'record length: <L>'), ('key length: \\d+', 'key length: <K>') ]
act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    init_lst = ["select 0 as id from rdb$database r0"]
    tail_lst = []
    for i in range(1, JOIN_CNT+1):
        init_lst.append( f" join (select {i} as id from rdb$database r{i-1}" )
        tail_lst.append( f" order by rdb$relation_id) r{i} on id = id where 1=1" )

    test_sql = '\n'.join(init_lst) + '\n'.join(tail_lst)

    qry_map = { 0 : (test_sql, f'{JOIN_CNT=}')}
    for qry_idx,v in qry_map.items():
        qry_comment = f'{qry_idx=} ' + v[1]
        qry_map[qry_idx] = (v[0], qry_comment)

    required_pattern_found_cnt = 0
    EXPECTED_MSG = f'Expected: found {JOIN_CNT+1} lines that matching to required pattern.'

    with act.db.connect() as con:
        cur = con.cursor()
        for qry_idx, qry_data in qry_map.items():
            test_sql, qry_comment = qry_data[:2]
            ps, rs, explained_lst =  None, None, None
            try:
                cur = con.cursor()
                ps = cur.prepare(test_sql)
                explained_lst = [replace_leading(s) for s in ps.detailed_plan.split('\n')]
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

            p_table_full_scan = re.compile('->\\s+Table\\s+("SYSTEM".)?"RDB\\$DATABASE"\\s+as\\s+.*\\s+Full\\s+Scan$', re.IGNORECASE)
            if explained_lst:
                for line in explained_lst:
                    if p_table_full_scan.search(line):
                        required_pattern_found_cnt += 1

            if required_pattern_found_cnt == JOIN_CNT+1:
                print(EXPECTED_MSG)
            else:
                print(f'UNEXPECTED: {JOIN_CNT=} {required_pattern_found_cnt=}')
                print(test_sql)
                # Print explained plan with padding eash line by dots in order to see indentations:
                print( '\n'.join(explained_lst) )

            act.expected_stdout = EXPECTED_MSG
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
