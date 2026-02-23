#coding:utf-8

"""
ID:          issue-8214
ISSUE:       8214
TITLE:       Incorrect result of index list scan for a composite index, the second segment of which is a text field with COLLATE UNICODE_CI
DESCRIPTION:
    Test adds check for:
    * collation with attributes 'case insensitive accent insensitive';
    * null values of some records (the must not appear in any query);
    * non-ascii values;
    * both asc and desc indices - results must be identical;
    * miscelaneous predicates
NOTES:
    [31.10.2024] pzotov
    Confirmed bug on 5.0.2.1547.
    Checked on 5.0.2.1551, 6.0.0.515.
"""

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

test_script = """
    set bail on;
    set list on;

    create collation txt_coll_ci for utf8 from unicode case insensitive;
    create collation txt_coll_ci_ai for utf8 from unicode case insensitive accent insensitive;

    recreate table mans (
        id bigint not null,
        code_sex smallint not null,
        name_1 varchar(50) collate txt_coll_ci,
        name_2 varchar(50) collate txt_coll_ci_ai,
        constraint pk_mans primary key(id)
    );

    commit;
    insert into mans (id, code_sex, name_1, name_2) values (1, 1, 'BoB',     'BØb');
    insert into mans (id, code_sex, name_1, name_2) values (2, 1, 'jOhN',    'jŐhŇ');
    insert into mans (id, code_sex, name_1, name_2) values (3, 2, 'BArbArA', 'BÄŔBĄŕă');
    insert into mans (id, code_sex, name_1, name_2) values (4, 2, 'aNNA',    'âŃŃÁ');
    insert into mans (id, code_sex, name_1, name_2) values (5, 1, null,      null);
    insert into mans (id, code_sex, name_1, name_2) values (6, 2, null,      null);
    insert into mans (id, code_sex, name_1, name_2) values (7, 1, 'danIEL',  'ĐÁniel');
    insert into mans (id, code_sex, name_1, name_2) values (8, 2, 'debora',  'ĐeborÁ');
    commit;

    create index mans_sex_name_1_asc on mans(code_sex, name_1);
    create index mans_sex_name_2_asc on mans(code_sex, name_2);

    create view v_test_1 as
    select msg, id, name_1
    from (
    	select 'chk-a' as msg, id, code_sex, name_1
    	from mans where code_sex between 1 and 2 and name_1 starts 'b'
    	UNION ALL
    	select 'chk-b' as msg, id, code_sex, name_1
    	from mans where code_sex > 0 and code_sex < 3 and name_1 starts 'b'
    	UNION ALL
    	select 'chk-c' as msg, id, code_sex, name_1
    	from mans where (code_sex =1 or code_sex =2) and name_1 starts 'b'
    	UNION ALL
    	select 'chk-d' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and name_1 starts 'b'
    	UNION ALL
    	select 'chk-e' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and name_1 like 'b%'
    	UNION ALL
    	select 'chk-f' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and name_1 similar to 'b%'
    	UNION ALL
    	select 'chk-g' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and name_1 in ('boB', 'barbarA')
    	UNION ALL
    	select 'chk-h' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and (name_1 is not distinct from 'boB' or name_1 is not distinct from 'barbarA')
    	UNION ALL
    	select 'chk-i' as msg, id, code_sex, name_1
    	from mans where code_sex in(1,2) and (name_1 >= 'D' and name_1 <= 'E')
    )
    order by msg, id
    ;

    create view v_test_2 as
    select msg, id, name_2
    from (
    	select 'chk-a' as msg, id, code_sex, name_2
    	from mans where code_sex between 1 and 2 and name_2 starts 'b'
    	UNION ALL
    	select 'chk-b' as msg, id, code_sex, name_2
    	from mans where code_sex > 0 and code_sex < 3 and name_2 starts 'b'
    	UNION ALL
    	select 'chk-c' as msg, id, code_sex, name_2
    	from mans where (code_sex =1 or code_sex =2) and name_2 starts 'b'
    	UNION ALL
    	select 'chk-d' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and name_2 starts 'b'
    	UNION ALL
    	select 'chk-e' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and name_2 like 'b%'
    	UNION ALL
    	select 'chk-f' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and name_2 similar to 'b%'
    	UNION ALL
    	select 'chk-g' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and name_2 in ('boB', 'barbarA')
    	UNION ALL
    	select 'chk-h' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and (name_2 is not distinct from 'boB' or name_2 is not distinct from 'barbarA')
    	UNION ALL
    	select 'chk-i' as msg, id, code_sex, name_2
    	from mans where code_sex in(1,2) and (name_2 >= 'D' and name_2 <= 'E')
    )
    order by msg, id
    ;


    select * from v_test_1;
    select * from v_test_2;
    commit;

    -----------------------------------------------------------

    alter index mans_sex_name_1_asc inactive;
    alter index mans_sex_name_2_asc inactive;

    create descending index mans_sex_name_1_dec on mans(code_sex, name_1);
    create descending index mans_sex_name_2_dec on mans(code_sex, name_2);
    commit;

    select * from v_test_1;
    select * from v_test_2;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+',' ') ])

expected_stdout = """
    MSG                             chk-a               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-a               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-b               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-b               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-c               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-c               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-d               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-d               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-e               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-e               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-f               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-f               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-g               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-g               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-h               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-h               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-i               
    ID                              7
    NAME_1                          danIEL

    MSG                             chk-i               
    ID                              8
    NAME_1                          debora



    MSG                             chk-a               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-a               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-b               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-b               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-c               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-c               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-d               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-d               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-e               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-e               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-f               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-f               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-g               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-g               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-h               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-h               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-i               
    ID                              7
    NAME_2                          ĐÁniel

    MSG                             chk-i               
    ID                              8
    NAME_2                          ĐeborÁ



    MSG                             chk-a               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-a               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-b               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-b               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-c               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-c               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-d               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-d               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-e               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-e               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-f               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-f               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-g               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-g               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-h               
    ID                              1
    NAME_1                          BoB

    MSG                             chk-h               
    ID                              3
    NAME_1                          BArbArA

    MSG                             chk-i               
    ID                              7
    NAME_1                          danIEL

    MSG                             chk-i               
    ID                              8
    NAME_1                          debora



    MSG                             chk-a               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-a               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-b               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-b               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-c               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-c               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-d               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-d               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-e               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-e               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-f               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-f               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-g               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-g               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-h               
    ID                              1
    NAME_2                          BØb

    MSG                             chk-h               
    ID                              3
    NAME_2                          BÄŔBĄŕă

    MSG                             chk-i               
    ID                              7
    NAME_2                          ĐÁniel

    MSG                             chk-i               
    ID                              8
    NAME_2                          ĐeborÁ
"""

@pytest.mark.version('>=5.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

