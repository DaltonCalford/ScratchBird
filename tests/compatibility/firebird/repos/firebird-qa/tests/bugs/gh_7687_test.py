#coding:utf-8

"""
ID:          issue-7687
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7687
TITLE:       Add LEVEL column to PLG$PROF_RECORD_SOURCES and PLG$PROF_RECORD_SOURCE_STATS_VIEW
DESCRIPTION:
    Test creates master-detail tables and view 'v_test' for them.
    Then we create auxiliary view 'vp_rec_stats' to see profiler data.
    Running query to 'v_test' will add data to profiler snapshot tables so that we must have ability
    to reconstruct the whole EXPLAINED PLAN of that query, i.e. with top-level and all subsequent access paths.
    This reconstruction is performed by recursive query which first gets records with 'parent_record_source_id is null'
    (and all these records always have LEVEL = 0), and then goes down according to the principle:
    "give be lines with level that be bigger but CLOSEST to current one". This is achieved by applying dense_rank()
    function, see 'ranked_level' column usage and notes below.
NOTES:
    [25.08.2023] pzotov
    1.  We can NOT use "next level must be current one plus 1" in recursive part of query that reconstructs explained plan!
        This is because some access paths can be 'splitted' onto one or several 'sub-paths' by LF character, thus they are looked
        to be somewhat as 'multi-line' parts. Every LF in such case causes LEVEL to be "sinlently" increased by 1, so lines
        AFTER this "complex" access path will have LEVEL that differs from source for MORE than 1.

        Here is example of such data (there is NO record with LEVEL = 3):
            STTM_ID   LEVEL  REC_ID  PAR_ID ACC_PATH                                           SQL_TEXT
            ======= ======= ======= ======= ================================================== ==================================================
               5760       0       1  <null> Select Expression                                  select first 1 r.* from rdb$database r order by 1
               5760       1       2       1   -> First N Records                               select first 1 r.* from rdb$database r order by 1
               5760       2       3       2     -> Refetch
                -> Sort (record length: 36, key select first 1 r.* from rdb$database r order by 1
               5760       4       4       3         -> Table "RDB$DATABASE" as "R" Full Scan   select first 1 r.* from rdb$database r order by 1

        This was implemented intentionally with purpose to make reconstruction of indents easier.
        Because of that, view that queries PLG$PROF_ tables must use "dense_rank()over(order by t.level)".
    2.  We have to make additional (preliminary) call to rdb$profiler.start_session() / .finish_session() BEFORE test in order to create view
        that is based on profiler snapshot tables. Otherwise error raises:
            SQLSTATE = 42S02 / ... / -Table unknown / -PLG$PROF_RECORD_SOURCE_STATS_VIEW
        See: doc/sql.extensions/README.profiler.md
        "Snapshot tables (as well views and sequence) are automatically created in the first usage of the profiler"
    3.  Currently there is problem with distinguish 'blocks' from profiler data that corresponds to sub-queries: there is no column that applies
        to the whole block and lines that belong (by hierarchy) to subquery can be displayed either before or after lines that relate to 'main'
        part of query. Although it seems that subquery is mostly displayed *after* main part.
        Because of that, it was decided for now to leave expected output as it is produced by current FB versions.
        If future runs will show that subquery part "jumps" randomly (from "top" to "bottom") than only presense of top-level access paths
        will be checked (without requirement to their concrete order).
    4.  In order to see indents properly (including case if they will change in size) we have to prefix each line ('#' character is used).
    5.  Initial discussion: https://groups.google.com/g/firebird-devel/c/dWIgSIemys4/m/TzUWYwmVAQAJ?pli=1

    Checked on 5.0.0.1169.

    [29.09.2023] pzotov
    Replaces expected output to be matched to current FB 5.x and 6.x snapshots.
    Added substitution to disable output of BLOB_ID values because access_path type is BLOB since 19-sep-2023
    (see https://github.com/FirebirdSQL/firebird/commit/39b019574a7eb23eff92ee71121043d9a9c8371f )

    [19-dec-2023] pzotov
    Removed 'rand()' in order to have predictable values in table column. Use mod() instead.
    Unstable outcomes started since 6.0.0.180 (18.12.2023).
    It seems that following commits caused this:
        https://github.com/FirebirdSQL/firebird/commit/ae427762d5a3e740b69c7239acb9e2383bc9ca83 // 5.x
        https://github.com/FirebirdSQL/firebird/commit/f647dfd757de3c4065ef2b875c95d19311bb9691 // 6.x

    [03.07.2025] pzotov
    Adjusted for FB 6.x: it is MANDATORY to specify schema `PLG$PROFILER.` when querying created profiler tables.
    See doc/sql.extensions/README.schemas.md, section title: '### gbak'; see 'SQL_SCHEMA_PREFIX' variable here.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.970; 5.0.3.1668.
"""

