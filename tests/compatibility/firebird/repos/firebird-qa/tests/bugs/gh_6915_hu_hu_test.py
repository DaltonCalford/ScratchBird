#coding:utf-8

"""
ID:          issue-6915-hu-hu
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6915
TITLE:       Performance effect of applying 'DISABLE-COMPRESSIONS=1' in UNICODE collation for LOCALE=hu_HU
DESCRIPTION:
    Original discussion:
    https://sourceforge.net/p/firebird/mailman/firebird-devel/thread/9361c612-d720-eb76-d412-7101518ca60d%40ibphoenix.cz/
    Test verifies only PERFORMANCE issues referred to in the ticket #6915. Correctness of ORDER BY is not checked.
    A pre-build database is used for check, see: files/gh_6915_hu_hu.zip (it was created in FB 4.x with date 31-jul-2021).
    Hungarian alphabet uses several digraphs and one trigraph: cs,dz,dzs,gy,ly,ny,sz,ty,zs.
    Test table has uniform distribution for following characters: c,d,g,l,n,s,t,z,cs,dz,dzs,gy,ly,ny,sz,ty,zs
    (they all have been randomly selected in the loop of ~ 1E6 iterations).
    SQL script that was used to fulfill test DB see in the end of this file.

    We decompress .fbk, restore from it and check that for every testing queries number of indexed reads will not
    exceed threshold, see 'MAX_IDX_READS_THRESHOLD' (con.info.get_table_access_stats() is used for that).
    After improvement this threshold could be set to 1.
    Only columns with attribute 'DISABLE-COMPRESSIONS=1' are checked.
NOTES:
    [24.12.2024] pzotov

    It seems that commit in 4.x (2af9ded1a696a43f5b0bea39a88610287e3ab06c; 04-aug-2021 17:58) had no effect:
    performance in 4.x remains poort for queries from this test up to recent snapshots (dec-2024).

    Commit in 5.x (cfc09f75a3dea099f54c09808e39fe778457f441; 04-aug-2021 20:25; 5.0.0.129) really SOLVED problem:
    adding attribute 'DISABLE-COMPRESSIONS=1' causes reducing indexed reads to 0 or 1 for all queries.

    There was commit in 5.x: 171cb7eebc365e301a7384eff96c0e3e069c95cc (date: 17-mar-2022 22:38) - which had
    further improvement for 'DISABLE-COMPRESSIONS=0' (i.e. when compression is Enabled). Snapshots of FB 5.x
    before that commit (i.e. up to 5.0.0.425) had poor performance for 'DISABLE-COMPRESSIONS=0', and after
    this commit (since 5.0.0.426) performance became equal to 'DISABLE-COMPRESSIONS=1'.
    Because of that, this test verifies performance of only ONE case: 'DISABLE-COMPRESSIONS=1', by comparing
    of indexed reads for each query with threshold, see MAX_IDX_READS_THRESHOLD.
    Before improvement related to 'DISABLE-COMPRESSIONS=1', indexed reads were huge for almost all check queries.
    This is outcome for 5.0.0.126 (31.07.2021):
        where f_ci_compr_disabled >= 'cs' order by f_ci_compr_disabled rows 1   ==> idx_reads=122087
        where f_ci_compr_disabled >= 'd' order by f_ci_compr_disabled rows 1    ==> idx_reads=182562
        where f_ci_compr_disabled >= 'dz' order by f_ci_compr_disabled rows 1   ==> idx_reads=243178
        where f_ci_compr_disabled >= 'dzs' order by f_ci_compr_disabled rows 1  ==> idx_reads=60339
        ...
        (and similarpoor results for 'starting with', 'like' and 'similar to')

    Confirmed poor performance on 5.0.0.126 (31.07.2021): all check queries have huge indexed reads,
    regardless on 'DISABLE-COMPRESSIONS=1' attribute (i.e. it had no effect on performance), 
    execution time was 5...15 seconds for each query.
    Checked on 5.0.0.129 (05.08.2021 04:25) -- all OK, indexed reads for all queries are 0 or 1.
    Checked on 6.0.0.553, 5.0.2.1580.
"""

from pathlib import Path
import zipfile
import locale
import pytest
from firebird.qa import *
from firebird.driver import connect

db = db_factory(charset = 'utf8')
act = python_act('db', substitutions=[('[ \t]+', ' ')])

