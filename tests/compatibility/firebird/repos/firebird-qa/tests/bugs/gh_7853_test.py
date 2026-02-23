#coding:utf-8

"""
ID:          issue-7853
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7853
TITLE:       Do not consider non-deterministic expressions as invariants in pre-filters
DESCRIPTION:
    Change in FB 5.x was pushed 14.12.2023 20:06:
    https://github.com/FirebirdSQL/firebird/commit/f0b7429a1873ed9470838da61bdca0bce748652d
    Functions gen_id(), rand(), gen_uuid(), rdb$set_context and some other are not considered as deterministic anymore.
    It means that explained plans for queries which used these functions will change: now they must NOT contain 'Filter (preliminary)'.
    This behavior changed since snapshot 5.0.0.1304 (date: 16-dec-2023).
NOTES:
    [17.12.2023] pzotov
        1. Explained plans for queries where 'tmain' have aliases 'm0'  ... 'm4' have:
           * 'Filter (preliminary)' - for snapshots before this fix
           * 'Filter' (WITHOUT preliminary) - after fix
        2. Last commit in the push is "deterministic uncorrelated subqueries to be considered as invariants" - but there is NO DIFFERENCE
           between explained plans for 5.x snapshots that belongs to time points just _before_ and _after_ this push.
           This is because push of PR#7853 has several commits, and one of them did broke such functionality whereas next
           commit - WITHIN THIS PUSH (!) - did restore previous state (letter by dimitr, 17.12.2023 21:17).

        3. Actually, NON-correlated subqueries became considered as INVARIANT much earlier, since 5.0.0.890, 10-jan-2023
           ("Merge pull request #7441 from FirebirdSQL/v5-pretty-explained-plan", a6ce0ec1632ec037b41b9cbcad42fd3ce6a9ea5e).
           It seems that this old commit (of 10-jan-2023) caused lot of old issues to be considered now as fixed, for example 
           https://github.com/FirebirdSQL/firebird/issues/3394
           Query where 'tmain' table has alias 'm4' (and 'tdetl' table is involved - in contrary to all other queries) - is from this ticket.
           This query explained plan must have "Sub-query (invariant)" since build 5.0.0.890

    [05.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = """
    set bail on;
    recreate sequence g;
    recreate table tmain(id int primary key using index tmain_pk, f01 int);
    recreate table tdetl(id int primary key using index tdetl_pk, pid int references tmain(id) using index tdetl_fk, f01 int);

    insert into tmain(id, f01) select row_number()over(), rand()* 1000 from rdb$types rows 100;
    insert into tdetl(id, pid, f01) select row_number()over(), 1+rand() * 49, rand()* 1000 from rdb$types,rdb$types rows 1000;
    commit;
    set statistics index tmain_pk;
    set statistics index tdetl_fk;
    commit;
"""

db = db_factory(init = init_sql)

query_lst = (
     "select count(*) from tmain m0 where gen_id(g,0) = 0"
    ,"select count(*) from tmain m1 where rand() > 0.5"
    ,"select count(*) from tmain m2 where gen_uuid() is not null"
    ,"select count(*) from tmain m3 where coalesce( rdb$set_context('USER_TRANSACTION', 'ROWS_COUNTER',cast(coalesce(rdb$get_context('USER_TRANSACTION', 'ROWS_COUNTER'),'0') as int) + 1 ), 0) >= 0"
    ,"select count(*) from tmain m4 where m4.f01 > ( select avg(f01) from tdetl d )"
)

act = python_act('db')

#---------------------------------------------------------
def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped
#---------------------------------------------------------
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        for q in query_lst:
            ps = None
            try:
                ps = cur.prepare(q)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan .split('\n')]) )
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TMAIN_NAME = '"TMAIN"' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TMAIN"'
    TABLE_TDETL_NAME = '"TDETL"' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TDETL"'
    expected_stdout = f"""
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {TABLE_TMAIN_NAME} as "M0" Full Scan

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {TABLE_TMAIN_NAME} as "M1" Full Scan

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {TABLE_TMAIN_NAME} as "M2" Full Scan

        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {TABLE_TMAIN_NAME} as "M3" Full Scan

        Sub-query (invariant)
        ....-> Singularity Check
        ........-> Aggregate
        ............-> Table {TABLE_TDETL_NAME} as "D" Full Scan
        Select Expression
        ....-> Aggregate
        ........-> Filter
        ............-> Table {TABLE_TMAIN_NAME} as "M4" Full Scan
    """
    
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
