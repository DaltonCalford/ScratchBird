#coding:utf-8

"""
ID:          issue-6344
ISSUE:       6344
TITLE:       Extend trace record for COMMIT/ROLLBACK RETAINING to allow chaining of transaction ids
DESCRIPTION:
    Test prepares trace config with requrement to watch only for TRANSACTION events.
    Then it starts trace session and makes several changes withing retained Tx.
    (this is done by invocation con.commit() method with argument 'retaining = True').

    Every COMMIT_RETAINING event in the trace log must contain following *new* elements:
    1) "INIT_" token with ID of transaction that originated changes; it must be shown in the same line with "TRA_" info;
    2) "New number <NN>" - ID that will be assigned to the next transaction in this 'chain'; it must be shown in separate line.

    We parse trace log and pay attention for lines like: "(TRA_nnn, INIT_mmm, ..."  and "New number <XXX>".
    Each interesting numbers are extracted from these lines and <tx_base> is subtracted from them.
JIRA:        CORE-6095
FBTEST:      bugs.core_6095
NOTES:
    [07-mar-2023] pzotov
    Re-implemented once again in order to escape dependency on differencies between FB major versions and ServerMode value.
    Two asserts play main role:
        * all lines with 'INIT_nnn' must contain only one value (see assert len(tx_init_set) == 1)
        * value in line "New number nnn" must be equal to the value that is specified on NEXT line in "TRA_nnn"
    Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.970
"""

import pytest
import re
from firebird.qa import *

init_script = """
    create sequence g;
    create table test(id int primary key, x int);
    set term ^;
    create trigger test_bi for test active before insert position 0 as
    begin
       new.id = coalesce(new.id, gen_id(g, 1) );
    end
    ^
    create procedure sp_worker(a_x int) as
    begin
        insert into test(x) values(:a_x);
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

trace = ['log_initfini = false',
         'log_transactions = true',
         ]

allowed_patterns = [
                      re.compile('\\s*\\(TRA_\\d+,\\s+INIT_\\d+', re.IGNORECASE)
                     ,re.compile('\\s*New\\s+number\\s+\\d+\\s*', re.IGNORECASE)
                   ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.6')
def test_1(act: Action, capsys):


    with act.trace(db_events=trace):
        with act.db.connect() as con_aux:
            with act.db.connect() as con:
                cur = con.cursor()
                con.execute_immediate('insert into test(x) values(123)')
                con.commit(retaining = True)                    # (TRA_12, ... ; next line: "New number 13"
                cur.callproc('sp_worker', [456])                # (TRA_13, INIT_12, ...
                con.commit(retaining = True)                    # (TRA_13, INIT_12, ... ; next line: "New number 14"
                con.execute_immediate('delete from test')       # (TRA_14, INIT_12, ...
                con.commit(retaining = True)                    # (TRA_14, INIT_12, ... ; next line: "New number 15"

    # Process trace
    tx_base = -1
    tx_init_set = set() # at finish it must contain only one element
    tx_for_subsequent_changes = -1
    for line in act.trace_log:
        if line.rstrip().split():
            #print(line.strip())
            for p in allowed_patterns:
                if p.search(line):
                    print(line.strip())

                    if '(TRA_' in line:
                        words = line.replace(',',' ').replace('_',' ').replace('(',' ').split()
                        # Result:
                        # 1) for tx WITHOUT retaining: ['TRA', '12', 'READ', 'COMMITTED', '|', 'REC', 'VERSION', '|', 'WAIT', '|', 'READ', 'WRITE)']
                        # 2) for tx which is RETAINED: ['TRA', '13', 'INIT', '12', 'READ', 'COMMITTED', '|', 'REC', 'VERSION', '|', 'WAIT', '|', 'READ', 'WRITE)']
                        #                                 0      1     2       3
                        tx_base = int(words[1]) if tx_base == -1 else tx_base
                        if words[2] == 'INIT':
                            
                            tx_init_set.add( int(words[3]) )
                            assert len(tx_init_set) == 1, 'Multiple transactions are specified in "INIT_nnn" tags in the trace: {tx_init_set}'

                            tx_that_finished_now = int(words[1])
                            assert tx_that_finished_now == tx_for_subsequent_changes, 'Finished Tx: {tx_that_finished_now} - NOT equals to "New number": {tx_for_subsequent_changes}'

                            #tx_origin_of_changes = int(words[3]) - tx_base
                            #tx_that_finished_now = int(words[1]) - tx_base
                            # print('Found "INIT_" token in "TRA_" record. Tx that is origin of changes: ', tx_origin_of_changes, '; Tx that finished now:', tx_that_finished_now)
                    elif 'number' in line:
                        tx_for_subsequent_changes = int(line.split()[2]) # 'New number 15' --> 15
                        tx_base = tx_for_subsequent_changes if tx_base < 0 else tx_base
                        # print('Found record with "NEW NUMBER" for subsequent Tx numbers: ', tx_for_subsequent_changes)


    expected_stdout = f"""
        New number {tx_base}
        (TRA_{tx_base}, INIT_{list(tx_init_set)[0]}, CONCURRENCY | WAIT | READ_WRITE)
        New number {tx_base+1}
        (TRA_{tx_base+1}, INIT_{list(tx_init_set)[0]}, CONCURRENCY | WAIT | READ_WRITE)
        New number {tx_base+2}
        (TRA_{tx_base+2}, INIT_{list(tx_init_set)[0]}, CONCURRENCY | WAIT | READ_WRITE)
    """

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
