#coding:utf-8

"""
ID:          issue-8178
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8178
TITLE:       Check result of conversion to string in COALESCE that involves all families of data types.
DESCRIPTION:
    Additional test for gh_8178
NOTES:
    [12.07.2024] pzotov
    Checked on 6.0.0.392, 5.0.1.1434, 4.0.5.3127, 3.0.12.33765
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table dvalues(
        t_int int default 12345
       ,t_boo boolean default true
       ,t_chr char default 'A'
       ,t_blb blob default 'bbbbbbblllllllloooooooooobbbbbbbb'
       ,t_dat date default '01.01.1991'
       ,t_tim time default '01:02:03.456'
       ,t_tst timestamp default '01.02.2003 23:34:56'
       ,t_nul char default null
    );
    insert into dvalues default values;
    commit;
    ---------------------------------------------------------
    recreate table dtypes(f smallint, t varchar(20));
    insert into dtypes(f,t) values( 1,'int');
    insert into dtypes(f,t) values( 2,'boo');
    insert into dtypes(f,t) values( 3,'chr');
    insert into dtypes(f,t) values( 4,'blb');
    insert into dtypes(f,t) values( 5,'dat');
    insert into dtypes(f,t) values( 6,'tim');
    insert into dtypes(f,t) values( 7,'tst');
    --insert into dtypes(f,t) values( 8,'nul');
    commit;

    set count on;

    set term ^;
    execute block returns( checked_expr varchar(1024), raised_gds int) as
        declare v_expr varchar(1024);
        declare v_done smallint;
    begin
        for
            select
                --a.t as a_t, b.t as b_t, c.t as c_t, d.t as d_t, e.t as e_t, f.t as f_t, g.t as g_t
                --,a.f as a_f, b.f as b_f, c.f as c_f, d.f as d_f, e.f as e_f, f.f as f_f, g.f as g_f,
                'select iif( coalesce('
                 ||
                 iif( a.t = 'nul',   'null',   't_'||a.t)
                 ||
                 iif( b.t = 'nul', ', null', ', t_'||b.t)
                 ||
                 iif( c.t = 'nul', ', null', ', t_'||c.t)
                 ||
                 iif( d.t = 'nul', ', null', ', t_'||d.t)
                 ||
                 iif( e.t = 'nul', ', null', ', t_'||e.t)
                 ||
                 iif( f.t = 'nul', ', null', ', t_'||f.t)
                 ||
                 iif( g.t = 'nul', ', null', ', t_'||g.t)
                 ||
                 ') is distinct from null, 1, 0 ) as x from dvalues' as expr
            from dtypes a
            cross join dtypes b
            cross join dtypes c
            cross join dtypes d
            cross join dtypes e
            cross join dtypes f
            cross join dtypes g
            where
                a.f not in (b.f, c.f, d.f, e.f, f.f, g.f)
            and b.f not in (c.f, d.f, e.f, f.f, g.f)
            and c.f not in (d.f, e.f, f.f, g.f)
            and d.f not in (e.f, f.f, g.f)
            and e.f not in (f.f, g.f)
            and f.f not in (g.f)
            order by a.t, b.t, c.t, d.t, e.t, f.t, g.t
            as cursor k
        do begin
           checked_expr = k.expr;
           raised_gds = 0;
           begin
               execute statement checked_expr into v_done;
               when any do
               begin
                   raised_gds = gdscode;
               end
           end
           if (raised_gds <> 0) then
               suspend;
        end
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0.12')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
