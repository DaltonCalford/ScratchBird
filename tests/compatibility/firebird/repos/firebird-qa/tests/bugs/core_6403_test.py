#coding:utf-8

"""
ID:          issue-6641
ISSUE:       6641
TITLE:       Some PSQL statements may lead to exceptions report wrong line/column
DESCRIPTION:
  We make list of strings, each of them is a multi-line statement starting with 'EXECUTE BLOCK ...'.
  Inside every EB one of following loops is checked:
    * while (<expr>) do ...
    * for select <expr> into ...
  -- where <expr> uses several arguments (because of loop) and eventually fails with zero division.
  In the BEGIN ... END block we have several lines of code which operates with arguments (see ticket examples).
  When <expr> fails then we catch error stack and parse it by searching line '- At block line: N, col: C'.
 These values (N and C) are compared later with those which are expected.

  In order to proper evaluate expected values of (N,C) we split source expression onto separate lines and numerate them.
  Also, within each line we search special marker: "/*!*/" (without qoutes).
  If line contains this marker then we find its position and subtract 5 from it (this is length of "while" word).
  NB: this requires that between "for" and "/*!*/" we must put two space characters (see below).

  Finally, we compare values in two dictionaries:
    source_expr_positions_map  - K = sequential number of exprerssion in the "main list"; V = line/col of token that causes problem;
    actual_stack_positions_map - K = the same as in source_expr_positions_map; V = line/col values that were parsed from error stack.
  All pairs for apropriate expressions must be equal.

  If some difference will be found then we show this error stack, expected line/col coordinates and actual ones, as in this example:
 ==================
    3 UNEXPECTED MISMATCH between expected and actual values of line/col:
    * expected (position of problematic token): 6, 5
    * actual (result of error message parsing): 10, 9
    * source message of error:
    Error while executing SQL statement:
    - SQLCODE: -802
    - arithmetic exception, numeric overflow, or string truncation
    - Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    - At block line: 10, col: 9
    Expression:
    ....:....1....:....2....:....3....:....4....:....5....:....6....:....7....:....8
    : 12345678901234567890123456789012345678901234567890123456789012345678901234567890
    1 : execute block
    2 : as
    3 :     declare n int = 5;
    4 :     declare c int;
    5 : begin
    6 :     for  /*!*/ select 100/:n from rdb$types into c
    7 :     -- ^^ NB: two spaces here.
    8 :     do begin
    9 :         insert into test(x) values(sqrt(:n));
    10 :         n = n - 1;
    11 :     end
    12 : end
  ==================
JIRA:        CORE-6403
FBTEST:      bugs.core_6403
"""

import pytest
import re
from firebird.qa import *

db = db_factory(init='recreate table test (x int) ;')

act = python_act('db')

expected_stdout = """
    Test 0 PASSED
    Test 1 PASSED
    Test 2 PASSED
    Test 3 PASSED
    Test 4 PASSED
"""

sttm_list = [
    """execute block
    as
        declare n integer = 0;
    begin
        while/*!*/ (1 / n > 0)
        do
        begin
            n = n - 1;
        end
    end
    """,
       """execute
     block as declare n integer = 1; begin
     while/*!*/
     (
                                        1 / n > 0
                                   )
         do
         begin
                   n
             = n - 1;
         end
     end
     """,
        """execute block
    as
    begin
        while/*!*/ ( (select coalesce(t.x,0) from rdb$database r left join test t on 1=1) <= 1 )
        do
        begin
            insert into test(x) values(1);
                insert into test(x) values(2);
                    insert into test(x) values(3);
        end
    end
    """,
       """execute block
    as
        declare n int = 5;
        declare c int;
    begin
        for  /*!*/ select 100/:n from rdb$types into c
        -- ^^ NB: two spaces here.
        do begin
            insert into test(x) values(sqrt(:n));
            n = n - 1;
        end
    end
    """,
       """execute block
    as
        declare n int = 5;
        declare c int;
    begin
        delete from test;
        while ( n >= 0 ) do
        begin
            for  /*!*/ execute statement (
                'select 100 / cast(? as int)
                 from rdb$database
                '
            ) ( n ) into c
            -- ^^ NB: two spaces here.
            do begin
                -- insert into test(x) values(sqrt(:n));
                insert into test(x) values( :n );
            end
            n = n - 1;
        end
    end
    """
    ]

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    sttm_map = {}
    source_expr_positions_map = {}
    for i, stm in enumerate(sttm_list):
        sttm_lines = stm.splitlines()
        for j, line in enumerate(sttm_lines):
            problematic_expr_position = line.find('/*!*/') - 5
            sttm_map[i, j+1] = (line, 1 + problematic_expr_position)
            if problematic_expr_position >= 0:
                source_expr_positions_map[i] = (j+1, 1 + problematic_expr_position)
    # - At block line: 3, col: 1
    pattern = re.compile('(-)?At\\s+block\\s+line(:)?\\s+\\d+(,)?\\s+col(:)?\\s+\\d+', re.IGNORECASE)
    actual_stack_positions_map = {}
    with act.db.connect() as con:
        for i, cmd in enumerate(sttm_list):
            try:
                con.execute_immediate(cmd)
            except Exception as e:
                for msg in e.args[0].splitlines():
                    if pattern.match(msg):
                        # '- At block line: 3, col: 1' ==>  ' 3  1'
                        line_col_pair = ''.join([s if s.isdigit() else ' ' for s in msg]).split()
                        actual_stack_positions_map[i] = (int(line_col_pair[0]), int(line_col_pair[1]), e.args[0])
    #
    for k, v in sorted(source_expr_positions_map.items()):
        w = actual_stack_positions_map.get(k)
        if w and v[:2] == w[:2]:
            print(f'Test {k} PASSED') # expected line/col in source code matches error stack pair.
        else:
            print(f'{k} UNEXPECTED MISMATCH between expected and actual values of line/col:')
            print(f'  * expected (position of problematic token): {v[0]}, {v[1]}')
            if w:
                print(f'  * actual (result of error message parsing): {w[0]}, {w[1]}')
                print('  * source message of error:')
                for i in w[2].splitlines():
                    print(' ' * 8, i)
            else:
                print('  * actual: NONE')

            print('Expression:')
            for i, s in enumerate(sttm_list[k].splitlines()):
                if i == 0:
                    print(f'|      {"".join(["....:...." + str(j) for j in range(1, 9)])}')
                    print(f'|    : {"1234567890" * 8}')
                print(f'|{"{:4g}".format(i+1)}: {s}')
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
