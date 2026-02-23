#coding:utf-8

"""
ID:          issue-7868
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7868
TITLE:       SET AUTOTERM ON/OFF. Additional tests.
DESCRIPTION:
    Test verifies several cases which were discussed in
    https://github.com/FirebirdSQL/firebird/pull/7868

    Each script (named as <K>) is performed  several times, depending on values of 'call_mode', 'autot_mode' and 'echo_mode':
        1. call_mode = 'via_input' ==> script <K> is saved in .sql which is further called by ISQL with '-input <script.sql>' and:
            1.1) with command switch '-autot'
            1.2) without '-autot' but with addiing 'set autoterm;' in this script before ISQL run
        2. call_mode = 'using_pipe' ==> script <K> is also saved in .sql and we use PIPE, like 'cat <script> | isql ...' and isql is used:
            2.1) with command switch '-autot' // see: autot_mode = '-autot_switch'
            2.2) without '-autot' but with addiing 'set autoterm;' in this script before ISQL run // see: autot_mode = 'set_autot'
    We have to REPEAT all these cases THREE times:
        A. echo_mode = 'no_echo'      ==> neither 'set echo on' nor '-echo' commands present (i.e. script is executed 'silently');
        B. echo_mode = '-echo_switch' ==> when ISQL is called with '-echo' command switch // see: autot_mode = '-autot_switch'
        C. echo_mode = 'set_echo_on'  ==> when script contain 'set echo on;' command // see: autot_mode = 'set_autot'
    Each script either must not issue any output or raises error.

NOTES:
    Checked on 6.0.0.154
"""

from pathlib import Path
import subprocess
import re
from difflib import ndiff
from difflib import unified_diff
import time

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db') #, substitutions = sub_lst)

STARTING_SPACE_REPLACEMENT = '#'

