#coding:utf-8

"""
ID:          issue-5728
ISSUE:       5728
TITLE:       Bugcheck 167 (invalid SEND request)
DESCRIPTION:
  Test extracts content of firebird.log, then runs scenario which earlier led to "invalid SEND request (167)"
  and then again get firebird.log for comparing with its previous content.
  The only new record in firebird.log must be:
    "Modifying procedure SP_CALC_VAL which is currently in use by active user requests"
JIRA:        CORE-5457
FBTEST:      bugs.core_5457
"""

import pytest
import time
import re
from difflib import unified_diff
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0.2')
def test_1(act: Action, capsys):
    log_before = act.get_firebird_log()
    #
    with act.db.connect() as con:
        sp_test_ddl = """
            create procedure sp_calc_val(a_id int) returns(val int) as
            begin
               val = a_id * 10;
               suspend;
            end
"""
        con.execute_immediate(sp_test_ddl)
        con.commit()
        test_table_ddl = """
            create table test(
                id int primary key,
                txt varchar(80),
                calc_val computed by ((select val from sp_calc_val(test.id)))
            )
"""
        con.execute_immediate(test_table_ddl)
        con.commit()
        #
        c = con.cursor()
        c.execute('insert into test select row_number()over(), ascii_char(96 + row_number()over()) from rdb$types rows 7')
        con.commit()
        #
        c.execute('select count(*), sum(calc_val) from test').fetchall()
        #
        sp_alter_ddl = """
        alter procedure sp_calc_val (a_id int) returns (val int) as
        begin
            val = a_id * 7;
            suspend;
        end
"""
        con.execute_immediate(sp_alter_ddl)
        c.execute('select count(*), sum(calc_val) from test').fetchall()
        con.commit()
        c.execute('select count(*), sum(calc_val) from test').fetchall()
        #
        time.sleep(1)
    #
    log_after = act.get_firebird_log()
    unexpected_patterns = [re.compile('\\s+internal\\s+Firebird\\s+consistency\\s+check', re.IGNORECASE)]
    for line in unified_diff(log_before, log_after):
        if line.startswith('+'):
            match2some = list(filter(None, [p.search(line) for p in unexpected_patterns]))
            if match2some:
                print(f'UNEXPECTED: {line}')
    #
    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
