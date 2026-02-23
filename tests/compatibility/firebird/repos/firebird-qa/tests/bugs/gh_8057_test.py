#coding:utf-8

"""
ID:          issue-8057
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8057
TITLE:       Let optimizer automatically update empty index statistics for relatively small system tables
DESCRIPTION:
    Only tables from <CHECKED_SYS_TABLES> list are checked (with additional filtering only those which have indices).
    For each index of selected RDB tables we get name of its first field (i.e. started part of index key).
    Then we construct query with INNER JOIN that involves such index to be included in execution plan.
    For example, for table RDB$DEPENDENCIES with two indices:
        CREATE INDEX RDB$INDEX_27 ON RDB$DEPENDENCIES (RDB$DEPENDENT_NAME, RDB$DEPENDENT_TYPE);
        CREATE INDEX RDB$INDEX_28 ON RDB$DEPENDENCIES (RDB$DEPENDED_ON_NAME, RDB$DEPENDED_ON_TYPE, RDB$FIELD_NAME);
    - following queries will be generated:
    -------------------------------------
        1) Index for columns (RDB$DEPENDENT_NAME, RDB$DEPENDENT_TYPE) will be involved here:
           select a.rdb$dependent_name, count(*)
           from rdb$dependencies a
           join rdb$dependencies b on a.rdb$dependent_name = b.rdb$dependent_name
           where a.rdb$dependent_name = ? and a.rdb$dependent_name < b.rdb$dependent_name
           group by 1

        2) Index for column (RDB$DEPENDED_ON_NAME, RDB$DEPENDED_ON_TYPE, RDB$FIELD_NAME) will be involved here: 
            select a.rdb$depended_on_name, count(*)
            from rdb$dependencies a
            join rdb$dependencies b on a.rdb$depended_on_name = b.rdb$depended_on_name
            where a.rdb$depended_on_name = ? and a.rdb$depended_on_name < b.rdb$depended_on_name
            group by 1
    -------------------------------------

    When every such query is prepared, optimizer must automatically update appropriate index statistics.
    KEY NOTE: this will be done only when system table is not empty and has no more than 100 data pages,
    see FB (see in Optimizer.cpp: "... (relPages->rel_data_pages > 0) && (relPages->rel_data_pages < 100)").

    Because of need to have some data in RDB tables, we must run DDL which force these tables to be fulfilled.
    For this purpose test creates <INIT_DB_OBJECTS_CNT> DB objects for each object type (mapping, sequences etc).
    Also, "NBACKUP -B ..." is called <NBACKUP_RUNS_CNT> times in order to fulfill table RDB$BACKUP_HISTORY table.

    Then, we get IndexRoot page number for each of selected RDB tables and start to parse this page content.
    During this parsing (see func 'parse_index_root_page') we get values of <K>th statistics for each of <N> starting
    parts of compound index (or for single segment for usual index). We accumulate this data in rel_sel_map{}.
    Finally, we check that all indices has NON-zero statistics: rel_sel_map{} must have NO items with zero values.
NOTES:
    [21.12.2024] pzotov
        Currently only FB 6.x has this feature (since 22-mar-2024 11:46).
        Commit:  https://github.com/FirebirdSQL/firebird/commit/ef66a9b4d803d5129a10350c54f00bc637c09b48
        ::: ACHTUNG ::: Index statistics must be searched in the Index Root page rather than in RDB$INDICES!
        Internals of misc FB page types can be found here:
        https://firebirdsql.org/file/documentation/html/en/firebirddocs/firebirdinternals/firebird-internals.html
        It is supposed that there are no expression-based indices for selected system tables (this case was not investigated).

        Confirmed ticket issue on 6.0.0.294-c353de4 (21-mar-2024 16:45): some of system tables remain with non-updated index statistics.
        Checked on 6.0.0.295-ef66a9b (22-mar-2024 13:48): all OK,every of checked system tables has non-zero statistics for its indices.
        Test executiuon time: ~8...10 seconds.
    [24.02.2025] pzotov
        Changed offset calculations according to #93db88: ODS14: header page refactoring (#8401)
        See: https://github.com/FirebirdSQL/firebird/commit/93db88084d5a04aaa3f98179e76cdfa092431fa8
        Thanks to Vlad for explanations.
    [18.07.2025] pzotov
        An old problem triggered by schemas changes was found when this test ran on 6.0.0.834:
        Sent report to Adriano 18.07.2025 08:24, subj:
        Weird "message length error (encountered 506, expected 253)" on attempting to create trivial function after trying to drop non-existing filter
        Fixed on:
        https://github.com/FirebirdSQL/firebird/commit/45b40b86b94bec9deadcab5d376e079700cd68aa
        Checked on 6.0.0.1039-45b40b8.
"""
import sys
import binascii
import struct
from typing import List
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, DbInfoCode