import os
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions=[('=', ''), ('ACCESS_PATH_BLOB_ID.*', '')])

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PLG$PROFILER.'
    test_sql = f"""
        recreate table tdetl(id int);
        recreate table tmain(id int primary key using index tmain_pk, x int);
        recreate table tdetl(id int primary key using index tdetl_pk, pid int references tmain using index tdetl_fk, y int, z int);

        insert into tmain(id,x)
        select i, -100 + mod(i,200) from (select row_number()over() i from rdb$types rows 200);

        insert into tdetl(id, pid, y,z)
        select i, 1+mod(i,10), mod(i,30), mod(i,70) from (select row_number()over() i from rdb$types,rdb$types rows 1000);
        commit;

        create index tmain_x on tmain(x);
        create index tdetl_y on tdetl(y);
        create index tdetl_z on tdetl(z);

        set statistics index tdetl_fk;
        commit;
        
        out {os.devnull};
        -- This is needed in order to create view based on snapshot 'plg$prof_*' tables:
        select rdb$profiler.start_session('profile session 0') from rdb$database;
        out;

        create view vp_rec_stats as
        select
            cast(t.statement_id as smallint) as sttm_id
           ,cast(t.level as smallint) as level
           ,cast(t.record_source_id as smallint) as  rec_id
           ,cast(t.parent_record_source_id as smallint) as par_id
           ,substring('#' || lpad('', 4*t.level,' ') || replace( replace(t.access_path, ascii_char(13), ''), ascii_char(10), ascii_char(10) || '#' || lpad('', 4*t.level,' ') ) from 1 for 320) as acc_path
           ,substring( cast(t.sql_text as varchar(255)) from 1 for 50 ) as sql_text
           ,dense_rank()over(order by t.level) as ranked_level
        from {SQL_SCHEMA_PREFIX}plg$prof_record_source_stats_view t
        join {SQL_SCHEMA_PREFIX}plg$prof_sessions s
            on s.profile_id = t.profile_id and
               s.description = 'profile session 1'
        ;
        execute procedure rdb$profiler.finish_session(true);
        commit;


        create view v_test as 
        select m4.id, d4.y, d4.z
        from tmain m4
        cross join lateral (
            select y, z
            from tdetl dx
            where
                dx.pid = m4.id
                and m4.x between dx.y and dx.z
        ) d4
        where exists(select count(*) from tdetl dy group by dy.pid having count(*) > 2);

        out {os.devnull};
        select rdb$profiler.start_session('profile session 1') from rdb$database;
        -- Test query for which we want to see data in the profiler tables:
        -- ##########
        select * from v_test;
        out;
        execute procedure rdb$profiler.finish_session(true);
        commit;

        set transaction read committed;
        set count on;
        set list on;

        -- ##############################################
        -- Output data from profiler.
        -- NB-1: each line is prefixed with '#' character in order to see indents.
        -- NB-2: 'level' column can not be used for joining in recursive part because of 'gaps'!
        --       Instead, 'ranked_level' is used fo this:
        -- ##############################################
        with recursive
        r as (
            select sttm_id, level, t.ranked_level, rec_id, par_id, acc_path --, sql_text
            from vp_rec_stats t
            where 
               t.par_id is null and t.sql_text not containing 'rdb$profiler.start_session'
            
            UNION ALL

            select t.sttm_id, t.level, t.ranked_level, t.rec_id, t.par_id, t.acc_path --, t.sql_text
            from vp_rec_stats t
            join r on t.sttm_id = r.sttm_id and t.ranked_level = r.ranked_level  + 1 and r.rec_id = t.par_id
        )
        select acc_path as access_path_blob_id from r;
    """

    expected_stdout_5x = f"""
        #Select Expression
        #    -> Filter (preliminary)
        #        -> Nested Loop Join (inner)
        #            -> Table "TMAIN" as "V_TEST M4" Full Scan
        #            -> Filter
        #                -> Table "TDETL" as "V_TEST D4 DX" Access By ID
        #                    -> Bitmap And
        #                        -> Bitmap And
        #                            -> Bitmap
        #                                -> Index "TDETL_Z" Range Scan (lower bound: 1/1)
        #                            -> Bitmap
        #
        #Sub-query (invariant)
        #    -> Filter
        #        -> Aggregate
        #            -> Table "TDETL" as "V_TEST DY" Access By ID
        #                -> Index "TDETL_FK" Full Scan
        Records affected: 10
    """

    expected_stdout_6x = f"""
        #Select Expression
        #    -> Filter (preliminary)
        #        -> Nested Loop Join (inner)
        #            -> Table "PUBLIC"."TMAIN" as "PUBLIC"."V_TEST" "M4" Full Scan
        #            -> Filter
        #                -> Table "PUBLIC"."TDETL" as "PUBLIC"."V_TEST" "D4" "DX" Access By ID
        #                    -> Bitmap And
        #                        -> Bitmap And
        #                            -> Bitmap
        #                                -> Index "PUBLIC"."TDETL_Z" Range Scan (lower bound: 1/1)
        #
        #Sub-query (invariant)
        #    -> Filter
        #        -> Aggregate
        #            -> Table "PUBLIC"."TDETL" as "PUBLIC"."V_TEST" "DY" Access By ID
        #                -> Index "PUBLIC"."TDETL_FK" Full Scan
        Records affected: 10
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
