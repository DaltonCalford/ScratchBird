#coding:utf-8

"""
ID:          n/a
TITLE:       CREATE INDEX: check all cases
DESCRIPTION:
    Check ability to create indices for all permitted cases and inability to do that for computed/blob/array columns.
    Content of RDB$ tables is verified in order to see data for just created index INSTEAD of usage 'SHOW' command.
    View 'v_index_info' is used to show all data related to indices.
    Its DDL differs for FB versions prior/ since 6.x (columns related to SQL schemas present for 6.x).
NOTES:
    [11.07.2025] pzotov
    This test replaces previously created ones with names: test_01.py ... test_10.py
    All these tests has been marked to be SKIPPED from execution.
    Checked on Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.

    [26.08.2025] pzotov
    Re-implemented after note by Antovn Zuev, Redbase.
    Changed names of indices (removed duplicates that were result of copy-paste).
    An ability to create index and make it INACTIVE (within one 'CREATE INDEX' statement) currently presents only 
    in FB 6.x (i.e. it was not backported), see :
        https://github.com/FirebirdSQL/firebird/issues/6233
        https://github.com/FirebirdSQL/firebird/pull/8091
    Added statements that must fail on every checked FB version.
    Checked on 6.0.0.1244; 5.0.4.1701; 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory(page_size = 8192)
tmp_user = user_factory('db', name='tmp_indices_creator', password='123')

substitutions = [('[ \t]+', ' '), ('BLOB_ID_.*', 'BLOB_ID'), ('(-)?At block line(:)?\\s+\\d+.*', '')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=4')
def test_1(act: Action, tmp_user: User):

    # RDB$INDICES:
    # 6.x:
    # constraint rdb$index_5 unique (rdb$schema_name, rdb$index_name);
    # index rdb$index_31 on rdb$indices (rdb$schema_name, rdb$relation_name);
    # index rdb$index_41 on rdb$indices (rdb$foreign_key_schema_name, rdb$foreign_key);

    # RDB$INDEX_SEGMENTS:
    # 6.x:
    # index rdb$index_6 on rdb$index_segments (rdb$schema_name, rdb$index_name);

    # RDB$RELATION_CONSTRAINTS:
    # 3.x ... 5.x:
    # index rdb$index_42 ... (rdb$relation_name, rdb$constraint_type);
    # index rdb$index_43 ... (rdb$index_name);
    # 6.x:
    # constraint rdb$index_12 unique (rdb$schema_name, rdb$constraint_name);
    # index rdb$index_42 ... (rdb$schema_name, rdb$relation_name, rdb$constraint_type);
    # index rdb$index_43 ... (rdb$schema_name, rdb$index_name);

    IDX_COND_SOURCE = '' if act.is_version('<5') else ',ri.rdb$condition_source as blob_id_idx_cond_source'
    SQL_SCHEMA_IDX = '' if act.is_version('<6') else ',ri.rdb$schema_name as ri_idx_schema_name'
    SQL_SCHEMA_FKEY = '' if act.is_version('<6') else ',ri.rdb$schema_name as ri_fk_schema_name'

    RINDX_RSEGM_JOIN_EXPR = 'ri.rdb$index_name = rs.rdb$index_name' + ('' if act.is_version('<6') else ' and ri.rdb$schema_name = rs.rdb$schema_name' )
    RINDX_RCNTR_JOIN_EXPR = 'ri.rdb$index_name = rc.rdb$index_name and ri.rdb$relation_name = rc.rdb$relation_name' + ('' if act.is_version('<6') else ' and ri.rdb$schema_name = rc.rdb$schema_name' )

    test_script = f"""
        set list on;
        set count on;
        set blob all;
        create view v_index_info as
        select
             ri.rdb$index_id as ri_idx_id
            ,ri.rdb$index_name as ri_idx_name
            ,ri.rdb$relation_name as ri_rel_name
            ,coalesce(cast(ri.rdb$unique_flag as varchar(1)), 'N.U.L.L - ERROR ?!') ri_idx_uniq
            ,ri.rdb$segment_count as ri_idx_segm_count
            ,coalesce(cast(ri.rdb$index_inactive as varchar(1)), 'N.U.L.L - ERROR ?!') as ri_idx_inactive
            ,ri.rdb$index_type as ri_idx_type
            ,ri.rdb$foreign_key as ri_idx_fkey
            ,ri.rdb$expression_source as blob_id_idx_expr
            ,ri.rdb$description as blob_id_idx_descr
            -- 5.x
            {IDX_COND_SOURCE}
            --  6.x
            {SQL_SCHEMA_IDX}
            {SQL_SCHEMA_FKEY}
            ,rs.rdb$field_name as rs_fld_name
            ,rs.rdb$field_position as rs_fld_pos
            ,rc.rdb$constraint_name as rc_constraint_name
            ,rc.rdb$constraint_type as rc_constraint_type
        from rdb$indices ri
        LEFT -- ::: NB: 'rdb$index_segments' has no records for COMPUTED-BY indices.
             join rdb$index_segments rs
             on {RINDX_RSEGM_JOIN_EXPR}
        left join rdb$relation_constraints rc
             on {RINDX_RCNTR_JOIN_EXPR}
        where coalesce(ri.rdb$system_flag,0) = 0 and ri.rdb$index_name starting with 'TEST_'
        order by ri.rdb$relation_name, ri.rdb$index_name, rs.rdb$field_position
        ;    
        commit;
        grant select on v_index_info to {tmp_user.name};
        commit;

        grant create table to user {tmp_user.name};
        grant alter any table to user {tmp_user.name};
        grant drop any table to user {tmp_user.name};
        commit;

        connect '{act.db.dsn}' user '{tmp_user.name}' password '{tmp_user.password}';

        -- create using simplest form:
        create table test(f01 int);
        create index test_f01_simplest on test(f01);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        create table test(f02 int);
        create unique index test_f02_unq on test(f02);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to use 'asc' keyword:
        create table test(f03 int);
        create asc index test_f03_asc on test(f03);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to use 'ascending' keyword:
        create table test(f04 int);
        create ascending index test_f04_ascending on test(f04);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to use 'desc' keyword:
        create table test(f05 int);
        create desc index test_f05_desc on test(f05);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to use 'descending' keyword:
        create table test(f06 int);
        create descending index test_f06_descending on test(f06);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to create multi-column index, asc (NB: max 16 columns can be specified):
        create table test(g01 int, g02 int, g03 int, g04 int, g05 int, g06 int, g07 int, g08 int, g09 int, g10 int, g11 int, g12 int, g13 int, g14 int, g15 int, g16 int);
        create index test_07_compound_asc on test(g01, g02, g03, g04, g05, g06, g07, g08, g09, g10, g11, g12, g13, g14, g15, g16);
        commit;
        select * from v_index_info;
        commit;
        drop index test_07_compound_asc;
        ---------------------------------
        -- check ability to create multi-column index, desc (NB: max 16 columns can be specified):
        create descending index test_08_compound_dec on test(g01, g02, g03, g04, g05, g06, g07, g08, g09, g10, g11, g12, g13, g14, g15, g16);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to create computed index, asc
        create table test(f09 int);
        create index test_f09_computed on test computed by (f09 * f09);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to create computed index, unique and desc
        create table test(f10 int);
        create unique descending index test_f10_computed on test computed by (f10 * f10);
        commit;
        select * from v_index_info;
        drop table test;
        ---------------------------------
        -- check ability to foreign key that refers to the PK from the same table (get index info tfor such FK)
        create table test(id int primary key using index test_pk, pid int references test using index test_fk_11);
        commit;
        select * from v_index_info;
        drop table test;
    """

    #####################################
    # 5.x: add checks for PARTIAL indices
    #####################################
    test_script_5x = f"""
        create table test(k01 int);
        create index test_k01_partial on test(k01) where k01 = 1 or k01 = 2 or k01 is null;
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        create table test(k02 int);
        create unique descending index test_k02_partial_unq_desc on test(k02) where k02 = 1 or k02 = 2 or k02 is null;
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        create table test(k04 int, dt date);
        create descending index test_k04_partial_computed on test computed by (k04 * k04) where dt = current_date;
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        create table test(k05 int, dt date);
        create descending index test_k05_partial_computed on test computed by (k05 * k05) where dt = (select max(dt) from test);
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        create table test(k06 int, dt computed by ( dateadd(k06 day to date '01.01.1970') ) );
        create descending index test_k06_partial_computed on test computed by (k06 * k06) where dt = (select max(dt) from test);
        commit;
        select * from v_index_info;
        drop table test;
    """

    #############################################
    # 6.x: check ability to create INACTIVE index
    # ::: NB :::
    # This currently can be done only in 6.x, see:
    # https://github.com/FirebirdSQL/firebird/issues/6233 (create index idx [as active | inactive] on ... [CORE5981])
    # https://github.com/FirebirdSQL/firebird/issues/8090 (Extracting of inactive index)
    # https://github.com/FirebirdSQL/firebird/pull/8091 (Ability to create an inactive index)
    ############################################
    test_script_6x = f"""
        -- check ability to create inactive index: simle case
        create table test(i01 int);
        create index test_i01_inactive inactive on test(i01);
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check ability to create inactive index:  unique + desc + computed by:
        create table test(i02 int);
        create unique descending index test_i02_inactive inactive on test computed by(i02 * i02);
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check ability to create inactive index:  unique + desc + partial:
        create table test(i03 int, dt date);
        create unique descending index test_i03_partial_inactive inactive on test(i03) where dt = current_date;
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check ability to create inactive index:  unique + desc + computed by + partial:
        create table test(i04 int, dt computed by ( dateadd(i04 day to date '01.01.2020') ) );
        create unique descending index test_i04_partial_computed_inactive inactive on test computed by (i04 * i04) where dt = (select min(dt) from test);
        commit;
        select * from v_index_info;
        drop table test;
    """
    # TODO LATER: add checks for "[schema-name.]" prefix, 6.x.

    ####################################################

    if act.is_version('<5'):
        pass
    else:
        test_script += test_script_5x

    if act.is_version('<6'):
        pass
    else:
        test_script += test_script_6x


    ######################################################
    ###   A C T I O N S    C A U S I N  G    F A I L   ###
    ######################################################
    test_script += """
        -- test that we can *not* create compound index with 17+ columns:
        create table test(
            h_000 smallint,
            h_001 smallint,
            h_002 smallint,
            h_003 smallint,
            h_004 smallint,
            h_005 smallint,
            h_006 smallint,
            h_007 smallint,
            h_008 smallint,
            h_009 smallint,
            h_010 smallint,
            h_011 smallint,
            h_012 smallint,
            h_013 smallint,
            h_014 smallint,
            h_015 smallint,
            h_016 smallint
        );
        create index test_NOT_ALLOWED_01 on test(
            h_000,
            h_001,
            h_002,
            h_003,
            h_004,
            h_005,
            h_006,
            h_007,
            h_008,
            h_009,
            h_010,
            h_011,
            h_012,
            h_013,
            h_014,
            h_015,
            h_016
        );
        commit;
        select * from v_index_info; -- no rows must be shown
        drop table test;
        ---------------------------------
        -- check that we can *not* create index on computed field
        create table test(id int, h02 computed by (id * id) );
        create index test_NOT_ALLOWED_02 on test( h02 );
        commit;
        select * from v_index_info; -- no rows must be shown
        drop table test;
        ---------------------------------
        -- check that we can *not* create index on blob field
        create table test(id int, h03 blob);
        create index test_NOT_ALLOWED_03 on test( h03 );
        commit;
        select * from v_index_info; -- no rows must be shown
        drop table test;
        ---------------------------------
        -- check that we can *not* create index on array field
        create table test(id int, h04 int[3,4]);
        create index test_NOT_ALLOWED_04 on test( h04 );
        commit;
        select * from v_index_info; -- no rows must be shown
        drop table test;
        ---------------------------------
        -- check that we can *not* create index if some column is duplicated
        -- ("Field ... cannot be used twice in index ..."):
        create table test(id int, h05 int);
        create index test_NOT_ALLOWED_05 on test( id, h05, id );
        commit;
        select * from v_index_info; -- no rows must be shown
        drop table test;
        -----------------------------------
        -- check that we can *not* create foreign key which has different datatype than column of PK:
        -- ("partner index segment no 1 has incompatible data type")
        -- ::: NB ::: NO table with name 'test' with any index must exist before this statement,
        -- See  https://github.com/FirebirdSQL/firebird/issues/8714
        create table test(id bigint primary key using index test_pk, pid int);
        alter table test add constraint test_NOT_ALLOWED_06 foreign key(pid) references test(id);
        -- Only index for PK must exists now:
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check that we can *not* create FK with smaller number of columns comparing to columns in appropriate PK/UK:
        -- ("could not find UNIQUE or PRIMARY KEY constraint in table ... with specified columns")
        create table test(x int, y int, z int, constraint test_x_y_z primary key(x,y,z), u int, v int);
        alter table test add constraint test_NOT_ALLOWED_07 foreign key(u,v) references test(x,y);
        commit;
        -- Only index for PK must exists now:
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check that we can *not* create computed index if evaluation error occurs for some values:
        -- ("Expression evaluation error for index ...")
        create table test(id int);
        set count off;
        insert into test(id) values(-4);
        set count on;
        commit;
        create index test_NOT_ALLOWED_08 on test computed by( sqrt(id) );
        commit;
        select * from v_index_info;
        drop table test;
        -----------------------------------
        -- check that we can not create too large number of indices for some table.
        -- NB: limit depends on page_size: 8K = 255; 16K = 511; 32K = 1023; formula: power(2, ( 8 + log(2, pg_size/8) ) - 1)
        -- ("cannot add index, index root page is full"):
        create table test(id int, h06 int);
        commit;
        --- set autoddl off;
        set transaction read committed;
        set term ^;
        execute block as
            declare i int;
            declare n int;
            declare pg_size int;
        begin
            i = 1;
            
            -- select mon$page_size from mon$database into pg_size;
            -- if ( rdb$get_context('SYSTEM', 'ENGINE_VERSION') >= '5.' ) then
            --     n = 1 + power( 2, ( 8 + log(2, pg_size/8192) ) ); -- 256; 512; 1024; ...
            -- else
            --     n = 1 + decode(pg_size,  8192, 408,   16384, 818,   32768, 1637,   9999);

            -- COULD NOT get proper formula too define max allowed indices per a table.
            -- Hope that this is greater than actual limit for currently used page sizes:
            n = 9999;
            while (i <= n) do
            begin
                execute statement 'create index test_' || i || ' on test(h06)'
                with autonomous transaction
                ;
                i = i + 1;
            end
        end
        ^
        set term ;^
        --- set autoddl on;
        commit;
        --set plan on;
        select count(*) as max_number_of_created_indices from v_index_info;
        rollback; -- !! see https://github.com/FirebirdSQL/firebird/issues/8714#issuecomment-3224128813
        drop table test;
    """

    #~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    expected_stdout_4x = """
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F01_SIMPLEST
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F02_UNQ
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F02
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F03_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F03
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F04_ASCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F04
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F05_DESC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F05
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F06_DESCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F06
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F09_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        (f09 * f09)
        BLOB_ID
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F10_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (f10 * f10)
        BLOB_ID
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        RI_IDX_ID 2
        RI_IDX_NAME TEST_FK_11
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY TEST_PK
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME PID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_3
        RC_CONSTRAINT_TYPE FOREIGN KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_2
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 2
        Statement failed, SQLSTATE = 54011
        unsuccessful metadata update
        -too many keys defined for index TEST_NOT_ALLOWED_01
        Records affected: 0
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_02 failed
        -attempt to index COMPUTED BY column in INDEX TEST_NOT_ALLOWED_02
        Records affected: 0
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_03 failed
        - attempt to index BLOB column in INDEX TEST_NOT_ALLOWED_03
        Records affected: 0
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_04 failed
        - attempt to index array column in index TEST_NOT_ALLOWED_04
        Records affected: 0
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_05 failed
        -Field ID cannot be used twice in index TEST_NOT_ALLOWED_05
        Records affected: 0
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -partner index segment no 1 has incompatible data type
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_5
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 1
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -could not find UNIQUE or PRIMARY KEY constraint in table TEST with specified columns
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME X
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME Y
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME Z
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 3
        Statement failed, SQLSTATE = 42000
        Expression evaluation error for index "***unknown***" on table "TEST"
        -expression evaluation not supported
        -Argument for SQRT must be zero or positive
        Records affected: 0

        Statement failed, SQLSTATE = 54000
        unsuccessful metadata update
        -cannot add index, index root page is full.
        MAX_NUMBER_OF_CREATED_INDICES 408
        Records affected: 1
    """
    
    expected_stdout_5x = """
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F01_SIMPLEST
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1

        RI_IDX_ID 1
        RI_IDX_NAME TEST_F02_UNQ
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F02
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F03_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F03
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F04_ASCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F04
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F05_DESC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F05
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F06_DESCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME F06
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F09_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        (f09 * f09)
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F10_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (f10 * f10)
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 2
        RI_IDX_NAME TEST_FK_11
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY TEST_PK
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME PID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_3
        RC_CONSTRAINT_TYPE FOREIGN KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_2
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 2
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K01_PARTIAL
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        where k01 = 1 or k01 = 2 or k01 is null
        RS_FLD_NAME K01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K02_PARTIAL_UNQ_DESC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        where k02 = 1 or k02 = 2 or k02 is null
        RS_FLD_NAME K02
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K04_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k04 * k04)
        BLOB_ID
        BLOB_ID
        where dt = current_date
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K05_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k05 * k05)
        BLOB_ID
        BLOB_ID
        where dt = (select max(dt) from test)
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K06_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k06 * k06)
        BLOB_ID
        BLOB_ID
        where dt = (select max(dt) from test)
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        Statement failed, SQLSTATE = 54011
        unsuccessful metadata update
        -too many keys defined for index TEST_NOT_ALLOWED_01
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_02 failed
        -attempt to index COMPUTED BY column in INDEX TEST_NOT_ALLOWED_02
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_03 failed
        - attempt to index BLOB column in INDEX TEST_NOT_ALLOWED_03
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_04 failed
        - attempt to index array column in index TEST_NOT_ALLOWED_04
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX TEST_NOT_ALLOWED_05 failed
        -Field ID cannot be used twice in index TEST_NOT_ALLOWED_05
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -partner index segment no 1 has incompatible data type
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_5
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 1
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -could not find UNIQUE or PRIMARY KEY constraint in table TEST with specified columns
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME X
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME Y
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RS_FLD_NAME Z
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 3
        
        Statement failed, SQLSTATE = 42000
        Expression evaluation error for index "***unknown***" on table "TEST"
        -expression evaluation not supported
        -Argument for SQRT must be zero or positive
        Records affected: 0

        Statement failed, SQLSTATE = 54000
        unsuccessful metadata update
        -cannot add index, index root page is full.        
        MAX_NUMBER_OF_CREATED_INDICES 408
        Records affected: 1
    """

    expected_stdout_6x = """
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F01_SIMPLEST
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F02_UNQ
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F02
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F03_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F03
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F04_ASCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F04
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F05_DESC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F05
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F06_DESCENDING
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME F06
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_07_COMPOUND_ASC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G02
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G03
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G04
        RS_FLD_POS 3
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G05
        RS_FLD_POS 4
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G06
        RS_FLD_POS 5
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G07
        RS_FLD_POS 6
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G08
        RS_FLD_POS 7
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G09
        RS_FLD_POS 8
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G10
        RS_FLD_POS 9
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G11
        RS_FLD_POS 10
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G12
        RS_FLD_POS 11
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G13
        RS_FLD_POS 12
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G14
        RS_FLD_POS 13
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G15
        RS_FLD_POS 14
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        RI_IDX_ID 1
        RI_IDX_NAME TEST_08_COMPOUND_DEC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 16
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME G16
        RS_FLD_POS 15
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 16
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F09_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        (f09 * f09)
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_F10_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (f10 * f10)
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 2
        RI_IDX_NAME TEST_FK_11
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY TEST_PK
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME PID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_3
        RC_CONSTRAINT_TYPE FOREIGN KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_2
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 2
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K01_PARTIAL
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        where k01 = 1 or k01 = 2 or k01 is null
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME K01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K02_PARTIAL_UNQ_DESC
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        where k02 = 1 or k02 = 2 or k02 is null
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME K02
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K04_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k04 * k04)
        BLOB_ID
        BLOB_ID
        where dt = current_date
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K05_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k05 * k05)
        BLOB_ID
        BLOB_ID
        where dt = (select max(dt) from test)
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID 1
        RI_IDX_NAME TEST_K06_PARTIAL_COMPUTED
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (k06 * k06)
        BLOB_ID
        BLOB_ID
        where dt = (select max(dt) from test)
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID <null>
        RI_IDX_NAME TEST_I01_INACTIVE
        RI_REL_NAME TEST
        RI_IDX_UNIQ 0
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 1
        RI_IDX_TYPE 0
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME I01
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID <null>
        RI_IDX_NAME TEST_I02_INACTIVE
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 1
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (i02 * i02)
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID <null>
        RI_IDX_NAME TEST_I03_PARTIAL_INACTIVE
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 1
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        where dt = current_date
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME I03
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        RI_IDX_ID <null>
        RI_IDX_NAME TEST_I04_PARTIAL_COMPUTED_INACTIVE
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 0
        RI_IDX_INACTIVE 1
        RI_IDX_TYPE 1
        RI_IDX_FKEY <null>
        BLOB_ID
        (i04 * i04)
        BLOB_ID
        BLOB_ID
        where dt = (select min(dt) from test)
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME <null>
        RS_FLD_POS <null>
        RC_CONSTRAINT_NAME <null>
        RC_CONSTRAINT_TYPE <null>
        Records affected: 1
        
        Statement failed, SQLSTATE = 54011
        unsuccessful metadata update
        -too many keys defined for index "PUBLIC"."TEST_NOT_ALLOWED_01"
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX "PUBLIC"."TEST_NOT_ALLOWED_02" failed
        -attempt to index COMPUTED BY column in INDEX "PUBLIC"."TEST_NOT_ALLOWED_02"
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX "PUBLIC"."TEST_NOT_ALLOWED_03" failed
        - attempt to index BLOB column in INDEX "PUBLIC"."TEST_NOT_ALLOWED_03"
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX "PUBLIC"."TEST_NOT_ALLOWED_04" failed
        - attempt to index array column in index "PUBLIC"."TEST_NOT_ALLOWED_04"
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE INDEX "PUBLIC"."TEST_NOT_ALLOWED_05" failed
        -Field ID cannot be used twice in index TEST_NOT_ALLOWED_05
        Records affected: 0
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -partner index segment no 1 has incompatible data type
        RI_IDX_ID 1
        RI_IDX_NAME TEST_PK
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 1
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME ID
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME INTEG_5
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 1
        
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -could not find UNIQUE or PRIMARY KEY constraint in table "PUBLIC"."TEST" with specified columns
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME X
        RS_FLD_POS 0
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME Y
        RS_FLD_POS 1
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        RI_IDX_ID 1
        RI_IDX_NAME TEST_X_Y_Z
        RI_REL_NAME TEST
        RI_IDX_UNIQ 1
        RI_IDX_SEGM_COUNT 3
        RI_IDX_INACTIVE 0
        RI_IDX_TYPE <null>
        RI_IDX_FKEY <null>
        BLOB_ID
        BLOB_ID
        BLOB_ID
        RI_IDX_SCHEMA_NAME PUBLIC
        RI_FK_SCHEMA_NAME PUBLIC
        RS_FLD_NAME Z
        RS_FLD_POS 2
        RC_CONSTRAINT_NAME TEST_X_Y_Z
        RC_CONSTRAINT_TYPE PRIMARY KEY
        Records affected: 3
        
        Statement failed, SQLSTATE = 42000
        Expression evaluation error for index "***unknown***" on table "PUBLIC"."TEST"
        -expression evaluation not supported
        -Argument for SQRT must be zero or positive
        
        Records affected: 0
        Statement failed, SQLSTATE = 54000
        unsuccessful metadata update
        -cannot add index, index root page is full.

        MAX_NUMBER_OF_CREATED_INDICES 255
        Records affected: 1
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