import locale
import time

EXPECTED_MSG = 'Expected: every checked system table has non-zero index statistics.'

IRT_PAGE_TYPE = 6
CHECKED_SYS_TABLES = """
   (
        'RDB$AUTH_MAPPING'
       ,'RDB$BACKUP_HISTORY'
       ,'RDB$CHARACTER_SETS'
       ,'RDB$CHECK_CONSTRAINTS'
       ,'RDB$COLLATIONS'
       ,'RDB$DEPENDENCIES'
       ,'RDB$EXCEPTIONS'
       ,'RDB$FIELDS'
       ,'RDB$FIELD_DIMENSIONS'
       ,'RDB$FILTERS'
       ,'RDB$FORMATS'
       ,'RDB$FUNCTIONS'
       ,'RDB$FUNCTION_ARGUMENTS'
       ,'RDB$GENERATORS'
       ,'RDB$INDEX_SEGMENTS'
       ,'RDB$INDICES'
       ,'RDB$PACKAGES'
       ,'RDB$PROCEDURES'
       ,'RDB$PROCEDURE_PARAMETERS'
       ,'RDB$PUBLICATIONS'
       ,'RDB$PUBLICATION_TABLES'
       ,'RDB$REF_CONSTRAINTS'
       ,'RDB$RELATIONS'
       ,'RDB$RELATION_CONSTRAINTS'
       ,'RDB$RELATION_FIELDS'
       ,'RDB$ROLES'
       ,'RDB$SECURITY_CLASSES'
       ,'RDB$TRIGGERS'
       ,'RDB$TYPES'
       ,'RDB$USER_PRIVILEGES'
       ,'RDB$VIEW_RELATIONS'
   )
"""

NBACKUP_RUNS_CNT = 10
INIT_DB_OBJECTS_CNT = 30

