#coding:utf-8
"""
ID:          issue-6cfc9b5d
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/6cfc9b5d7dc343295968cc4545c6fa184966cfc9
TITLE:       Scrollable cursors. Automated test with randomly generated mission for positioning.
DESCRIPTION:
    Test verifies commit "Fixed the remaining issues with scrollable cursor re-positioning" (06.12.2021 07:47).
    We generate data (see usage of table 'TEST' and SP sp_generate_test_data), and this data (see TEST.BINARY_DATA)
    can be one of following type:
       * NULL in every record;
       * be compressible with MAX ratio (string like 'AAA....AAA');
       * be absolutely incompressible (UUID-based values).
    Number of rows in the TEST table is defined by settings N_ROWS
    Number of operations for change cursor position is defined by settings FETCH_OPERATIONS_TO_PERFORM
    
    Then we perform miscelaneous operations that change position of scrollable cursor (on random basis).
    These operations are stored in the table 'job_for_cursor_movement', see call of SP sp_make_job_for_cursor_movement.
    Outcome of that: randomly generated operations are stored in the table 'move_operations'.

    Result of positioning (value of obtained ID) is compared with expected one.
    These actions are performed for all possible combinations of the following criteria:
        * data compressibility (see 'a_suitable_for_compression')
        * connection protocol ('local' or 'remote')
        * value of WireCrypt parameter (Enabled or DIsabled)
NOTES:
    [20.07.2024] pzotov
    1. Values of TEST.ID must start with 1 rather rthan 0.
    2. Custom driver-config object must be used for DPB because two values of WireCrypt parameter must be checked:
       Enabled and Disabled (see 'w_crypt').
       Also, we verify result for two protocols that are used for connection: local and remote (INET).

    Checked on 6.0.0.396, 5.0.1.1440 (both SS/CS).
"""

import pytest
from firebird.qa import *
from firebird.driver import driver_config, connect, NetProtocol
import time

#########################
###  S E T T I N G S  ###
#########################
N_ROWS = 20
N_WIDTH = 32765
FETCH_OPERATIONS_TO_PERFORM = 100
#-------------------------

