#coding:utf-8

"""
ID:          issue-4314
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4314
TITLE:       Sub-optimal predicate checking while selecting from a view [CORE3981]
DESCRIPTION:
NOTES:
    [20.08.2024] pzotov
        Checked on 6.0.0.438, 5.0.2.1479, 4.0.6.3142, 3.0.12.33784.
    [26.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.

"""
from firebird.driver import DatabaseError

import locale
import re

import pytest
from firebird.qa import *

init_sql = """
    recreate table rr(
       rel_name varchar(63)
       ,id int
       ,fid int
    );

    recreate table rf(
       rel_name varchar(63)
       ,fid int
       ,fnm varchar(63)
    );
    insert into rr
    select a.rdb$relation_name, a.rdb$relation_id, a.rdb$field_id
    from rdb$relations a
    ;
    insert into rf select f.rdb$relation_name, f.rdb$field_id, f.rdb$field_name
    from rdb$relation_fields f
    ;
    commit;

    alter table rr add constraint rr_rel_name_unq unique (rel_name);
    create index rr_id on rr (id);

    alter table rf add constraint rf_fnm_rel_name_unq unique(fnm, rel_name);
    create index rf_rel_name on rf(rel_name);

    recreate view v as
    select r.rel_name, abs(r.id) as id
    from rr r
    left 
    join rf on r.rel_name = rf.rel_name and r.fid = rf.fid
    where r.id < 128;

    set statistics index rr_rel_name_unq;
    set statistics index rr_id;
    set statistics index rf_fnm_rel_name_unq;
    set statistics index rf_rel_name;
    commit;
"""

db = db_factory(init = init_sql)
act = python_act('db')

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    query_from_view = """
        select /* trace_tag: VIEW */ v.rel_name as v_rel_name, v.id as v_id from v
        where id = 0
    """
    query_from_dt = """
        select /* trace_tag: DERIVED TABLE */ d.rel_name as d_rel_name, d.id as d_id
        from (
            select r.rel_name, abs(r.id) as id
            from rr r
            left 
            join rf on r.rel_name = rf.rel_name and r.fid = rf.fid
            where r.id < 128
        ) d
        where d.id = 0
    """

    with act.db.connect() as con:
        cur = con.cursor()
        for test_sql in (query_from_view, query_from_dt):
            ps = None
            try:
                ps = cur.prepare(test_sql)
                print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

    expected_stdout_5x = """
        Select Expression
        ....-> Filter
        ........-> Nested Loop Join (outer)
        ............-> Filter
        ................-> Table "RR" as "V R" Access By ID
        ....................-> Bitmap
        ........................-> Index "RR_ID" Range Scan (upper bound: 1/1)
        ............-> Filter
        ................-> Table "RF" as "V RF" Access By ID
        ....................-> Bitmap
        ........................-> Index "RF_REL_NAME" Range Scan (full match)

        Select Expression
        ....-> Filter
        ........-> Nested Loop Join (outer)
        ............-> Filter
        ................-> Table "RR" as "D R" Access By ID
        ....................-> Bitmap
        ........................-> Index "RR_ID" Range Scan (upper bound: 1/1)
        ............-> Filter
        ................-> Table "RF" as "D RF" Access By ID
        ....................-> Bitmap
        ........................-> Index "RF_REL_NAME" Range Scan (full match)
    """

    expected_stdout_6x = """
        Select Expression
        ....-> Filter
        ........-> Nested Loop Join (outer)
        ............-> Filter
        ................-> Table "PUBLIC"."RR" as "PUBLIC"."V" "R" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."RR_ID" Range Scan (upper bound: 1/1)
        ............-> Filter
        ................-> Table "PUBLIC"."RF" as "PUBLIC"."V" "PUBLIC"."RF" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."RF_REL_NAME" Range Scan (full match)
        Select Expression
        ....-> Filter
        ........-> Nested Loop Join (outer)
        ............-> Filter
        ................-> Table "PUBLIC"."RR" as "D" "R" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."RR_ID" Range Scan (upper bound: 1/1)
        ............-> Filter
        ................-> Table "PUBLIC"."RF" as "D" "PUBLIC"."RF" Access By ID
        ....................-> Bitmap
        ........................-> Index "PUBLIC"."RF_REL_NAME" Range Scan (full match)
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
