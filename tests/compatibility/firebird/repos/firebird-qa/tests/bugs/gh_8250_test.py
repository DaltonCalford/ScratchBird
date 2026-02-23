#coding:utf-8

"""
ID:          8250
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8250
TITLE:       Bad performance on simple two joins query on tables with composed index
DESCRIPTION:
NOTES:
    [25.09.2024] pzotov
    0. Tables in explained plans are specified in the same order for snapshots before and after fix.
       Difference can be seen only in added "keys: <N>" clause.
       This clause currently exists only in FB 6.x, commit:
       https://github.com/FirebirdSQL/firebird/commit/c50b0aa652014ce3610a1890017c9dd436388c43
       Because of this, test min_version = 6.0.
    1. Improvement can be checked by parsing of explained plan if its top-level hash join looks like
       "Hash Join (inner) (keys: N, total key length: ...)" and value of <N> is NOT LESS THAN some 
       minimal threshold defined by variable MIN_REQUIRED_KEYS_IN_TOP_LEVEL_HJ.
       As explained by dimitr, for this test such value must be 5 (letter 24.09.2024 07:22).
    2. Tables must have indices (defined by PK and FK constraints) for explained plan to show 'keys: N'
       with N > 1. Otherwise top-level HJ line will be 'keys: 1' and query will run as slow as it was
       specified in the ticket. The reason is that in case of missed indices optimizer uses other criteria
       for estimation of cost and plan will remain ineffective (at least for current FB 6.x).
       Explained by dimitr, letter 24.09.2024 12:29.

    Confirmed problem on 6.0.0.461.
    Checked on 6.0.0.467.
"""

import re
import time
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

MIN_REQUIRED_KEYS_IN_TOP_LEVEL_HJ = 5
ROWS_LIMIT_FOR_CHILD_TABLES = 5000

init_sql = f"""
    create table test1(id1 int not null, id2 int not null, name varchar(30) not null);
    create table test2(id1 int not null, id2 int not null, code30 varchar(36) not null, descr varchar(36));
    create table test3(id1 int not null, id2 int not null, code30 varchar(36) not null, code15 varchar(15) not null, price double precision);

    set term ^;
    execute block as
        declare n_cnt int = {ROWS_LIMIT_FOR_CHILD_TABLES};
        declare i int;
        declare v_code30 type of column test2.code30;
    begin
        insert into test1(id1, id2, name) select 1, i, left(uuid_to_char(gen_uuid()), 30) from (select row_number()over()-1 i from rdb$types rows 16);
        i = 0;
        while (i < n_cnt) do
        begin
            v_code30 = lpad('', 36, uuid_to_char(gen_uuid()));
            insert into test2(id1, id2, code30, descr) values( 1, mod(:i, 4), :v_code30, :v_code30);
            insert into test3(id1, id2, code30, code15, price) values( 1, mod(:i, 4) , :v_code30, left(uuid_to_char(gen_uuid()), 15), round(1000));
            i = i + 1;
        end

    end
    ^
    set term ;^
    commit;
    set echo on;
    alter table test1 add constraint t1_pk primary key(id1,id2);
    alter table test2 add constraint t2_pk primary key(id1,id2,code30);
    alter table test3 add constraint t3_pk primary key(id1,id2,code30,code15);
"""
db = db_factory(init = init_sql, page_size = 8192)

substitutions =  [  ('[ \t]+', ' ')
                   ,('keys: \\d+, total key length: \\d+', 'keys, total key length')
                   ,('record length: \\d+', 'record length')
                 ]

act = python_act('db', substitutions = substitutions)


#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    test_sql = """
        select 1 x
        from test2 t2
        join test3 t3 on t3.id1 = t2.id1 and t3.id2 = t2.id2 and t3.code30 = t2.code30
        join test1 t1 on t1.id1 = t2.id1 and t1.id2 = t2.id2
    """

    # 'Hash Join (inner) (keys: 3, total key length: 38)'
    p_hj_keys = re.compile(r'Hash Join \(inner\) \(keys: \d+', re.IGNORECASE)
    top_level_keys_found = -1
    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            for s in ps.detailed_plan.split('\n'):
                if (pm := p_hj_keys.search(s)):
                    top_level_keys_found = max(top_level_keys_found, int(pm.group().split()[-1]))
                    break
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    max_keys_msg = 'Top-level hash join keys: '
    expected_keys = ''
    if top_level_keys_found >= MIN_REQUIRED_KEYS_IN_TOP_LEVEL_HJ:
        max_keys_msg += 'EXPECTED'
        expected_keys = max_keys_msg
    else:
        max_keys_msg += f'UNEXPECTED, {" too low: "+str(top_level_keys_found)+" - less than "+str(MIN_REQUIRED_KEYS_IN_TOP_LEVEL_HJ) if top_level_keys_found > 0 else "NOT FOUND"}'

    print(max_keys_msg)

    expected_stdout_5x = f"""
        Select Expression
        ....-> Filter
        ........-> Hash Join (inner) (keys, total key length)
        ............-> Hash Join (inner) (keys, total key length)
        ................-> Table "TEST3" as "T3" Full Scan
        ................-> Record Buffer (record length)
        ....................-> Table "TEST1" as "T1" Full Scan
        ............-> Record Buffer (record length)
        ................-> Table "TEST2" as "T2" Full Scan
        {expected_keys}
    """

    expected_stdout_6x = f"""
        Select Expression
        ....-> Filter
        ........-> Hash Join (inner) (keys, total key length)
        ............-> Hash Join (inner) (keys, total key length)
        ................-> Table "PUBLIC"."TEST3" as "T3" Full Scan
        ................-> Record Buffer (record length)
        ....................-> Table "PUBLIC"."TEST1" as "T1" Full Scan
        ............-> Record Buffer (record length)
        ................-> Table "PUBLIC"."TEST2" as "T2" Full Scan
        {expected_keys}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
