#coding:utf-8

"""
ID:          issue-5878
ISSUE:       5878
TITLE:       Gradual slowdown compilation (create, recreate or drop) of views
DESCRIPTION:
    Test creates <VIEWS_COUNT> views with <FIELDS_COUNT> columns and DDL similar to ticket (see 'v_ddl_lst_1').
    Then we run ALTER VIEW command for each of them (see 'v_ddl_lst_2') and after this DROP all views.
    During this work we gather statistics for every RDB tables using con.info.get_table_access_stats().

    There are two tables which caused problem: RDB$DEPENDENCIES and (especially) RDB$RELATION_FIELDS.
    Before fix in 4.x, RDB$DEPENDENCIES had increased value of indexed reads (comparing to the total IR for all tables).
    After fix, the ratio become less than 0.33.
    For the table RDB$RELATION_FIELDS one may see:
        * big value (about 4.3M) of NATURAL reads and low value of IR for all 4.x snapshots;
        * extremely big value of INDEXED reads for snapshots 6.0.0.800 ... 6.0.0.1295 (about 16.5M).

    Test verifies that FOUR ratios not exceeding thresholds:
        * number of NR for RDB$RELATION_FIELDS vs total NR for all tables must be <= RDB_RELFLD_NR_MAX_RATIO
        * number of IR for RDB$RELATION_FIELDS vs total IR for all tables must be <= RDB_RELFLD_IR_MAX_RATIO
        * number of NR for RDB$DEPENDENCIES vs total NR for all tables must be <= RDB_DEPEND_NR_MAX_RATIO
        * number of IR for RDB$DEPENDENCIES vs total IR for all tables must be <= RDB_DEPEND_IR_MAX_RATIO
    Thresholds have different values depending on FB major version (4.x vs 5.x+).
JIRA:        CORE-5612
FBTEST:      bugs.core_5612
NOTES:
    [19.10.2025] pzotov
    
    It was reported that problem was fixed 26-oct-2018 in:
    https://github.com/FirebirdSQL/firebird/commit/2cb9e648315b9f1ec925e48618b0678a1b9959fe
    Although, on all 4.x too big value of NR for RDB$RELATION_FIELDS remains.

    Difference between 4.0.0.1227 vs 4.0.0.1346 can be seen in the indexed reads for RDB$DEPENDENCIES:
    ratio between IR for this table vs total IR is ~0.40 (before fix) vs 0.32 (after fix).

    On 4.0.7.x problem exists with too big NR for RDB$RELATION_FIELDS table (~4.3M).
    On 5.x problem not appeared (there are no huge values of NR or IR for any tables).
    On 6.x bug appeared since 6.0.0.800-b8be591 ("Feature #1113 - SQL Schemas"), 10.06.2025 23:42 UTC:
    there are ~15.6M indexed reads of RDB$RELATION_FIELDS for that snapshot and up to 6.0.0.1295.
    Ratio between this IR and total sum of IR for all tables was about 1.

    Since 6.0.0.1299 this bug has been fixed: number opf IR for RDB$RELATION_FIELDS is ~47K, ratio for
    IR of this table vs all tables became normal, i.e. less than 0.33.

    This is table statistics for tested snapshots:
    -------------------------------------------------------------------------------------------------
    !                      !     4.0.7.3237    !   5.0.4.1725  !     6.0.0.1295   !     6.0.0.1299  !
    ! TABLE NAME           !      NR        IR !     NR     IR !     NR        IR !      NR      IR !
    ! ---------------------!-------------------!---------------!------------------!-----------------!
    ! RDB$CHARACTER_SETS   !       0         1 !      0      1 !      0         1 !       0       1 !
    ! RDB$COLLATIONS       !       0         1 !      0      1 !      0         1 !       0       1 !
    ! RDB$DATABASE         !    1255         0 !   1255      0 !   1252         0 !    1252       0 !
    ! RDB$DEPENDENCIES     !       0     83250 !      0  50740 !      0     50740 !       0   50740 !
    ! RDB$FIELDS           !       0     35000 !      0  30000 !      0     30000 !       0   30000 !
    ! RDB$FORMATS          !       0       500 !      0    500 !      0       500 !       0     500 !
    !                      !       0         0 !  52500      0 !      0         0 !       0       0 !
    ! RDB$GENERATORS       !       0       252 !      0    252 !      0       252 !       0     252 !
    ! RDB$INDICES          !       0        41 !      0     41 !      0         0 !       0       0 !
    !                      !       0         0 !  75000      0 !      0         0 !       0       0 !
    ! RDB$RELATIONS        !       0     22505 !      0  12505 !      0     18750 !       0   18750 !
    ! RDB$RELATION_FIELDS  ! 4295000     50000 !      0  50000 !      0  15666250 !       0   47500 !
    !                      !       0         0 !      0      0 !      0      2006 !       0    2006 !
    ! RDB$SECURITY_CLASSES !       0      3000 !      0    500 !      0       500 !       0     500 !
    ! RDB$TRIGGERS         !       0         3 !      0      3 !      0         0 !       0       0 !
    ! RDB$USER_PRIVILEGES  !       0     11250 !      0  11250 !      0     15000 !       0   11250 !
    ! RDB$VIEW_RELATIONS   !       0      2500 ! 311250   2500 ! 311250      2500 !  311250    2500 !
    -------------------------------------------------------------------------------------------------

    It must be noted that for reliable reproducing of the problem (on 4.x) number of created views must be NOT LESS than 190.
    Otherwise number of NR for RDB$RELATION_FIELDS will be zero and all operations against this table will be indexed.
    
    Thanks to dimitr for suggestion (letter 10.10.2025 09:53).
    Confirmed problem on 6.0.0.1295, fix on 6.0.0.1299.
    Checked on 6.0.0.1312; 5.0.4.1725; 4.0.7.3237
"""
import os
from pathlib import Path
import time

