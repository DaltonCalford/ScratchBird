#coding:utf-8

"""
ID:          issue-5268
ISSUE:       5268
TITLE:       Detach using Linux client takes much longer than from Windows
DESCRIPTION:
    We measure APPROXIMATE time that is required for detaching from database by evaluating number of seconds that passed
    from UNIX standard epoch time inside ISQL and writing it to log. After returning control from ISQL we evaluate again
    that number by calling Python 'time.time()' - and it will return value upto current UTC time, i.e. it WILL take in
    account local timezone from OS settings (this is so at least on Windows). Thus we have to add/substract time shift
    between UTC and local time - this is done by 'time.timezone' command.
    On PC-host with CPU 3.0 GHz and 2Gb RAM in almost all cases difference was less than 1000 ms, so it was decided
    to set MAX_DETACH_TIME_THRESHOLD = 1200 ms.
JIRA:        CORE-4977
FBTEST:      bugs.core_4977
"""

import pytest
import time
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    ################################
    MAX_DETACH_TIME_THRESHOLD = 1200
    ################################

    act.script = """
        set heading off;
        set term ^;
        execute block returns(dd bigint) as
        begin
            dd = datediff(second from timestamp '01.01.1970 00:00:00.000' to cast('now' as timestamp));
            suspend;
        end
        ^
    """
    act.execute()
    ms_before_detach = 0
    for line in act.stdout.splitlines():
        # ::: NB  ::: do NOT remove "and line.split()[0].isdigit()" if decide to replace subprocess.call()
        # with pipe-way like: runProgram('isql',[dsn,'-q','-o',sqllog.name], sqltxt) !!
        # String like: 'Database ....' does appear first in log instead of result!
        splitted = line.split()
        if splitted and splitted[0].isdigit():
            ms_before_detach = int(splitted[0])
    time_for_detach_ms = int((time.time() - ms_before_detach - time.timezone) * 1000)
    assert time_for_detach_ms <= MAX_DETACH_TIME_THRESHOLD, f'{time_for_detach_ms=} - greater than {MAX_DETACH_TIME_THRESHOLD=}'
