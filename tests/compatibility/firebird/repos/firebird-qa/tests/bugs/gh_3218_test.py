#coding:utf-8

"""
ID:          issue-3218
ISSUE:       3218
TITLE:       Optimizer fails applying stream-local predicates before merging [CORE2832]
DESCRIPTION:
    We evaluate number of records in rdb$relation_fields for which corresponding rows in rdb$relations have ID < 10.
    This value ('cnt_chk') must be equal to the number of indexed reads for this table when we run test query ('chk_sql').
    Before fix number of indexed reads in rdb$relation_fields was equal to the TOTAL number of rows in this table.
FBTEST:      bugs.gh_3218
NOTES:
    [20.01.2024] pzotov
        Confirmed problem on 5.0.0.442: number of indexed reads was equal to the total count of records in rdb$relation_fields.
    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
    [25.07.2025] pzotov
        Separated test scripts for check on versions prior/since 6.x.
        On 6.x we have to take in account indexed fields containing SCHEMA names, see below DDL for RDB$RELATION_FIELDS.
        Thanks to dimitr for suggestion.
    Checked on 6.0.0.1061; 5.0.3.1686
"""

from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

init_sql = """
    set term ^;
    create or alter procedure sp_get_relations returns(
        rdb$relation_id type of column rdb$relations.rdb$relation_id,
        rdb$relation_name type of column rdb$relations.rdb$relation_name
    ) as
    begin
        for
        select
            rdb$relation_id,
            rdb$relation_name
        from rdb$relations
        into
            rdb$relation_id,
            rdb$relation_name
        do
        begin
            suspend;
        end
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_sql)

substitutions = [('RDB\$INDEX_\\d+', 'RDB$INDEX_*'),] 
act = python_act('db', substitutions = substitutions)

#----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#----------------------------------------------------------

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = upper('rdb$relation_fields')")
        rf_rel_id = None
        for r in cur:
            rf_rel_id = r[0]
        assert rf_rel_id

        cur.execute("select count(iif(r.rdb$relation_id < 10, 1, null)) cnt_chk, count(*) as cnt_all from rdb$relation_fields rf left join rdb$relations r on rf.rdb$relation_name = r.rdb$relation_name and r.rdb$relation_id < 10")

        cnt_chk, cnt_all = cur.fetchall()[0][:2]
        
        #------------------------------------------------------

        result_map = {}
        
        test_sql_5x = """
            select 1 from sp_get_relations r
            join rdb$relation_fields f
                on f.rdb$relation_name = r.rdb$relation_name
            where r.rdb$relation_id < 10
        """


        # ALTER TABLE RDB$RELATION_FIELDS ADD CONSTRAINT RDB$INDEX_15 UNIQUE (RDB$FIELD_NAME, RDB$SCHEMA_NAME, RDB$RELATION_NAME);
        # CREATE INDEX RDB$INDEX_3 ON RDB$RELATION_FIELDS (RDB$FIELD_SOURCE_SCHEMA_NAME, RDB$FIELD_SOURCE);
        # CREATE INDEX RDB$INDEX_4 ON RDB$RELATION_FIELDS (RDB$SCHEMA_NAME, RDB$RELATION_NAME);
        test_sql_6x = """
            select 1
            from sp_get_relations r
            join rdb$relation_fields f
                on f.rdb$schema_name = upper('system')
                   and f.rdb$relation_name = r.rdb$relation_name
            where r.rdb$relation_id < 10
        """

        chk_sql = test_sql_5x if act.is_version('<6') else test_sql_6x
        ps, rs = None, None
        try:
            ps = cur.prepare(chk_sql)

            # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
            # We have to store result of cur.execute(<psInstance>) in order to
            # close it explicitly.
            # Otherwise AV can occur during Python garbage collection and this
            # causes pytest to hang on its final point.
            # Explained by hvlad, email 26.10.24 17:42
            rs = cur.execute(ps)

            tabstat1 = [ p for p in con.info.get_table_access_stats() if p.table_id == rf_rel_id ]
            cur.fetchall()
            tabstat2 = [ p for p in con.info.get_table_access_stats() if p.table_id == rf_rel_id ]
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) ) # explained plan, with preserving indents by replacing leading spaces with '#'

            idx_reads = (tabstat2[0].indexed if tabstat2[0].indexed else 0)
            if tabstat1:
                idx_reads -= (tabstat1[0].indexed if tabstat1[0].indexed else 0)

            print('Result:')
            if idx_reads == 0 or idx_reads > cnt_chk:
                print(f'POOR! Number of records in rdb$relation_fields: 1) to be filtered: {cnt_chk}, 2) total: {cnt_all}. Number of indexed_reads: {idx_reads}')
            else:
                print('Acceptable.')

        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()

    expected_stdout_5x = """
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Procedure "SP_GET_RELATIONS" as "R" Scan
        ........-> Filter
        ............-> Table "RDB$RELATION_FIELDS" as "F" Access By ID
        ................-> Bitmap
        ....................-> Index "RDB$INDEX_*" Range Scan (full match)
        Result:
        Acceptable.
    """
    expected_stdout_6x = """
        Select Expression
        ....-> Nested Loop Join (inner)
        ........-> Filter
        ............-> Procedure "PUBLIC"."SP_GET_RELATIONS" as "R" Scan
        ........-> Filter
        ............-> Table "SYSTEM"."RDB$RELATION_FIELDS" as "F" Access By ID
        ................-> Bitmap
        ....................-> Index "SYSTEM"."RDB$INDEX_*" Range Scan (full match)
        Result:
        Acceptable.
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
