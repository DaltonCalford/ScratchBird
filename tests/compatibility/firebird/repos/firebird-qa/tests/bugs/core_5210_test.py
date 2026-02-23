#coding:utf-8

"""
ID:          issue-5491
ISSUE:       5491
TITLE:       Firebird 3.0 + fbclient 3.0 - POST_EVENT won't work
DESCRIPTION:
  We create database-level trigger which sends event with name 'dml_event' on COMMIT.
  Then we do new connect and run thread with INSERT statement (with delay = 1.0 second), and wait
  NO MORE than <max4delivering> sec.
  We should receive event during ~ 1.0 second.
  We have to consider result as FAIL if we do not receive event in <max4delivering> seconds.
  Result of "events.wait(max4delivering)" will be non-empty dictionary with following key-value:
    {'dml_event': 1} - when all fine and client got event;
    {'dml_event': 0} - when NO event was delivered
  All such actions are repeated several times in order to increase probability of failure if something
  in FB will be broken.
JIRA:        CORE-5210
FBTEST:      bugs.core_5210
"""

import pytest
from time import time
from threading import Timer
from firebird.qa import *

init_script = """
    set term ^;
    create or alter trigger trg_commit on transaction commit as
    begin
        post_event 'dml_event';
    end ^
    set term ;^
    commit;

    recreate table test(id int);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    {'dml_event': 1}
    1: event was SUCCESSFULLY delivered.

    {'dml_event': 1}
    2: event was SUCCESSFULLY delivered.

    {'dml_event': 1}
    3: event was SUCCESSFULLY delivered.

    {'dml_event': 1}
    4: event was SUCCESSFULLY delivered.

    {'dml_event': 1}
    5: event was SUCCESSFULLY delivered.
"""

def send_events(con, command_list):
   cur = con.cursor()
   for cmd in command_list:
      cur.execute(cmd)
   con.commit()

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
   def check_events(seqno: int):
      with act.db.connect() as con:
         timed_event = Timer(1.0, send_events, args=[con, ["insert into test(id) values (rand()*1000)",]])
         with con.event_collector(['dml_event']) as events:
            timed_event.start()
            t1 = time()
            max4delivering = 3
            e = events.wait(max4delivering)
            t2 = time()
         print(e)
         print(f'{seqno}: event was SUCCESSFULLY delivered.' if t2-t1 < max4delivering
               else f'{seqno}: event was NOT delivered for {t2-t1}s (threshold is {max4delivering}s)')

   #
   check_events(1)
   check_events(2)
   check_events(3)
   check_events(4)
   check_events(5)
   # Check
   act.expected_stdout = expected_stdout
   act.stdout = capsys.readouterr().out
   assert act.clean_stdout == act.clean_expected_stdout
