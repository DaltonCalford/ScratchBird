#coding:utf-8

"""
ID:          issue-3474
ISSUE:       3474
TITLE:       Client receive event's with count equal to 1 despite of how many times EVENT was POSTed in same transaction
DESCRIPTION:
    We create stored procedure as it was specified in the ticket, and call it with input arg = 3.
    This mean that we have to receive THREE events after code which calls this SP will issue COMMIT.
JIRA:        CORE-3095
FBTEST:      bugs.core_3095
NOTES:
    [11.10.2023] pzotov
    Added delay for 1 second before call procedure which makes POST_EVENT.
    Otherwise this test constantly fails on Linux with ServerMode = Super.
    One can assume that this is due to the fact that events are delivered to the client BEFORE the client is ready to process them.
    Thanks to Alex for suggestion.
"""

import pytest
from firebird.qa import *
import time
init_script = """
    set term ^;
    create or alter procedure sp_test (a_evt_count int) as
    begin
        while (a_evt_count > 0) do
        begin
            post_event('loop');
            a_evt_count = a_evt_count - 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    NUM_OF_EVENTS = 50
    with act.db.connect() as con:
        c = con.cursor()
        with con.event_collector(['loop']) as events:
            
            time.sleep(1) # <<< NB <<<

            c.execute(f'execute procedure sp_test({NUM_OF_EVENTS})')
            con.commit()
            evt = events.wait(30)
            for k,v in evt.items():
                print(k,':',v)
    expected_stdout = f"""
       loop : {NUM_OF_EVENTS}
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
