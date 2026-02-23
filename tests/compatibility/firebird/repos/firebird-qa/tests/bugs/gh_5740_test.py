#coding:utf-8

"""
ID:          issue-5740
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5740
TITLE:       Trace INCLUDE_FILTER with [[:WHITESPACE:]]+ does not work when statement contains newline is issued
DESCRIPTION:
    We create a list of several DDLs which all contain NEWLINE character(s) between keyword and name of DB object.
    Then we launch trace session and execute all these DDLs.
    Finally we check whether trace log contains every DDL or not.
    Expected result: text of every DDL should be FOUND in the trace log.
JIRA:        CORE-5470
NOTES:
    On Windows print(act.trace_log) displays text with EOL containing space between CR and LF, i.e.: chr(13) + space + chr(10):
    ['2024-05-16T12:42:17.8040 ... EXECUTE_STATEMENT_FINISH\r \n', '\tE:\\TEMP\\QA\\FBQA\\TEST_10\\TEST.FDB ... :::1/62705)\r \n', ]

    Space between CR and LF likely is an artifact of list to string conversion done by print() using it's __str__ method.
    Explanation see in reply from pcisar:
    subj: "act.trace_log ends with strange EOL that is: CR + space + NL // Windows"; date: 05-MAR-2023
    In order to get trace text with normal EOLs we have to do:
    trace_txt = '\n'.join( [line.rstrip() for line in act.trace_log] )
    
    Confirmed bug on 4.0.0.483 (date of build: 05-jan-2017).
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

chk_statements_lst = ["""recreate /* ddl_1 line_1 */

            table /* ddl_1 line_2 */



            t_test /* ddl_1 line_3 */ (x int)
        """,
           """comment on  /* ddl_2 line_1 */
    table /* ddl_2 line_2 */


    t_test is /* ddl_2 line_3 */
            'foo  /* ddl_2 line_4 */
             /* ddl_2 line_4 */ bar'
    """,
"""

            create /* ddl_3 line_1 */
    or /* ddl_3 line_2 */

    alter /* ddl_3 line_3 */
                    view /* ddl_3 line_4 */

                    v_rio /* ddl_3 line_5 */

                    as /* ddl_3 line_6 */
                    select * /* ddl_3 line_6 */
                    from /* ddl_3 line_7 */

                    rdb$database /* ddl_3 line_8 */
    """]

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_errors = true',
         'log_statement_finish = true',
         'max_sql_length = 65500',
         'include_filter = "%(ddl_[[:DIGIT:]]+[[:WHITESPACE:]]+line_[[:DIGIT:]]+)%"',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace), act.db.connect() as con:
        for cmd in chk_statements_lst:
            con.execute_immediate(cmd)
        con.commit()

    trace_txt = '\n'.join( [line.rstrip() for line in act.trace_log] )
    missed_cnt = 0
    for sttm in [c.rstrip() for c in chk_statements_lst]:
        if trace_txt.find(sttm) < 0:
            missed_cnt += 1
            if missed_cnt == 1:
                print('Missed in the trace log:')
            
            print('----- sttm start -----')
            for x in [x.strip() for x in sttm.split('\n')]:
                print(x)
            print('----- sttm finish ----')
    
    if missed_cnt:
        print('----- trace start -----')
        for x in trace_txt.split('\n'):
            print(x)
        print('----- trace finish ----')

    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