init_script = f"""
    create or alter procedure sp_generate_test_data as begin end;
    create or alter procedure sp_make_job_for_cursor_movement as begin end;
    create or alter view v_fetch_expected_data as select 1 x from rdb$database;
    create or alter view v_fetch_job as select 1 x from rdb$database;

    recreate table move_operations(nm varchar(10));
    insert into move_operations(nm) values('first');
    insert into move_operations(nm) values('last');
    insert into move_operations(nm) values('next');
    insert into move_operations(nm) values('prior');
    insert into move_operations(nm) values('absolute');
    insert into move_operations(nm) values('relative');
    commit;

    recreate table job_for_cursor_movement(
      rn int primary key
      ,nm varchar(10)
      ,arg_n int
      ,cnt int
      ,id_min int
      ,id_max int
      ,bof int
      ,eof int
      ,id_expected int
      -- ,constraint job_valid check( nm in ('absolute', 'relative') and arg_n is not null )
    ) 
    ;

    ---------------------
    recreate table test( id int primary key, binary_data varchar( {N_WIDTH} ) character set octets );
    comment on table test is q'#Textual column width was substitued from .py script, see variable 'N_WIDTH' there.#'
    ;
    commit;
    ---------------------

    create or alter view v_fetch_job as
    with
    i as (
        select
           count(*)+1 as cnt
           ,min(id) as id_min
           ,max(id) as id_max
        from test
    )
    ,a as (
        select
            f.nm
            ,iif(f.nm in ('absolute', 'relative'), 1, null) as def_sign
            ,i.cnt
            ,i.id_min
            ,i.id_max
            ,iif(f.nm = 'absolute', 0, -i.cnt) as rnd_min
            ,iif(f.nm = 'absolute', i.cnt, 2*i.cnt) as rnd_max
        from move_operations f
        cross join i
    )
    select
         row_number()over(order by rand()) as rn
        ,a.nm
        ,a.def_sign * cast(a.rnd_min  + rand() * a.rnd_max as int) as arg_n
        ,a.cnt
        ,a.id_min
        ,a.id_max
    from a
    cross join(select rand() as x from rdb$types a, rdb$types b rows 1000) b
    ;

    ----------------------------
    create or alter view v_fetch_expected_data as
    with recursive
    r as (
        select
            rn
            ,cast(null as int) as id_expect_bak
            ,nm
            ,arg_n
            ,case nm
                 when 'first' then id_min
                 when 'last' then id_max
                 when 'prior' then bof
                 when 'next' then id_min
                 when 'relative' then iif( id_min + arg_n - 1 > id_max, eof, iif( id_min + arg_n <= id_min, bof, id_min + arg_n - 1 ) )
                 when 'absolute' then -- iif( arg_n > id_max, eof, iif( id_min + arg_n <=0, bof, id_min + arg_n ) )
                     case
                         when b.arg_n > b.id_max then b.eof
                         when b.arg_n > 0 then b.id_min + (b.arg_n-1)
                         when b.arg_n  = 0 or b.arg_n < -b.cnt then b.bof
                         else -- b.arg_n between -b.cnt and -1
                             b.id_max + b.arg_n - 1
                     end
             end as id_expected
            ,cnt
            ,id_min
            ,id_max
            ,bof
            ,eof
        from job_for_cursor_movement b
        where rn = 1

        UNION ALL

        select
            x.rn
            ,r.id_expected as id_expect_bak
            ,x.nm
            ,x.arg_n
            ,case x.nm
                 when 'first' then r.id_min
                 when 'last' then r.id_max
                 when 'prior' then iif(r.id_expected = r.eof, r.id_max, iif( r.id_expected <= r.id_min, r.bof, r.id_expected - 1 ) )
                 when 'next' then iif( r.id_expected = r.bof, r.id_min, iif( r.id_expected >= r.id_max, r.eof, r.id_expected + 1 ) )
                 when 'relative' then -- iif( r.id_expected + x.arg_n > r.id_max, r.eof, iif( r.id_expected + x.arg_n < r.id_min, r.bof, r.id_expected + x.arg_n ) )
                     case
                         when r.id_expected = r.bof then iif(x.arg_n <= 0, r.bof, iif(x.arg_n >= r.cnt, r.eof, x.arg_n))
                         when r.id_expected = r.eof then iif(x.arg_n >= 0, r.eof, iif(abs(x.arg_n) >= r.cnt, r.bof, r.id_max + 1 - abs(x.arg_n)))
                         else iif( r.id_expected + x.arg_n > r.id_max, r.eof, iif( r.id_expected + x.arg_n < r.id_min, r.bof, r.id_expected + x.arg_n ) )
                     end
                 when 'absolute' then
                     case
                         when x.arg_n > r.id_max then r.eof
                         when x.arg_n > 0 then r.id_min + (x.arg_n-1)
                         when x.arg_n  = 0 or x.arg_n < -r.cnt then r.bof
                         else -- x.arg_n between -r.cnt and -1
                             r.id_max + x.arg_n - 1
                     end
             end as id_expected
            ,r.cnt
            ,r.id_min
            ,r.id_max
            ,r.bof
            ,r.eof
        from r
        join job_for_cursor_movement x on r.rn + 1 = x.rn
        --where x.rn <= 3

    )
    select * from r
    ;
    -----------------------------

    set term ^;
    create or alter procedure sp_make_job_for_cursor_movement(a_rows_to_add int = 20) as
        declare cnt int;
        declare bof int = -2147483648;
        declare eof int = 2147483647;
    begin
        delete from job_for_cursor_movement;
        select count(*) from test into cnt;

        insert into job_for_cursor_movement(
            rn,
            nm,
            arg_n,
            cnt,
            id_min,
            id_max,
            bof,
            eof
        )
        select
            rn,
            nm,
            arg_n,
            cnt,
            id_min,
            id_max,
            :bof,
            :eof
        from v_fetch_job
        rows :a_rows_to_add
        ;

        merge into job_for_cursor_movement t
        using (select rn, id_expected from v_fetch_expected_data) s on t.rn = s.rn
        when MATCHED then update set t.id_expected = s.id_expected
        ;

    end
    ^
    create or alter procedure sp_generate_test_data(a_suitable_for_compression smallint, a_rows int) as
        declare i int = 1;
        declare dummy int;
        declare s varchar({N_WIDTH});
        declare uuid_text varchar({N_WIDTH}) character set octets;
        declare uuid_addi varchar(16) character set octets;
    begin
        delete from test;
        select count(*) from test into dummy;
        s = iif(a_suitable_for_compression = 0, null, lpad('', {N_WIDTH}, 'A'));
        if (a_suitable_for_compression in (0,1) ) then
            begin
            while( i <= a_rows ) do
                begin
                    insert into test(id, binary_data) values(:i, :s);
                    i = i + 1;
                end
            end
        else
            begin
               -- generate NON-compressible data
               while (i <= a_rows) do
               begin
                   uuid_text = '';
                   uuid_addi = '';
                   while ( 1=1 ) do
                   begin
                       uuid_addi = gen_uuid();
                       if ( octet_length(uuid_text) < {N_WIDTH} - octet_length(uuid_addi) ) then
                           uuid_text = uuid_text || trim(uuid_addi);
                       else
                           begin
                               uuid_text = uuid_text || left(uuid_addi, {N_WIDTH} - octet_length(uuid_text));
                               leave;
                           end


                   end
                   insert into test(id,binary_data) values( :i, :uuid_text);
                   --execute statement ('insert into test(id,binary_data) values(?, ?)') ( :i, :uuid_text);
                   i = i + 1;
               end
            end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init = init_script, charset='win1251')
act = python_act('db')

#----------------------------

def print_row(row, cur = None):
    if row:
        print(f"{row[0]}")
        if cur and (cur.is_bof() or cur.is_eof()):
            print('### STRANGE BOF/EOR WHILE SOME DATA CAN BE SEEN ###')
    else:
        msg = '*** NO_DATA***'
        if cur:
            msg += '  BOF=%r    EOF=%r' % ( cur.is_bof(), cur.is_eof() )
        print(msg)

#----------------------------

@pytest.mark.scroll_cur
@pytest.mark.version('>=5.0.0')
def test_1(act: Action, capsys):
    
    for a_suitable_for_compression in (0,1,2):
        move_ops = {}
        with act.db.connect() as con:
            cur = con.cursor()
            cur.callproc('sp_generate_test_data', (a_suitable_for_compression, N_ROWS))
            con.commit()
            cur.callproc('sp_make_job_for_cursor_movement', (FETCH_OPERATIONS_TO_PERFORM,))
            con.commit()

            cur.execute('select rn, nm, arg_n, id_expected from job_for_cursor_movement order by rn')
            for r in cur:
                move_ops[ r[0] ]  = (r[1], '' if r[2] == None else r[2], r[3])
        
        # result:
        # table 'test' contains <N_ROWS> rows with data that has character appropriated to a_suitable_for_compression value
        # table 'job_for_cursor_movement' contains <FETCH_OPERATIONS_TO_PERFORM> rows with randomly generated 'jobs' for cursor positioning.
        # column job_for_cursor_movement.id_expected has EXPECTED values for ID after each position operation against table 'test'; we will compare ID with it.

        # Example of move_ops:
        #   1 : oper = fetch_last , id_expected: 19
        #   2 : oper = fetch_first , id_expected: 0
        #   3 : oper = fetch_relative(5) , id_expected: 5
        #   4 : oper = fetch_relative(16) , id_expected: 2147483647
        #   ...
        #  96 : oper = fetch_absolute(2) , id_expected: 1
        #  97 : oper = fetch_prior , id_expected: 0
        #  98 : oper = fetch_relative(5) , id_expected: 5
        #  99 : oper = fetch_relative(-9) , id_expected: -2147483648
        # 100 : oper = fetch_absolute(10) , id_expected: 9

        srv_cfg = driver_config.register_server(name = f'test_6cfc9b5d_srv_{a_suitable_for_compression}', config = '')

        for protocol_name in ('local', 'remote'):
            for w_crypt in ('Enabled', 'Disabled'):
                
                PASSED_MSG = f'{a_suitable_for_compression=}, {protocol_name=}, {w_crypt=}: PASSED.'

                db_cfg_name = f'test_90129c6d_wcrypt_{a_suitable_for_compression}_{w_crypt}_{protocol_name}'
                db_cfg_object = driver_config.register_database(name = db_cfg_name)
                db_cfg_object.server.value = srv_cfg.name
                db_cfg_object.protocol.value = None if protocol_name == 'local' else NetProtocol.INET
                db_cfg_object.database.value = str(act.db.db_path)
                db_cfg_object.config.value = f"""
                    WireCrypt = w_crypt
                """

                details_lst = []
                chk_retcode = 0
                with connect(db_cfg_name, user = act.db.user, password = act.db.password) as con:

                    cur = con.cursor()
                    cur.open("select id, binary_data from test order by id")

                    for k,v in sorted(move_ops.items()):
                        crow = None
                        chk_val = None
                        f_kind = v[0].strip()
                        if f_kind == 'prior':
                            crow = cur.fetch_prior()
                        elif f_kind == 'next':
                            crow = cur.fetch_next()
                        elif f_kind == 'first':
                            crow = cur.fetch_first()
                        elif f_kind == 'last':
                            crow = cur.fetch_last()
                        elif f_kind == 'relative':
                            crow = cur.fetch_relative( v[1] )
                        elif f_kind == 'absolute':
                            crow = cur.fetch_absolute( v[1] )

                        wrong_found_msg = ''
                        if crow:
                            chk_val = crow[0]
                            if cur.is_bof() or cur.is_eof():
                                wrong_found_msg += ('One of BOF/EOF is true (BOF=%r, EOF=%r), but cursor returns data: %d' % (cur.is_bof(), cur.is_eof(), crow[0]))
                        else:
                            if cur.is_bof():
                               chk_val = -2147483648
                            elif cur.is_eof():
                               chk_val = 2147483647
                            else:
                               wrong_found_msg += '!! No data from cursor but also neither BOF nor EOF.' 

                        msg = ''.join(
                                         ( '%5d ' % k
                                           ,'+++ passed +++' if wrong_found_msg == '' and v[2] == chk_val else '## MISMATCH ##'
                                           ,f'fetch oper: {f_kind},{v[1]}; id_expected: {v[2]}'
                                           ,f'; id_actual: {chk_val};'
                                           ,'BOF' if cur.is_bof() else ''
                                           ,'EOF' if cur.is_eof() else ''
                                         )
                                     )
                        details_lst.append(msg)

                        if not v[2] == chk_val or wrong_found_msg:
                            chk_retcode = 1
                            print(f'{a_suitable_for_compression=}, {protocol_name=}, {w_crypt=}: FAILED.')
                            for x in details_lst:
                                print(x)
                            break

                    if chk_retcode == 0:
                        print(PASSED_MSG)
                    else:
                        print('###############')
                        print('### F A I L ###')
                        print('###############')

                act.expected_stdout = PASSED_MSG
                act.stdout = capsys.readouterr().out
                assert act.clean_stdout == act.clean_expected_stdout
                act.reset()
