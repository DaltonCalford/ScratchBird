#coding:utf-8

"""
ID:          issue-8304
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8304
TITLE:       wrong results using minvalue/maxvalue in join condition
DESCRIPTION:
NOTES:
    [04.11.2024] pzotov
    Confirmed bug on 6.0.0.515-d53f368 (dob: 30.10.2024).
    Checked on 6.0.0.515-1c3dc43; 5.0.2.1551-90fdb97; 4.0.6.3165 (intermediate build).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_sml smallint default 0 not null;
    create domain dm_txt varchar(100) not null;
    create table tbl1 (
        ds int,
        ru dm_sml,
        wi dm_sml,
        ko dm_sml
    );

    create table tbl2 (
        id     int,
        ru     dm_sml,
        ru_txt dm_txt,
        wi     dm_sml,
        wi_txt dm_txt,
        ko     dm_sml,
        ko_txt dm_txt
    );

    commit;

    insert into tbl1 (ds, ru, wi, ko) values(50, 1, 1, 0);

    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(1, 1, 'a', 1, 'a', 1, 'a');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(2, 1, 'b', 1, 'b', 0, 'b');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(3, 1, 'c', 0, 'c', 1, 'c');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(4, 1, 'd', 0, 'd', 0, 'd');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(5, 0, 'e', 1, 'e', 1, 'e');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(6, 0, 'f', 1, 'f', 0, 'f');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(7, 0, 'g', 0, 'g', 1, 'g');
    insert into tbl2 (id, ru, ru_txt, wi, wi_txt, ko, ko_txt) values(8, 0, 'h', 0, 'h', 0, 'h');

    commit;

    set count on;
    set list on;

    -- no record - wrong:
    select 'case-1' as msg, a.*
    from tbl1 a
    join tbl2 b on minvalue(a.ko, 1) = b.ko and
                   minvalue(a.ru, 1) = b.ru and
                   minvalue(a.wi, 1) = b.wi
    ;

    -- one record - correct:
    select 'case-2' as msg, a.*
    from tbl1 a
    join tbl2 b on decode(a.ko, 0, 0, 1) = b.ko and
                   decode(a.ru, 0, 0, 1) = b.ru and
                   decode(a.wi, 0, 0, 1) = b.wi
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             case-1
    DS                              50
    RU                              1
    WI                              1
    KO                              0
    Records affected: 1

    MSG                             case-2
    DS                              50
    RU                              1
    WI                              1
    KO                              0
    Records affected: 1
"""

@pytest.mark.version('>=4.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
