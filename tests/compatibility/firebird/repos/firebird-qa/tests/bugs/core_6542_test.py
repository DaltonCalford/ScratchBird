#coding:utf-8

"""
ID:          issue-6769
ISSUE:       6769
TITLE:       More efficient implementation of SUBSTRING for UTF8 character set
DESCRIPTION:
    21.11.2021. Totally re-implemented, package 'psutil' must be installed.

    We make two calls of psutil.Process(fb_pid).cpu_times() (before and after SQL code) and obtain CPU User Time
    values from each result.
    Difference between them can be considered as much more accurate performance estimation.

    We make <N_MEASURES> calls of two stored procedures with names: 'sp_ufss_test' and 'sp_utf8_test'.
    Each procedure has input argument which is required number of iterations with evaluation result of SUBSTRING.
    Number of iterations it set in variable N_COUNT_PER_MEASURE.

    Each result (difference between cpu_times().user values when apropriate procedure finishes) is added to the list.
    Finally, we evaluate MEDIAN of ratios between cpu user time which was received for UTF8 and UNICODE_FSS charsets.
    If this median is less then threshold (see var. UTF8_TO_UFSS_MAX_RATIO) then result can be considered as ACCEPTABLE.

    See also: https://psutil.readthedocs.io/en/latest/#psutil.cpu_times

    Confirmed poor performance on 3.0.8.33446 (16.04.2021) and 4.0.0.2422 (15.04.2021):
    median ratios are: 15.57 for FB-3 and 13.57 for FB-4.

    Checked on Windows, builds: 3.0.8.33527, 4.0.1.2660, 5.0.0.310.
    Result: median ratios are 1.0 in all cases.
    When heavy workload (created by another apps) exist, threshold must be increased up to ~1.3.

    21.11.2021. Checked on Linux (after installing pakage psutil):
      5.0.0.313 SS:   4.159s
      4.0.1.2668 SS:  3.786s
      3.0.8.33540 SS: 3.274s

    Old comment about other string functions (data obtained using datediff() values): // DO NOT DELETE!
    =================
      SUBSTRING remains most problematic from performance POV.
      Ratio TIME_UTF8 / TIME_UFSS  for other string functions (checked on 4.0.0.2448):
          right(..., 10):      0.4;  right(..., 8000):  0.4
          left(...,10):        1.14;  left(..., 8000):   0.3
          reverse():           0.7
          upper() / lower():   0.576 ... 0.635
          position():          0.5
          trim():              0.35
          octet_length():      1.00
          char_length():       0.632
          bit_length():        1.00
          ascii_val():         1.00
          hash():              1.00
          overlay():           0.31
          replace():           0.5
          lpad()/rpad():       0.73
    =================
JIRA:        CORE-6542
FBTEST:      bugs.core_6542
NOTES:
    [20.06.2022] pzotov
    Checked on 4.0.1.2692, 3.0.8.33535 -- both Windows and Linux.
"""

import os
import psutil
import time

import pytest
from firebird.qa import *

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

db = db_factory()

act = python_act('db')

expected_stdout = """
    Duration ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0.8')
def test_1(act: Action, capsys):
   
    ###########################
    ###   S E T T I N G S   ###
    ###########################
    # How many times we call PSQL code (two stored procedures:
    # one for performing comparisons based on LIKE, second based on SIMILAR TO statements):
    N_MEASURES = 31

    # How many iterations must be done in each of stored procedures when they work:
    N_COUNT_PER_MEASURE = 100000

    # Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
    #
    UTF8_TO_UFSS_MAX_RATIO = 3.00
    #############################

    cset_map = {'sp_ufss': 'unicode_fss', 'sp_utf8': 'utf8'}

    with act.db.connect() as con:
        for k,v in cset_map.items():
            cset_brief = k
            cset_name = v
            sp_cset_ddl = f'''    create or alter procedure {cset_brief}_test (
                    n_count int = 100000
                ) as
                    declare str1 varchar(8000) character set {cset_name}; -- unicode_fss /  utf8
                    declare str2 varchar(10) character set {cset_name}; -- unicode_fss /  utf8
                begin
                    str1 = lpad('abcd', 8000, '--');
                    while (n_count > 0) do
                    begin
                        str2 = substring(str1 from 1 for 10);
                        n_count = n_count - 1;
                    end
                end
            '''
            con.execute_immediate(sp_cset_ddl)
            con.commit()


    sp_time = {}
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
        for i in range(0, N_MEASURES):
            for x_charset in cset_map.keys():
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( x_charset + '_test', (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()

                sp_time[ x_charset, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    ratio_lst = []
    for i in range(0, N_MEASURES):
        ratio_lst.append( sp_time['sp_utf8',i]  / sp_time['sp_ufss',i]  )

    median_ratio = median(ratio_lst)

    print( 'Duration ratio: ' + ('acceptable' if median_ratio < UTF8_TO_UFSS_MAX_RATIO else '/* perf_issue_tag */ POOR: %12.2f, more than threshold: %12.2f' % ( median_ratio, UTF8_TO_UFSS_MAX_RATIO ) ) )
    if median_ratio >= UTF8_TO_UFSS_MAX_RATIO:
        print('Ratio statistics for %d measurements' % N_MEASURES)
        for p in ratio_lst:
            print( '%12.2f' % p )


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