tmp_sql = temp_file('tmp_autoterm.sql')
tmp_log = temp_file('tmp_autoterm.log')

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_sql: Path, tmp_log: Path, capsys):

    # K = query; V = expected_out
    test_dict = {
        """
            /*set term*; execute block as begin end**/
        """
        :
        """
        """
        , ###############################################
        """
            execute block as
            begin
            end;

            execute block as
            begin
            end;
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1825231225

        """
            -- q;w;e
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1825231225
        """
            -- q;w;e;
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1825446834
        """
            -- foo
            select unknown_column from rdb$database;
        """
        :
        """
            Statement failed, SQLSTATE = 42S22
            Dynamic SQL Error
            -SQL error code = -206
            -Column unknown
            -"UNKNOWN_COLUMN"
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1825452390
        """
            /* x
            --> */
        """
        :
        """
        """

        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1826352683
        """
            set term ^;
            execute block as
            begin
            end
            ^^
            set term ;^
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1826361804
        """
            set term ^;
            execute block as
            begin
            end
            ^
            set term ^;
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1826254181
        """
            ;;
        """
        :
        """
        """
        , ###############################################
          # https://github.com/FirebirdSQL/firebird/pull/7868#issuecomment-1825592875
        """
            execute block as
                declare n int;
            begin
                /*
                */

                /*
                1
                */

                /*
                1
                2
                3
                */

                /*
                1
                2
                3
                4
                */
            end;
        """
        :
        """
        """
        , ###############################################
        """
          /* multi-line comment without semicolon, number of rows = 1 */
        """
        :
        """
        """
        , ###############################################
        """
          /* multi-line comment 
          without semicolon, 
          number of rows > 1 */
        """
        :
        """
        """
        , ###############################################
        """
          -- single-line comment without semicolon, trivial.
        """
        :
        """
        """
        , ###############################################
        """
          -- single-line comment without semicolon but with multi-lined incompleted comment: /* foo
        """
        :
        """
        """
        , ###############################################

        """
          -- single-line comment without semicolon but with multi-lined comment included: /* bar */
        """
        :
        """
        """
    }

    # For each K from test_dict we check output:
    #     1. call_mode = 'via_input' ==> script <K> is saved in .sql which is further called by ISQL with '-input <script.sql>' and:
    #         1.1) with command switch '-autot'
    #         1.2) without '-autot' but with addiing 'set autoterm;' in this script before ISQL run
    #     2. call_mode = 'using_pipe' ==> script <K> is also saved in .sql and we use PIPE, i.e. 'cat <script> | isql ...' and isql is used:
    #         2.1) with command switch '-autot' // see: autot_mode = '-autot_switch'
    #         2.2) without '-autot' but with addiing 'set autoterm;' in this script before ISQL run // see: autot_mode = 'set_autot'
    # We have to REPEAT all these cases THREE times:
    #     A. When neither 'set echo on' nor '-echo' commands present (i.e. script is executed 'silently');
    #     B. When ISQL is called with '-echo' command switch // see: autot_mode = '-autot_switch'
    #     C. When script contain 'set echo on;' command // see: autot_mode = 'set_autot'

    subst_pairs_lst = [ ('(-)?At line \\d+.*', ''), ] 
    brk_flag = 0
    
    for echo_mode in ('no_echo', '-echo_switch', 'set_echo_on'):

        if brk_flag:
            break
        for k, v in test_dict.items():
            if brk_flag:
                break

            min_indent = min([len(x) - len(x.lstrip()) for x in k.splitlines() if x.strip()])
            lstrip_txt = '\n'.join( [x[min_indent:] for x in k.rstrip().splitlines()] )

            for autot_mode in ('-autot_switch', 'set_autot'):
    
                if brk_flag:
                    break

                if echo_mode == 'no_echo':
                    expect_txt = v
                else:
                    expect_txt = '\n'.join( (lstrip_txt, v.lstrip()) )
                    if autot_mode == 'set_autot':
                        expect_txt = '\n'.join( ( 'set autoterm on;', expect_txt.lstrip() ) )

                expect_lst = [''.join( (STARTING_SPACE_REPLACEMENT, x.strip()) ) for x in expect_txt.strip().splitlines()] # ['#', '#1......', '#', '#', '#2....']

                for call_mode in ('via_input', 'using_pipe'):

                    if brk_flag:
                        break

                    with open(tmp_sql, 'w') as f:
                         if echo_mode == 'set_echo_on':
                             f.write('set echo on;\n')
                         if autot_mode == 'set_autot':
                             f.write('set autoterm on;\n')
                         
                         f.write(lstrip_txt.strip())

                    # we run script using 'isql -in <script.sql>'
                    # Instead of act.isql() we use here subprocess.run() and redirect output to log file.
                    with open(tmp_log, 'w') as g:
                        isql_params = [
                            act.vars['isql']
                           ,'-q'
                           ,'-user', act.db.user
                           ,'-password', act.db.password
                           ,act.db.dsn
                        ]

                        if echo_mode == '-echo_switch':
                            isql_params.extend(['-echo'])

                        if autot_mode == '-autot_switch':
                            isql_params.extend(['-autot'])

                        if call_mode == 'via_input':
                            isql_params.extend(['-input', str(tmp_sql)])
                            subprocess.run( isql_params
                                            ,stdout = g
                                            ,stderr = subprocess.STDOUT
                                          )
                        else:
                            subprocess.run( isql_params
                                            ,input = str.encode(tmp_sql.read_text())
                                            ,stdout = g
                                            ,stderr = subprocess.STDOUT
                                          )


                    with open(tmp_log, 'r') as g:
                        actual_txt = g.read().strip()

                    actual_lst = []
                    for x in actual_txt.splitlines():
                        if not x.strip():
                            actual_lst.append( ''.join( (STARTING_SPACE_REPLACEMENT, x) ) )
                        else:
                            for sub_pair in subst_pairs_lst:
                                # sub_pair: ('(-)?At line \\d+.*', '')
                                x = re.sub( sub_pair[0], sub_pair[1], x ).strip()
                                if x:
                                    actual_lst.append( ''.join( (STARTING_SPACE_REPLACEMENT, x) ) )

                    if len([p for p in unified_diff(expect_lst, actual_lst)]):
                        print('AT LEAST ON CASE FAILED:')
                        print('* echo_mode=',echo_mode)
                        print('* autot_mode=',autot_mode)
                        print('* call_mode=',call_mode)
                        print('* lstrip_txt >>>',lstrip_txt,'<<<')
                        print('* expect_txt >>>',expect_txt,'<<<')
                        print('* expect_lst >>>',expect_lst,'<<<')
                        print('* actual_lst >>>',actual_lst,'<<<')
                        print('* number of differences:',len([p for p in unified_diff(expect_lst, actual_lst)]))

                        for p in unified_diff(expect_lst, actual_lst):
                            print(p)
                        brk_flag = 1

                        break
                
                else:
                   # we run script using PIPE mechanism
                   pass

    act.expected_stdout = ''
    act.stdout = capsys.readouterr().out
    assert act.stdout == act.expected_stdout
