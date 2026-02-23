#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/cc183f599ee09233d6da294893f651de0ab76136
TITLE:       Add key info to the merge join plan output
DESCRIPTION:
NOTES:
    MERGE JOIN will be chosen by optimizer when several conditions are met:
    * data sources are joined by INNER join;
    * data sources are ORDERED, BUT *NOT* via GROUP BY (because optimizer tends to think about grouped data
      that its cardinality *much* less than cardinality of source and it causes to decide using hash join instead).
      This was explained by dimitr privately, letter: 24-sep-2024 16:30.
    * number of conflicts in the hash table must be above 1009 * 1000 = 1009000.
      Experimental shows that minimal threshold for switching from HJ to MJ is 1009883 rows.
      This value must NOT depend on machine but can have limited dependency on page_size
      (see letter from dimitr, 24-sep-2024 19:09).
    * No ticket has been created for this test.

    Checked on 6.0.0.467.
"""

import re
import time
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

init_sql = """
    create table test1(id int not null);
    create table test2(id int not null, pid int not null);

    set stat on;
    set echo on;
    set term ^;
    execute block as
        declare n_cnt int =    1009883; -- OK, plan MERGE JOIN
        -- declare n_cnt int =    1009882; -- plan HJ
        declare i int  = 0;
    begin
        while (i < n_cnt) do
        begin
            insert into test1(id) values(:i);
            i = i + 1;
        end
    end
    ^
    set term ;^
    insert into test2(id, pid) select row_number()over(), id from test1;
    commit;
"""
db = db_factory(init = init_sql, page_size = 8192)

substitutions =  [  ('[ \t]+', ' ')
                   ,('keys: \\d+, total key length: \\d+', 'keys, total key length')
                   ,('record length: \\d+, key length: \\d+', 'record length, key length')
                 ]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

#-----------------------------------------------------------

def replace_leading(source, char="."):
    stripped = source.lstrip()
    return char * (len(source) - len(stripped)) + stripped

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    test_sql = """
        select a.id, b.pid
        from (
            select id from test1 order by id
        ) a
        join 
        (
            select pid from test2 b order by pid
        ) b
        on a.id = b.pid
    """

    with act.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare(test_sql)
            print( '\n'.join([replace_leading(s) for s in ps.detailed_plan.split('\n')]) )
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

    act.expected_stdout = """
        Select Expression
        ....-> Filter
        ........-> Merge Join (inner) (keys: 1, total key length: 8)
        ............-> Sort (record length: 28, key length: 8)
        ................-> Sort (record length: 28, key length: 8)
        ....................-> Table "TEST1" as "A TEST1" Full Scan
        ............-> Sort (record length: 28, key length: 8)
        ................-> Sort (record length: 28, key length: 8)
        ....................-> Table "TEST2" as "B B" Full Scan
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
