#coding:utf-8

"""
ID:          issue-7548
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7548
TITLE:       SET BIND OF TIMESTAMP WITH TIME ZONE TO CHAR is not working with UTF8 connection charset
DESCRIPTION:
    Test checks all character sets defined in CHARSET_MAP dictionary (see firebird/driver/core.py).
    No errors must occur for all of them.
NOTES:
    [19.04.2023] pzotov
    Confirmed bug on 4.0.3.2929, 5.0.0.1014
    Checked on 4.0.3.2931, 5.0.0.1021 - all OK.

    [31.08.2023] pzotov
    Refactored:
    * output of checked timestamp can be suppressed, see redirection: 'out {os.devnull}';
    * there is no need to display the same error messages for many charsets; rather, we can use dictionary
      with key = error message and val = list of character sets which did not pass check (i.e. for which
      we get "SQLSTATE = 22001 / ... / -string right truncation / -expected length N, actual M";
    * removed unneeded substitutions, use 'init' arg for db_factory in order to make test run faster;
"""

import os
import pytest
from firebird.qa import *
from firebird.driver import CHARSET_MAP
from collections import defaultdict

db = db_factory(init = 'recreate table test(f01 timestamp with time zone);')

ts = '19-APR-2023 21:15:19.0110'
test_sql = f"""
    set list on;
    delete from test;
    insert into test values(timestamp '{ts}');
    set term ^;
    execute block as
    begin
        set bind of timestamp with time zone to char;
    end
    ^
    set term ;^
    out {os.devnull};
    select * from test;
    out;
    rollback;
"""

act = python_act('db')

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, capsys):
    cset_list = []
    with act.db.connect() as con:
        with con.cursor() as cur:
            cur.execute("select trim(c.rdb$character_set_name) from rdb$character_sets c where coalesce(c.rdb$system_flag,0)<>0 and c.rdb$character_set_name not in ('NONE', 'OCTETS')")
            cset_list = [x[0] for x in cur.fetchall() if x[0] in CHARSET_MAP]

    # Result: cset_list = ['ASCII', 'UNICODE_FSS', 'UTF8', 'SJIS_0208', ..., 'KOI8U', 'WIN1258', 'GBK', 'GB18030']

    cset_fails = defaultdict(list)
    for cset_i in cset_list:
        act.reset()
        act.expected_stdout = ''
        act.isql(switches=['-q'], charset = cset_i, input = test_sql, combine_output = True)
        if act.clean_stdout == act.clean_expected_stdout:
            pass
        else:
            cset_fails[act.clean_stdout] += cset_i,

    if cset_fails:
        print('Detected fails:')
        for k,v in sorted(cset_fails.items()):
            print('Character sets:', ','.join(v))
            print('Result:')
            print(k)
            print('')

    assert '' == capsys.readouterr().out