# SQL script which will cause filling of RDB tables with some data:
INIT_DB_OBJECTS_SQL = f"""
    alter database enable publication;
    set term ^;
    execute block as
        declare n_obj_cnt smallint = {INIT_DB_OBJECTS_CNT};
        declare i int;
        declare v_sttm varchar(8190);
    begin
        i = 0;
        while (i < n_obj_cnt) do
        begin
            v_sttm = 'recreate sequence g_' || i;
            execute statement v_sttm;
            v_sttm = 'create or alter mapping local_map_' || i || ' using any plugin from group musicians to role guitarist';
            execute statement v_sttm;

            -- /**********************
            begin
                v_sttm = 'drop collation name_coll_' || i;
                execute statement v_sttm;
                when any do
                begin
                end
            end

            begin
                v_sttm = 'drop role r_manager_' || i;
                execute statement v_sttm;
                when any do
                begin
                end
            end

            begin
                v_sttm = 'drop table tbl_' || i;
                execute statement v_sttm;
                when any do
                begin
                end
            end

            begin
                v_sttm = 'drop domain dm_' || i;
                execute statement v_sttm;
                when any do
                begin
                end
            end

            begin
                v_sttm = 'drop filter jpg_' || i;
                execute statement v_sttm;
                when any do
                begin
                end
            end
            -- ************************/

            -- examples\api\api9f.sql
            -- declare filter desc_filter_01 -- ==> will be saved in indexed column rdb$filters.rdb$function_name
            -- input_type 1
            -- output_type -4
            -- entry_point 'desc_filter'
            -- module_name 'api9f'
            -- ;
            
            v_sttm = 'declare filter jpg_' || i || ' input_type ' || i || ' output_type -4 entry_point ''desc_filter'' module_name ''api9f''';
            execute statement v_sttm;


            v_sttm = 'create collation name_coll_' || i || ' for utf8 from unicode case insensitive';
            execute statement v_sttm;

            v_sttm = 'recreate exception exc_' || i || ' ''missing element with index @1''';
            execute statement v_sttm;

            v_sttm = 'create role r_manager_' || i;
            execute statement v_sttm;

            v_sttm = 'create domain dm_' || i || ' as int not null check(value > 0)';
            execute statement v_sttm;

            v_sttm = 'recreate table tbl_' || i
                     || '( id int generated by default as identity constraint pk_tbl_'|| i  || ' primary key'
                     || ', pid int'
                     || ', f_0 dm_' || i
                     || ', f_01 int'
                     || ', f_02 int[3,4]'
                     || ', constraint fktbl_' || i || ' foreign key(pid) references tbl_' || i || '(id)'
                     || ', constraint chk_tbl_' || i || ' check (f_01 > 0)'
                     || ')'
            ;
            execute statement v_sttm;

            v_sttm = 'recreate trigger trg_' || i || '_bi for tbl_' || i
                     || ' active before insert as'
                     || ' begin'
                     || ' end'
            ;
            execute statement v_sttm;

            execute statement 'alter database include table tbl_' || i || ' to publication';


            v_sttm = 'recreate view vew_' || i || ' as select 1 x from rdb$database';
            execute statement v_sttm;

            v_sttm = 'create or alter procedure sp_' || i || '(a_0 int, a_1 varchar(10)) as begin end';
            execute statement v_sttm;

            v_sttm = 'create or alter function fn_' || i || '(a_0 int, a_1 int) returns int as begin return a_0 + a_1; end';
            execute statement v_sttm;

            v_sttm = 'create or alter package pg_' || i || ' as begin function pg_fn() returns int; end';
            execute statement v_sttm;

            v_sttm = 'recreate package body pg_' || i || ' as begin function pg_fn() returns int as begin return 1; end end';
            execute statement v_sttm;

            i = i + 1;
        end
    end
    ^
    ------------------------------------
    set term ;^
    commit;
"""

db = db_factory(init = INIT_DB_OBJECTS_SQL)
act = python_act('db', substitutions = [('[ \t]+', ' ')])
tmp_nbk_lst = temp_files( [ f'tmp_8057.{i}.nbk' for i in range(NBACKUP_RUNS_CNT) ] )

#-----------------------------------------------------------------------

