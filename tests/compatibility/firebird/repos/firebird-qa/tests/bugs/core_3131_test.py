#coding:utf-8

"""
ID:          issue-3508
ISSUE:       3508
TITLE:       WIN1257_LV (Latvian) collation is wrong for 4 letters: A E I U
DESCRIPTION:
JIRA:        CORE-3131
FBTEST:      bugs.core_3131
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='WIN1257')

act = python_act('db')

test_script = """
    set bail on;
    set names win1257;

    create collation coll_1257_ci_ai
       for win1257 from win1257_lv
       no pad case insensitive accent sensitive;

    commit;

    create table test1257 (
            letter varchar(2) collate coll_1257_ci_ai,
            sort_index  smallint
    );

    commit;

     -- ### ONCE AGAIN ###
     -- 1) for checking this under ISQL following must be encoded in WIN1257
     -- 2) for running under fbt_run utility following must be encoded in UTF8.

     insert into test1257 values ('Iz',  18);
     insert into test1257 values ('Īb',  19);
     insert into test1257 values ('Īz',  20);

     insert into test1257 values ('Ķz',  24);
     insert into test1257 values ('Ēz',  12);
     insert into test1257 values ('Gb',  13);

     insert into test1257 values ('Ģz',  16);
     insert into test1257 values ('Ib',  17);

     insert into test1257 values ('Gz',  14);
     insert into test1257 values ('Ģb',  15);

     insert into test1257 values ('Ņb',  31);
     insert into test1257 values ('Ņz',  32);
     insert into test1257 values ('Cb',  5);
     insert into test1257 values ('Ūb',  39);
     insert into test1257 values ('Ūz',  40);
     insert into test1257 values ('Zb',  41);
     insert into test1257 values ('Eb',  9);
     insert into test1257 values ('Ez',  10);
     insert into test1257 values ('Ēb',  11);

     insert into test1257 values ('Ub',  37);
     insert into test1257 values ('Uz',  38);

     insert into test1257 values ('Lz',  26);
     insert into test1257 values ('Ļb',  27);
     insert into test1257 values ('Ļz',  28);
     insert into test1257 values ('Kb',  21);
     insert into test1257 values ('Kz',  22);
     insert into test1257 values ('Šz',  36);
     insert into test1257 values ('Lb',  25);
     insert into test1257 values ('Cz',  6);
     insert into test1257 values ('Čb',  7);
     insert into test1257 values ('Čz',  8);

     insert into test1257 values ('Sb',  33);
     insert into test1257 values ('Sz',  34);
     insert into test1257 values ('Šb',  35);

     insert into test1257 values ('Nb',  29);
     insert into test1257 values ('Nz',  30);
     insert into test1257 values ('Ķb',  23);
     insert into test1257 values ('Zz',  42);
     insert into test1257 values ('Žb',  43);
     insert into test1257 values ('Žz',  44);

     insert into test1257 values ('Ab',  1);
     insert into test1257 values ('Az',  2);
     insert into test1257 values ('Āb',  3);
     insert into test1257 values ('Āz',  4);
     commit;

     set heading off;
     select *
     from test1257 tls
     order by tls.letter collate coll_1257_ci_ai;
"""

expected_stdout = """
	Ab              1
	Az              2
	Āb              3
	Āz              4
	Cb              5
	Cz              6
	Čb              7
	Čz              8
	Eb              9
	Ez             10
	Ēb             11
	Ēz             12
	Gb             13
	Gz             14
	Ģb             15
	Ģz             16
	Ib             17
	Iz             18
	Īb             19
	Īz             20
	Kb             21
	Kz             22
	Ķb             23
	Ķz             24
	Lb             25
	Lz             26
	Ļb             27
	Ļz             28
	Nb             29
	Nz             30
	Ņb             31
	Ņz             32
	Sb             33
	Sz             34
	Šb             35
	Šz             36
	Ub             37
	Uz             38
	Ūb             39
	Ūz             40
	Zb             41
	Zz             42
	Žb             43
	Žz             44
"""


script_file = temp_file('test-script.sql')

@pytest.mark.version('>=3')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp1257')
    act.expected_stdout = expected_stdout
    act.isql(switches=[], input_file=script_file, charset='WIN1257')
    assert act.clean_stdout == act.clean_expected_stdout