tmp_fbk = temp_file('gh_6915_hu_hu.tmp.fbk')
tmp_fdb = temp_file('gh_6915_hu_hu.tmp.fdb')

MAX_IDX_READS_THRESHOLD = 1
EXPECTED_MSG = f'Expected. All queries have indexed reads no more than {MAX_IDX_READS_THRESHOLD=}'

test_sql = """
    with
    d as (
        select '0' as disabled_compression from rdb$database 
        -- union all
        -- select '1' as disabled_compression from rdb$database
    )
    ,
    c as (
        select 'ci' as case_attribute from rdb$database union all
        select 'cs' from rdb$database
    )
    ,o as (
        select '>=' as search_op from rdb$database union all
        select 'starting with' from rdb$database union all
        select 'like' from rdb$database union all
        select 'similar to' from rdb$database
    )
    ,e as (
        -- c,d,g,l,n,s,t,z,cs,dz,dzs,gy,ly,ny,sz,ty,zs
        select p.item as letter
        from list_to_rows('c,d,g,l,n,s,t,z,cs,dz,dzs,gy,ly,ny,sz,ty,zs') p
    )
    ,f as (
        select 
            d.*, c.*, o.*, e.*
            ,'select 1 from test where f_' || c.case_attribute || '_compr_' || iif(d.disabled_compression = '0', 'disabled', 'enabled')
            || ' ' || trim(o.search_op) || ' '
            || ''''
            || e.letter
            || trim( iif( upper(trim(o.search_op)) in ('>=', upper('starting with')), '', '%') )
            || ''''
            || '  order by f_' || c.case_attribute || '_compr_' || iif(d.disabled_compression = '0', 'disabled', 'enabled')
            || '  rows 1'
        as query_txt
        from d
        cross join c
        cross join o
        cross join e
    )
    select
        --case_attribute
        --,search_op
        --,letter
        max(iif(disabled_compression = 0, query_txt, null)) as q_compr_disabled
        --max(iif(disabled_compression = 1, query_txt, null)) as q_compr_enabled
    from f
    group by
        case_attribute
        ,search_op
        ,letter
    ;
"""