import pytest
from firebird.qa import *

###########################
###   S E T T I N G S   ###
###########################
#VIEWS_COUNT =  20
#FIELDS_COUNT = 254
#VIEWS_COUNT =  500
#FIELDS_COUNT = 10

VIEWS_COUNT =  250
FIELDS_COUNT = 10

db = db_factory(page_size = 16384)
act = python_act('db')
tmp_sql = temp_file('tmp_core_5612.sql')

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_sql: Path, capsys):

    if act.is_version('<5'):
        RDB_RELFLD_IR_MAX_RATIO = 0.33
        RDB_RELFLD_NR_MAX_RATIO = 1
        RDB_DEPEND_IR_MAX_RATIO = 0.33
        RDB_DEPEND_NR_MAX_RATIO = 0
    else:
        RDB_RELFLD_IR_MAX_RATIO = 0.33
        RDB_RELFLD_NR_MAX_RATIO = 0
        RDB_DEPEND_IR_MAX_RATIO = 0.33
        RDB_DEPEND_NR_MAX_RATIO = 0

    # create or alter view v_test_n (f_0, f_1, ..., f_249) as
    # select 
    #    (select rdb$description from rdb$database where rdb$relation_id = d.rdb$relation_id),
    #    (select rdb$relation_id from rdb$database where rdb$relation_id = d.rdb$relation_id),
    #    ...
    #    (select rdb$relation_id from rdb$database where rdb$relation_id = d.rdb$relation_id),
    # from rdb$database as d;

    v_fields_in_hdr = ','.join( [ f'f_{i}' for i in range(FIELDS_COUNT) ] )

    v_ddl_lst_1 = []
    for i in range(VIEWS_COUNT):
        source_field_name = 'd.rdb$relation_id' if i == 0 else f'd.f_{i-1}'
        v_fields_in_sql = '\n,'.join( [f'(select rdb$description from rdb$database where rdb$relation_id = ' + ('d.rdb$relation_id' if i == 0 else f'd.f_{j}') + ')' for j in range(FIELDS_COUNT)] )
        v_ddl_prefix = f'create or alter view v_test_{i}({v_fields_in_hdr}) as select \n{v_fields_in_sql} \nfrom '
        v_ddl_lst_1.append(v_ddl_prefix + ('rdb$database as d' if i == 0 else f'v_test_{i-1} as d') + ';\n\n' )

    v_fields_in_sql = '\n,'.join( [f'(select rdb$format from rdb$relations x where x.rdb$relation_id = d.rdb$relation_id)' for i in range(FIELDS_COUNT)] )
    v_ddl_lst_2 = [ f'\ncreate or alter view v_test_{i}({v_fields_in_hdr}) as\nselect\n' + v_fields_in_sql + '\nfrom rdb$pages as d;' for i in range(VIEWS_COUNT)]

    rel_fields_idx_count = rel_fields_nat_count = 0
    rdb_depend_idx_count = rdb_depend_nat_count = 0
    total_tabs_idx_count = total_tabs_nat_count = 0
    with act.db.connect() as con:

        cur=con.cursor()
        cur.execute('select rdb$relation_id, trim(rdb$relation_name) from rdb$relations where rdb$system_flag = 1')
        rdb_map = {}
        for r in cur:
            rdb_map[ r[0] ] = (r[1], 0, 0) # name; NR; IR

        for p in con.info.get_table_access_stats():
            t_name, nr_count, ir_count = rdb_map[ p.table_id ] 
            if p.sequential is not None:
                nr_count -= p.sequential
            if p.indexed is not None:
                ir_count -= p.indexed
            rdb_map[ p.table_id ] = (t_name, nr_count, ir_count)

        # creating views:
        #############
        for v in v_ddl_lst_1:
            con.execute_immediate(v)
            con.commit()

        # altering views:
        #############
        for i in range(VIEWS_COUNT-1,-1,-1):
            con.execute_immediate(v_ddl_lst_2[i])
            con.commit()
        
        # drop views:
        #############
        for i in range(VIEWS_COUNT-1,-1,-1):
            con.execute_immediate(f'drop view v_test_{i}')
            con.commit()

        for p in con.info.get_table_access_stats():
            t_name, nr_count, ir_count = rdb_map[ p.table_id ] 
            if p.sequential is not None:
                nr_count += p.sequential
            if p.indexed is not None:
                ir_count += p.indexed
            rdb_map[ p.table_id ] = (t_name, nr_count, ir_count)
            if t_name == 'RDB$RELATION_FIELDS':
                rel_fields_idx_count = ir_count
                rel_fields_nat_count = nr_count
            elif t_name == 'RDB$DEPENDENCIES':
                rdb_depend_idx_count = ir_count
                rdb_depend_nat_count = nr_count

            total_tabs_idx_count += ir_count
            total_tabs_nat_count += nr_count

    rdb_relfld_ir_ratio = rel_fields_idx_count / total_tabs_idx_count
    rdb_relfld_nr_ratio = rel_fields_nat_count / total_tabs_nat_count

    rdb_depend_ir_ratio = rdb_depend_idx_count / total_tabs_idx_count
    rdb_depend_nr_ratio = rdb_depend_nat_count / total_tabs_nat_count

    EXPECTED_MSG = 'Indexed and natural reads for RDB$RELATION_FIELDS and RDB$DEPENDENCIES (evaluated spearately for indexed and natural) are ACCEPTABLE.'

    if rdb_relfld_ir_ratio <= RDB_RELFLD_IR_MAX_RATIO and rdb_relfld_nr_ratio <= RDB_RELFLD_NR_MAX_RATIO and rdb_depend_ir_ratio <= RDB_DEPEND_IR_MAX_RATIO and rdb_depend_nr_ratio <= RDB_DEPEND_NR_MAX_RATIO:
        print(EXPECTED_MSG)
    else:
        print('At least one of ratios for indexed and/or natural reads of RDB$RELATION_FIELDS and/or RDB$DEPENDENCIES vs all tables is greater than max allowed:')
        print('Check statistics:')
        print('Table name'.ljust(32), 'NR'.rjust(9), 'IR'.rjust(9))
        for v in sorted(rdb_map.values()):
            if v[1] or v[2]:
                print(v[0].ljust(32), f'{v[1]:9d}', f'{v[2]:9d}')

        print(f'rdb_relfld_ir_ratio={rdb_relfld_ir_ratio:.4f}, RDB_RELFLD_IR_MAX_RATIO={RDB_RELFLD_IR_MAX_RATIO:.4f}; {rdb_relfld_ir_ratio <= RDB_RELFLD_IR_MAX_RATIO= }')
        print(f'rdb_relfld_nr_ratio={rdb_relfld_nr_ratio:.4f}, RDB_RELFLD_NR_MAX_RATIO={RDB_RELFLD_NR_MAX_RATIO:.4f}; {rdb_relfld_nr_ratio <= RDB_RELFLD_NR_MAX_RATIO= }')
        print(f'rdb_depend_ir_ratio={rdb_depend_ir_ratio:.4f}, RDB_DEPEND_IR_MAX_RATIO={RDB_DEPEND_IR_MAX_RATIO:.4f}; {rdb_depend_ir_ratio <= RDB_DEPEND_IR_MAX_RATIO= }')
        print(f'rdb_depend_nr_ratio={rdb_depend_nr_ratio:.4f}, RDB_DEPEND_NR_MAX_RATIO={RDB_DEPEND_NR_MAX_RATIO:.4f}; {rdb_depend_nr_ratio <= RDB_DEPEND_NR_MAX_RATIO= }')

    act.expected_stdout = EXPECTED_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