def parse_index_root_page(db_file, pg_size, rel_name, irt_page_number, rel_sel_map, verbose = False):
    
    # rel_sel_map -- byref
    min_irtd_selec = sys.float_info.max

    with open( db_file, "rb") as db_handle:
        db_handle.seek( irt_page_number * pg_size )
        page_content = db_handle.read( pg_size )
        page_as_hex=binascii.hexlify( page_content )
        if verbose:
            print(f'{pg_size=}, page_as_hex:')
            print(page_as_hex.decode("utf-8"))

        # https://firebirdsql.org/file/documentation/html/en/firebirddocs/firebirdinternals/firebird-internals.html#fbint-page-6
        # https://docs.python.org/3/library/struct.html#format-characters
        """
            See src/jrd/ods.h, https://github.com/FirebirdSQL/firebird/pull/8340
            *** BEFORE ***
            struct index_root_page
            {
                pag irt_header;
                USHORT irt_relation;
                USHORT irt_count;
                struct irt_repeat {
                    SLONG irt_root;
                    union {
                        float irt_selectivity;
                        SLONG irt_transaction;
                    } irt_stuff;
                    USHORT irt_desc;
                    UCHAR irt_keys;
                    UCHAR irt_flags;
                } irt_rpt[1];
            };

            *** AFTER ***
            struct index_root_page
            {
                pag irt_header;
                USHORT irt_relation;            // 2 relation id (for consistency)
                USHORT irt_count;               // 2 number of indices
                ULONG irt_dummy;                // 4 so far used as a padding to ensure the same alignment in 32-bit and 64-bit builds
                struct irt_repeat
                {
                    FB_UINT64 irt_transaction;  // 8 transaction in progress
                    ULONG irt_page_num;         // 4 page number
                    ULONG irt_page_space_id;    // 4 page space
                    USHORT irt_desc;            // 2 offset to key descriptions
                    USHORT irt_flags;           // 2 index flags
                    UCHAR irt_state;            // 1 index state
                    UCHAR irt_keys;             // 1 number of keys in index
                    USHORT irt_dummy;           // 2 alignment to 8-byte boundary
                } irt_rpt[1];
            };

        """

        # Two bytes, UNsigned. Offset 0x10 on the page. The relation id. This is the value of RDB$RELATIONS.RDB$RELATION_ID.
        irt_relation = struct.unpack_from('@H', page_content[0x10:0x12])[0] # (128,) --> 128

        # Two bytes, UNsigned. Offset 0x12 on the page. The number of indices defined for this table. 
        irt_count = struct.unpack_from('@H', page_content[0x12:0x14])[0]

        # 24.02.2025
        irt_dummy = struct.unpack_from('@H', page_content[0x14:0x18])[0]

        if verbose:
            print(f'{irt_relation=}, {rel_name.strip()=}, {irt_count=}')

        for i in range(irt_count):

            ###################
            IRT_REPEAT_LEN = 24
            ###################

            irt_tran_offset_i = i * IRT_REPEAT_LEN + int(0x18)
            irt_page_num_offset = i * IRT_REPEAT_LEN + int(0x26)        # 18 + 8
            irt_page_space_id_offset = i * IRT_REPEAT_LEN + int(0x30)   # 26 + 4
            irt_desc_offset_i = i * IRT_REPEAT_LEN + int(0x34)          # 30 + 4
            irt_flags_offset_i = i * IRT_REPEAT_LEN + int(0x36)         # 34 + 2
            irt_state_offset_i = i * IRT_REPEAT_LEN + int(0x38)         # 36 + 2
            irt_keys_offset_i = i * IRT_REPEAT_LEN + int(0x39)          # 38 + 1
            irt_dummy_offset =  i * IRT_REPEAT_LEN + int(0x40)          # 39 + 1


            # Four bytes, SIGNED. Offset 0x00 in each descriptor array entry.
            # This field is the page number where the root page for the individual index (page type 0x07) is located.
            # 24.02.2025: this field no more exists in new ODS:
            irt_root_i = -1 # before 24.02.2025: struct.unpack_from('@i', page_content[irt_root_offset_i : irt_root_offset_i + 4])[0]

            # Normally this field will be zero but if an index is in the process of being created, the transaction id will be found here.
            # irt_tran_i = struct.unpack_from('@i', page_content[irt_tran_offset_i : irt_tran_offset_i + 4])[0]
            # 24.02.2025: this field now is FB_UINT64, i.e 8 bytes:
            irt_tran_i = struct.unpack_from('@i', page_content[irt_tran_offset_i : irt_tran_offset_i + 8])[0]

            # Two bytes, UNsigned. Offset 0x08 in each descriptor array entry. This field holds the offset, from the start of the page,
            # to the index field descriptors which are located at the bottom end (ie, highest addresses) of the page.
            # To calculate the starting address, add the value in this field to the address of the start of the page.
            irt_desc_i = struct.unpack_from('@H', page_content[irt_desc_offset_i : irt_desc_offset_i + 2])[0]

            # One byte, UNsigned. This defines the number of keys (columns) in this index.
            irt_keys_i = struct.unpack_from('@B', page_content[ irt_keys_offset_i : irt_keys_offset_i + 1])[0]

            # One byte, UNsigned. The flags define various attributes for this index, these are encoded into various bits in the field, as follows:
            # See src/jrd/btr.h
            #   Bit 0 : Index is unique (set) or not (unset).
            #   Bit 1 : Index is descending (set) or ascending (unset).
            #   Bit 2 : Index [creation?] is in progress (set) or not (unset).
            #   Bit 3 : Index is a foreign key index (set) or not (unset).
            #   Bit 4 : Index is a primary key index (set) or not (unset).
            #   Bit 5 : Index is expression based (set) or not (unset).
            #   Bit 6 : Index is conditional
            irt_flags_i = struct.unpack_from('@B', page_content[ irt_flags_offset_i : irt_flags_offset_i + 1])[0]

            if verbose:
                print(f'  {i=} ::: {irt_root_i=}, {irt_tran_i=}, {irt_desc_i=}, {irt_keys_i=}, {irt_flags_i=}, bin(irt_flags_i)=','{0:08b}'.format(irt_flags_i))

            for j in range(irt_keys_i):

                # Two bytes, UNsigned. Offset 0x00 in each field descriptor. This field defines the field number of the table that makes up 'this' field in the index.
                # This number is equivalent to RDB$RELATION_FIELDS.RDB$FIELD_ID.
                irtd_field = struct.unpack_from('@H', page_content[ j*8 + irt_desc_i : j*8 + irt_desc_i + 2])[0]
                #print(f'       column: {j} ::: {irtd_field=}')

                # Two bytes, UNsigned. Offset 0x02 in each field descriptor. This determines the data type of the appropriate field in the index.
                irtd_itype = struct.unpack_from('@H', page_content[ j*8 + irt_desc_i + 2 : j*8 + irt_desc_i + 4])[0]
                #print(f'                       {irtd_itype=}')

                # Four bytes, floating point format. Offset 0x04 in each field descriptor. This field holds the selectivity of this particular column in the index. 
                irtd_selec = struct.unpack_from('@f', page_content[ j*8 + irt_desc_i + 4 : j*8 + irt_desc_i + 8])[0]
                min_irtd_selec = min(min_irtd_selec, irtd_selec)
                #print(f'                       {irtd_selec=}')

            # Input, byRef:
            rel_sel_map[rel_name.strip()] = min_irtd_selec

