#coding:utf-8

"""
ID:          issue-7218
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7218
TITLE:       Let ISQL show per-table run-time statistics.
NOTES:
    [23.02.2023] pzotov
        Checked on 5.0.0.958.
    [08.07.2025] pzotov
        ::: WARNING :::
        First word in the header (" Table name  | Natural | Index  ...") starts from offset = 1,
        first byte if space (i.e. the letter "T" is shown on byte N2).

        Leading space are removed from each line before assertion, i.e. act.clean_stdout will contain:
        "Table name  | Natural | Index  ..."  - i.e. the "T" letter will be at starting byte (offset=0).
        This causes expected_out to be 'wrongly formatted', e.g. like the header row is 'shifted' for 
        one character to left:
        --------------------+---------+---------+---------+
        Table name         | Natural | Index   | Insert  |
        --------------------+---------+---------+---------+
        SYSTEM.RDB$FIELDS   |         |        2|         |

        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.930; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813

"""

import pytest
from firebird.qa import *
from pathlib import Path

db = db_factory(charset = 'utf8')

act = python_act('db', substitutions=[('in file .*', 'in file XXX')])

non_ascii_ddl='''
     set bail on;
     recreate table "склад" (
          id int
         ,amt numeric(12,2)
         ,grp_id int
         ,constraint "склад_ПК" primary key (id)
     );

     recreate table "справочник групп изделий используемых в ремонте спецавтомобилей" (
          id int
         ,grp varchar(155)
         ,constraint "справочник групп изделий используемых в ремонте спецавтомоб__ПК" primary key (id)
     );

     recreate view "группы_изд" as select * from "справочник групп изделий используемых в ремонте спецавтомобилей";

     recreate view "Электрика" ("ид изделия", "Запас") as
     select
          s.id
         ,s.amt
     from "склад" s
     join "группы_изд" g on s.grp_id = g.id
     where g.grp = 'Электрика'
     ;
     commit;

     -------------------------------------------------
     insert into "группы_изд" values(1, 'Метизы');
     insert into "группы_изд" values(2, 'ГСМ');
     insert into "группы_изд" values(3, 'Электрика');
     insert into "группы_изд" values(4, 'Лако-красочные материалы');
     -------------------------------------------------
     insert into "склад"(id, amt, grp_id) values (1, 111, 1);
     insert into "склад"(id, amt, grp_id) values (2, 222, 3);
     insert into "склад"(id, amt, grp_id) values (3, 333, 1);
     insert into "склад"(id, amt, grp_id) values (4, 444, 3);
     insert into "склад"(id, amt, grp_id) values (5, 555, 3);
     insert into "склад"(id, amt, grp_id) values (7, 777, 1);
     commit;

     SET PER_TABLE_STATS ON;
     set list on;
     select count(*) as "Всего номенклатура электрики, шт." from "Электрика";
'''

tmp_file = temp_file('non_ascii_ddl.sql')

expected_stdout_5x = """
    Всего номенклатура электрики, шт. 3
    Per table statistics:
    ----------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
    Table name                                                     | Natural | Index   | Insert  | Update  | Delete  | Backout | Purge   | Expunge |
    ----------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
    RDB$FIELDS                                                      |         |        2|         |         |         |         |         |         |
    RDB$RELATION_FIELDS                                             |         |        4|         |         |         |         |         |         |
    RDB$RELATIONS                                                   |         |        3|         |         |         |         |         |         |
    RDB$SECURITY_CLASSES                                            |         |        1|         |         |         |         |         |         |
    склад                                                           |        6|         |         |         |         |         |         |         |
    справочник групп изделий используемых в ремонте спецавтомобилей |        4|         |         |         |         |         |         |         |
    ----------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
"""

expected_stdout_6x = """
    Всего номенклатура электрики, шт. 3
    Per table statistics:
    ------------------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
    Table name                                                             | Natural | Index   | Insert  | Update  | Delete  | Backout | Purge   | Expunge |
    ------------------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
    SYSTEM.RDB$FIELDS                                                       |         |        2|         |         |         |         |         |         |
    SYSTEM.RDB$RELATION_FIELDS                                              |         |        4|         |         |         |         |         |         |
    SYSTEM.RDB$RELATIONS                                                    |         |        4|         |         |         |         |         |         |
    SYSTEM.RDB$SECURITY_CLASSES                                             |         |        1|         |         |         |         |         |         |
    PUBLIC."склад"                                                          |        6|         |         |         |         |         |         |         |
    PUBLIC."справочник групп изделий используемых в ремонте спецавтомобилей"|        4|         |         |         |         |         |         |         |
    ------------------------------------------------------------------------+---------+---------+---------+---------+---------+---------+---------+---------+
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_file: Path):
    tmp_file.write_bytes(non_ascii_ddl.encode('cp1251'))

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    # !NB! run with charset:
    act.isql(switches=['-q'], combine_output = True, input_file = tmp_file, charset = 'win1251', io_enc = 'cp1251')
    assert act.clean_stdout == act.clean_expected_stdout