@pytest.mark.intl
@pytest.mark.version('>=5.0.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, capsys):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_6915_hu_hu.zip', at = 'gh_6915_hu_hu.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    act.gbak(switches = ['-rep', str(tmp_fbk), str(tmp_fdb)], combine_output = True, io_enc = locale.getpreferredencoding())
    assert '' == act.stdout
    act.reset()
    reads_map = {}
    with connect(str(tmp_fdb), user = act.db.user, password = act.db.password, charset = 'utf8') as con:
        cur = con.cursor()
        cur2 = con.cursor()

        cur.execute("select 1 from rdb$database r left join rdb$procedures p on p.rdb$procedure_name = upper('LIST_TO_ROWS')")
        required_sp_exists = cur.fetchone()[0]
        assert required_sp_exists == 1, 'Missed required procedure. Check restored database.'

        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = upper('test')")
        src_relation_id = cur.fetchone()[0]

        cur.execute(test_sql)
        for r in cur:
            idx_reads = -1
            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == src_relation_id:
                    idx_reads = - (x_table.indexed if x_table.indexed else 0)

            cur2.execute(r[0])
            cur2.fetchall()

            for x_table in con.info.get_table_access_stats():
                if x_table.table_id == src_relation_id:
                    idx_reads += (x_table.indexed if x_table.indexed else 0)

            reads_map[ r[0] ] = idx_reads


    if max(reads_map.values()) <= MAX_IDX_READS_THRESHOLD:
        print(EXPECTED_MSG)
    else:
        print(f'UNEXPECTED: at least one query has values of indexed reads greater than {MAX_IDX_READS_THRESHOLD=}')
        for check_qry, idx_reads in reads_map.items():
            if idx_reads > MAX_IDX_READS_THRESHOLD:
                print(f'{check_qry=}, {idx_reads=}')

    act.expected_stdout = f"""
        {EXPECTED_MSG}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

# End of test.

################################
# SQL with initial DDL and data:
################################

# set names utf8;
# -- https://github.com/FirebirdSQL/firebird/issues/6915#issue-958497709
# -- contractions in Hungarian lang: cs, dz, dzs, gy, ly, ny, sz, ty, zs
# create database 'localhost:r:\temp\tmp4test.fdb' default character set utf8;
# create collation u_ci_compr_disabled
#     for utf8  
#     from unicode  
#     case insensitive
#     'LOCALE=hu_HU;DISABLE-COMPRESSIONS=1'
# ;
#    
# create collation u_cs_compr_disabled
#     for utf8  
#     from unicode  
#     case sensitive
#     'LOCALE=hu_HU;DISABLE-COMPRESSIONS=1'
# ;
# create collation u_ci_compr_enabled
#     for utf8  
#     from unicode  
#     case insensitive
#     'LOCALE=hu_HU;DISABLE-COMPRESSIONS=0'
# ;
#    
# create collation u_cs_compr_enabled
#     for utf8  
#     from unicode  
#     case sensitive
#     'LOCALE=hu_HU;DISABLE-COMPRESSIONS=0'
# ;
# create table test (
#    f_cs_compr_disabled varchar(10) collate u_cs_compr_disabled
#   ,f_ci_compr_disabled varchar(10) collate u_ci_compr_disabled
#   ,f_cs_compr_enabled varchar(10) collate u_cs_compr_enabled
#   ,f_ci_compr_enabled varchar(10) collate u_ci_compr_enabled
# );
# create global temporary table gtt (
#     id smallint generated by default as identity constraint gtt_pk primary key
#    ,txt varchar(10) 
# ) on commit preserve rows
# ;
#
# set term ^;
# create or alter procedure list_to_rows (
#     A_LST blob sub_type 1 segment size 80,
#     A_DEL char(1) = ',')
# returns (
#     LINE integer,
#     EOF integer,
#     ITEM varchar(8190))
# AS
#   declare pos_ int;
#   declare noffset int = 1;
#   declare beg int;
#   declare buf varchar(8190);
# begin
#   -- Splits blob to lines by single char delimiter.
#   -- adapted from here:
#   -- http://www.sql.ru/forum/actualthread.aspx?bid=2&tid=607154&pg=2#6686267
#   if (a_lst is null) then exit;
#   line=0;
#
#   while (0=0) do begin
#     buf = substring(a_lst from noffset for 30100);
#     pos_ = 1; beg = 1;
#     while (pos_ <= char_length(buf) and pos_ <= 30000) do begin
#       if (substring(buf from pos_ for 1) = :a_del) then begin
#         if (pos_ > beg) then
#           item = substring(buf from beg for pos_ - beg);
#         else
#           item = ''; --null;
#         suspend;
#         line=line+1;
#         beg = pos_ + 1;
#       end
#       pos_ = pos_ + 1;
#     end
#     if (noffset + pos_ - 2 = char_length(a_lst)) then leave;
#     noffset = noffset + beg - 1;
#     if (noffset > char_length(a_lst)) then leave;
#   end
#
#   if (pos_ > beg) then begin
#     item = substring(buf from beg for pos_ - beg);
#     eof=-1;
#   end
#   else begin
#     item = '';
#     eof=-1;
#   end
#   suspend;
#
# end
# ^
# execute block
# as
#   declare rowscount int = 1000000;
#   --declare rowscount int = 100;
#   declare id_min smallint;
#   declare id_max smallint;
#   declare id_rnd smallint;
#   declare i int = 0;
#   declare c varchar(10);
# begin
#     insert into gtt(txt) select p.item from list_to_rows('c,d,g,l,n,s,t,z,cs,dz,dzs,gy,ly,ny,sz,ty,zs', ',') p;
#     select min(id), max(id) from gtt into id_min, id_max;
#     while (i < rowscount) do
#     begin
#         id_rnd = cast( -0.5 + rand() * (0.5 + id_max - id_min) as int );
#         id_rnd = minvalue( maxvalue(id_min,id_rnd), id_max);
#         select g.txt from gtt g where g.id = :id_rnd into c;
#         insert into test(
#              f_cs_compr_disabled
#             ,f_ci_compr_disabled
#             ,f_cs_compr_enabled
#             ,f_ci_compr_enabled
#         ) values (
#              :c
#             ,:c
#             ,:c
#             ,:c
#         );
#         i = i + 1;
#     end
# end
# ^
# set term ;^
# commit;
# create index test_cs_compr_disabled on test (f_cs_compr_disabled);
# create index test_ci_compr_disabled on test (f_ci_compr_disabled);
# create index test_cs_compr_enabled on test (f_cs_compr_enabled);
# create index test_ci_compr_enabled on test (f_ci_compr_enabled);
# commit;
