#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check work with separator that is specified as one ASCII literal.
DESCRIPTION:
    Provided by red-soft. Original file name: "unlist.test_separators.py"
    Code from original test was modified : we check here UNLIST output when separator is every
    ASCII character except letters (A...Z, a..z), chr(13) and chr(26).
NOTES:
    [10.04.2025] pzotov
    1. ascii_char(0) can not be used as separator because FB hangs, see
       https://github.com/FirebirdSQL/firebird/pull/8418#issuecomment-2792358627
    2. ascii_char(13) is not included in the list of checked separators in order avoid excessive
       complexity of expression that is used to construct expected_out.
    Checked on 6.0.0.725
"""

import pytest
from firebird.qa import *

db = db_factory()

FIELD_NAME_PREFIX = 'unlist using separator'
SOURCE_LIST = '1.2.3.4.5'
base_sql = f"""select /* check ascii_char(%d) */ * from unlist(q'#{SOURCE_LIST}#', '%s') as a("{FIELD_NAME_PREFIX} ascii_char(%d) = %s");"""

separator_list = [chr(x) for x in range(1, 48)]            # control characters (1...31); space; !"#$%&\'()*+,-./
separator_list.extend( [chr(x) for x in range(58, 65)] )   # :;<=>?@
separator_list.extend( [chr(x) for x in range(91, 97)] )   # [\\]^_`
separator_list.extend( [chr(x) for x in range(124, 128)] ) # |}~ and \x7f [`DEL`]

separator_list.remove( chr(13) ) # remove it in order to simplify expression to construct expected_out
separator_list.remove( chr(26) ) # remove it because ISQL can not rpepare such expr., see #8512

queries_lst = ['set list on;',]
queries_lst.extend( [ (base_sql % (ord(x), x if x !="'"else x+x, ord(x), x if x != '"' else x+x)).replace('.', x) for x in separator_list ] )

act = isql_act('db', substitutions=[ ('[ \\t]+', ' ') ])

expected_out_lst = [ f"{FIELD_NAME_PREFIX} ascii_char({ord(x)}) = {x} {y}" for x in separator_list for y in SOURCE_LIST.split('.') ]

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = '\n'.join(expected_out_lst)
    act.isql(switches=['-q'], input = '\n'.join(queries_lst), combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