#-----------------------------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_nbk_lst: List[Path], capsys):

    try:
        for i,tmp_nbk_i in enumerate(tmp_nbk_lst):
            act.expected_stderr = ''
            tmp_nbk_i.unlink(missing_ok = True)
            act.nbackup(switches=['-b', str(i), act.db.dsn, tmp_nbk_i], io_enc = locale.getpreferredencoding())
            assert act.clean_stderr == act.clean_expected_stderr
            act.reset()
    except DatabaseError as e:
        print(e.__str__())
    
    #-----------------------------------------------------------
    sql_get_sys_tables_index_info = f"""
        with recursive r
        as (
                select
                     rr.rdb$relation_id as rel_id
                    ,rr.rdb$relation_name rel_name
                    ,ri.rdb$index_id-1 as idx_id
                    ,ri.rdb$index_name idx_name
                    ,ri.rdb$segment_count seg_cnt
                    ,rs.rdb$field_position fpos
                    ,rs.rdb$field_name fname
                    ,',' || cast( trim(rs.rdb$field_name) as varchar(8190) ) as idx_key
                    ,sign(octet_length(ri.rdb$expression_blr)) as idx_on_expr
                from rdb$relations rr
                join rdb$indices ri on rr.rdb$relation_name = ri.rdb$relation_name
                join rdb$index_segments rs on ri.rdb$index_name = rs.rdb$index_name
                where
                    ri.rdb$system_flag = 1
                    and ri.rdb$relation_name in {CHECKED_SYS_TABLES}
                    and ri.rdb$index_inactive is distinct from 1
                    and (rs.rdb$field_position = 0 or ri.rdb$expression_blr is not null)

                union all

                select
                     r.rel_id
                    ,r.rel_name
                    ,ri.rdb$index_id-1
                    ,ri.rdb$index_name
                    ,r.seg_cnt
                    ,rs.rdb$field_position
                    ,rs.rdb$field_name fname
                    ,r.idx_key || ',' || trim(rs.rdb$field_name )
                    ,r.idx_on_expr
                from rdb$indices ri
                join rdb$index_segments rs on ri.rdb$index_name = rs.rdb$index_name
                join r on ri.rdb$relation_name = r.rel_name
                and ri.rdb$index_name = r.idx_name
                and (rs.rdb$field_position = r.fpos+1 or r.idx_on_expr = 1)
        )
        --select * from r

        ,m as (
            select
                 rel_id
                ,rel_name
                ,idx_id
                ,idx_name
                ,iif(idx_on_expr = 1, '<expr>', substring(idx_key from 2)) as idx_key
            from r
            where fpos = seg_cnt-1 or idx_on_expr = 1
        )
        select m.rel_id, m.rel_name, m.idx_id, m.idx_name, m.idx_key, p.rdb$page_number as irt_page
        from m
        join rdb$pages p on m.rel_id = p.rdb$relation_id and p.rdb$page_type = {IRT_PAGE_TYPE}
        order by rel_id, idx_id
    """

    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute(sql_get_sys_tables_index_info)
        rel_irt_map = {}
        for r in cur:
            rel_id, rel_name, idx_id, idx_name, idx_key, irt_page = r[:6]
            idx_starting_fld = idx_key.split(",")[0]

            #print(f'{rel_id=}, {rel_name.strip()=}, {irt_page=}')
            #print(f'{idx_id=}, {idx_name.strip()=}')
            #print(f'{idx_key=}, starting field: {idx_starting_fld}')

            sql_to_make_recalc_idx_stat = f"""
                select a.{idx_starting_fld}, count(*)
                from {rel_name.strip()} a
                join {rel_name.strip()} b on a.{idx_starting_fld} = b.{idx_starting_fld}
                where a.{idx_starting_fld} = ? and a.{idx_starting_fld} < b.{idx_starting_fld}
                group by 1
            """

            #print(f'{sql_to_make_recalc_idx_stat=}')
            ps = None
            try:
                # This must cause update index statistics of <idx_name>:
                ps = cur.prepare(sql_to_make_recalc_idx_stat)

                # Print explained plan with padding eash line by dots in order to see indentations:
                #print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
                #print('')
            except DatabaseError as e:
                print(e.__str__())
                print(e.gds_codes)
            finally:
                if ps:
                    ps.free()

            rel_irt_map[ rel_id ] = (rel_name, irt_page)

    rel_sel_map = {} # K = rel_name, V = minimal selectivity (i.e. min irtd_selec for all indices)

    # NB: we have to re-connect in order to see updated indices statistics!
    with act.db.connect() as con:
        cur = con.cursor()

        for rel_id, (rel_name, irt_no) in rel_irt_map.items():
            parse_index_root_page(act.db.db_path, con.info.page_size, rel_name, irt_no, rel_sel_map, verbose = False)
    
    if min(rel_sel_map.values()) > 0:
        print(EXPECTED_MSG)
    else:
        print('UNEXPECTED: AT LEAST ONE OF SYSTEM TABLES HAS ZERO INDEX STATISTICS')
        for rel_name, min_idx_selectivity in rel_sel_map.items():
            print(f'{rel_name=}, {min_idx_selectivity=}')

    act.expected_stdout = f"""
        {EXPECTED_MSG}
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
