#coding:utf-8

"""
ID:          issue-8418
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8418
TITLE:       UNLIST function. Check work with separator that is specified as one ASCII literal.
DESCRIPTION:
    Provided by red-soft. Original file name: "unlist.test_end_separators.py"
    Code from original test was modified: we check here UNLIST output when literal separator belongs
    to several ASCII ranges, namely:
        control characters (1...31); space; !"#$%&\'()*+,-./
        :;<=>?@
        [\\]^_`
        |}~ and \x7f [`DEL`]

    We check several strings that end with one or more such separators (see 'CHECKED_LISTS').
    Particularly, we also check result when string ends with extremely long sequence containing ~32K
    separators.

NOTES:
    [10.04.2025] pzotov
    1. ascii_char(0) can not be used as separator because FB hangs, see
       https://github.com/FirebirdSQL/firebird/pull/8418#issuecomment-2792358627
    2. ascii_char(13) is not included in the list of checked separators in order avoid excessive
       complexity of expression that is used to construct expected_out.
    3. semicolon (ascii_char(59), ';') has odd behaviour: delay for ~8s can be seen before PREPARE_STATEMENT
       in the trace when string containing ~32K trailing separators is parsed, see:
       https://github.com/FirebirdSQL/firebird/pull/8418#issuecomment-2792461612

    Test execution time: ~30s.
    Checked on 6.0.0.725
"""

import pytest
from firebird.qa import *

db = db_factory()

#separator_list = []
#separator_list.extend( [chr(x) for x in range(58, 65)] )
#CHECKED_LISTS = [ 't.y' + '.' * 65532 ]

separator_list = [chr(x) for x in range(1, 48)]            # control characters (1...31); space; !"#$%&\'()*+,-./
separator_list.extend( [chr(x) for x in range(58, 65)] )   # :;<=>?@
separator_list.extend( [chr(x) for x in range(91, 97)] )   # [\\]^_`
separator_list.extend( [chr(x) for x in range(124, 128)] ) # |}~ and \x7f [`DEL`]


# separator_list.remove( chr(59) ) # perormance impact! See: https://github.com/FirebirdSQL/firebird/pull/8418#issuecomment-2794067192
separator_list.remove( chr(13) ) # remove it in order to simplify expression to construct expected_out
separator_list.remove( chr(26) ) # remove it because ISQL can not rpepare such expr., see #8512

# 65533 --> Statement failed, SQLSTATE = 42000 / ... / -String literal with 65536 bytes exceeds the maximum length of 65535 bytes
CHECKED_LISTS = [ 'q.w.', 'e.r..', 't.y...', 't.y' + '.' * 65532 ]

FIELD_NAME_PREFIX = 'unlist using separator'

queries_lst = ['set list on;',]

expected_out_lst = []
for i_dup, checked_item in enumerate(CHECKED_LISTS):
    base_sql = f"""select /* check ascii_char(%d) */ * from unlist(q'#{checked_item}#', '%s') as a("{FIELD_NAME_PREFIX} {i_dup+1}*ascii_char(%d) = %s");"""
    queries_lst.extend( [ (base_sql % (ord(x), x if x !="'"else x+x, ord(x), x if x != '"' else x+x)).replace('.', x) for x in separator_list ] )
    expected_out_lst.extend( [ f"{FIELD_NAME_PREFIX} {i_dup+1}*ascii_char({ord(x)}) = {x} {y}" for x in separator_list for y in checked_item.rstrip('.').split('.') ] )

#with open('tmp-long-duplicated-separators-at-end.sql', 'w') as f:
#    f.write('\n'.join(queries_lst))

act = isql_act('db', substitutions=[ ('[ \\t]+', ' ') ])

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = '\n'.join(expected_out_lst)
    act.isql(switches=['-q'], input = '\n'.join(queries_lst), combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
