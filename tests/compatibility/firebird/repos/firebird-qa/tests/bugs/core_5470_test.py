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
FBTEST:      bugs.core_5470
NOTES:
    [04.03.2023] pzotov
    Reimplemented because on Windows act.trace_log ends with *weird* sequence of characters: chr(13) + SPACE + chr(10).
    This causes to incorrect result of act.stdout.find(cmd): it returns -1 for every DDL command and test fails
    (because every <cmd> is multi-line text with correct EOL delimiters).
    In order to check that trace contains all lines of every DDL statement, we add 'tags' into each of these lines,
    and check that final trace output contains all these tags: "ddl_1 line_1", "ddl_1 line_2" etc.
    
    Confirmed bug on 3.0.1.32609 (27-sep-2016)  -- act.trace_log is empty list
    Checked on:
        3.0.2.32703 (21-mar-2017); 3.0.11.33665; 4.0.3.2904; 5.0.0.970 -- all fine.
"""
import os
import pytest
from firebird.qa import *
from pathlib import Path
import time

db = db_factory()

act = python_act('db', substitutions = [('^((?!ddl_).)*$', '')])

tmp_file = temp_file('tmp_trace_5470.txt')

ddl_lst = ["""recreate /* ddl_1 line_1 */

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
         #'log_initfini = false',
         #'log_errors = true',
         'log_statement_finish = true',
         'max_sql_length = 32768', # <<< !!! <<<
         'include_filter = "%(ddl_[[:DIGIT:]]+[[:WHITESPACE:]]+line_[[:DIGIT:]]+)%"',
         ]

expected_trace_log = """
    recreate /* ddl_1 line_1 */
    table /* ddl_1 line_2 */
    t_test /* ddl_1 line_3 */ (x int)

    comment on  /* ddl_2 line_1 */
    table /* ddl_2 line_2 */
    t_test is /* ddl_2 line_3 */
    'foo  /* ddl_2 line_4 */
    /* ddl_2 line_4 */ bar'

    create /* ddl_3 line_1 */
    or /* ddl_3 line_2 */
    alter /* ddl_3 line_3 */
    view /* ddl_3 line_4 */
    v_rio /* ddl_3 line_5 */
    as /* ddl_3 line_6 */
    select * /* ddl_3 line_6 */
    from /* ddl_3 line_7 */
    rdb$database /* ddl_3 line_8 */
"""

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, tmp_file: Path, capsys):
    with act.trace(db_events=trace), act.db.connect() as con:
        for cmd in ddl_lst:
            con.execute_immediate(cmd)
        con.commit()
    
    act.trace_to_stdout() # '\n'.join( [x.replace('\r','').replace('\n','') for x in act.trace_log] )
    for line in act.stdout.split('\n'):
        print(line)

    act.expected_stdout = expected_trace_log
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
