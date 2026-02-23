#coding:utf-8

"""
ID:          issue-5930
ISSUE:       5930
TITLE:       SIMILAR TO is substantially (500-700x) slower than LIKE on trivial pattern matches with VARCHAR data.
DESCRIPTION:
    21.11.2021. Totally re-implemented, package 'psutil' is used in order to get CPU time rather then timedelta.

    We make two calls of psutil.Process(fb_pid).cpu_times() (before and after SQL code) and obtain CPU User Time
    values from each result.
    Difference between them can be considered as much more accurate performance estimation.

    On each calls of procedural code (see variable N_MEASURES) dozen execution of LIKE <pattern%> and
    SIMILAR_TO statements are performed (see variable N_COUNT_PER_MEASURE). Name of procedures which do work:
    'sp_like_test' and 'sp_sim2_test' (first of them uses 'LIKE' statement, second uses 'SIMILAR TO').
    Both procedures uses the same data for handling.

    Each result (difference between cpu_times().user values when PSQL code finished) is added to the list.
    Finally, we evaluate MEDIAN of ratio values between cpu user time which was received for SIMILAR_TO and LIKE statements.
    If this median is less then threshold (see var. SIM2_LIKE_MAX_RATIO) then result can be considered as ACCEPTABLE.

    See also:
    https://psutil.readthedocs.io/en/latest/#psutil.cpu_times

    Confirmed bug on WI-T4.0.0.1575
    Checked on Windows:
        5.0.0.311 : 12.620s  ; 4.0.1.2660 : 13.388s.

    21.11.2021. Checked on Linux (after installing pakage psutil):
        5.0.0.313: 10.107s ; 4.0.1.2668: 8.519s ;
JIRA:        CORE-5664
FBTEST:      bugs.core_5664
NOTES:
    [08.06.2022] pzotov
    Package 'psutil' is now dependency, installed automatically with qa-plugin v 0.15.1.
    Adapted for firebird-qa plugin. Checked on 4.0.1.2692, 5.0.0.509 - both Windows and Linux
"""

import pytest
from firebird.qa import *
import psutil

db = db_factory()

act = python_act('db')

expected_stdout = """
    String ENDS WITH pattern, result: acceptable.
    String STARTS WITH pattern, result: acceptable.
"""

#------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#------------------

###########################
###   S E T T I N G S   ###
###########################
# How many times we call PSQL code (two stored procedures:
# one for performing comparisons based on LIKE, second based on SIMILAR TO statements).
# DO NOT set this value less than 10!
#
N_MEASURES = 21

# How many iterations must be done in each of stored procedures when they work.
# DO NOT set this value less then 10'000 otherwise lot of measures will last ~0 ms
# and we will not able to evaluate ratio properly:
#
N_COUNT_PER_ITER_MAP = { 'starts_with' : 5000, 'ends_with': 1000 }

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made
# using SIMILAR TO vs LIKE. Dozen of measures show that SIMILAR_TO works much *slower*
# than LIKE if <text_to_search> is at the BEGINNING of long string (rather than at the end).
# Because of this, thresholds must differ:
#
SIM2_LIKE_RATIO_MAP = { 'starts_with' : 10.0, 'ends_with': 8.0 }

###########################

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    
    # Patternized code for creating TWO procedures:
    # 1) for applying LIKE
    # 2) for applying SIMILAR TO
    ############################
    sp_ddl = '''
        create or alter procedure sp_%(sp_prefix)s_test (
            n_count int
           ,long_string_form  varchar(20) -- 'starts_with' | 'ends_with'
        ) as
            declare i int = 0;
            declare long_text varchar(32761);
            declare word_to_search varchar(16384);
            declare pattern_to_chk varchar(32761);
            declare v_guid varchar(32755);
        begin
            v_guid = lpad( '', 16384, uuid_to_char(gen_uuid()) );
            v_guid = replace(v_guid,'-','');

            -- ::: NB :::
            -- Text that we want to search (word_to_search)
            -- must either be in the beginning or at the end of string.
            -- No sense to search text if it differs with string at first characters
            -- because evaluating of result for STARTS_WITH will be instant in that case
            -- (rather thanresult for ENDS_WITH).
            -- Also, we have to remove '-' from both text and pattern because this is special
            -- character in SIMILAR TO operator (we do this just for simplification of test).

            word_to_search = replace(left(v_guid, 4096),'-','');

            if ( long_string_form = 'starts_with' ) then
                -- ............ "starts with" ..........
                begin
                    long_text = word_to_search || v_guid;
                    pattern_to_chk = word_to_search || '%%'; ------------ 'ABCDEF' LIKE/SIMILAR_TO 'ABC<wildcard_char>'
                end
            else -- ............ "ends with" ..........
                begin
                    long_text = v_guid || word_to_search;
                    pattern_to_chk = '%%' || word_to_search; ------------ 'ABCDEF' LIKE/SIMILAR_TO '<wildcard_char>DEF'
                end

            while (i < n_count) do
            begin
                -- ##############################################
                -- evaluating result: applying LIKE or SIMILAR_TO
                -- ##############################################
                i = i + iif( long_text %(sp_statement)s pattern_to_chk, 1, 1);
            end
        end
    '''

    with act.db.connect() as con:
        op_map = {'like' : 'LIKE', 'sim2' : 'SIMILAR TO'}
        for sp_prefix,sp_statement in op_map.items():
            con.execute_immediate( sp_ddl % locals() )
            con.commit()

        # Result: procedures with names: 'sp_like_test' and 'sp_sim2_test' have been created.
        # Both of them use input parameter, n_count -- number of iterations.

        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        # test_1: string ENDS with pattern, i.e.:   <str> like '%QWERTY' == vs==  <str> similar to '%QWERTY'
        # test_2: string STARTS with pattern, i.e.: <str> like 'QWERTY%' == vs == <str> similar to 'QWERTY%'
       
        sp_call_data = {}
        for long_text_form, n_count_per_measure in sorted(N_COUNT_PER_ITER_MAP.items()):
            
            ratio_list = []

            for i in range(0, N_MEASURES):
                sp_time = {}
                for sp_name in op_map.keys():
                    fb_info_init = psutil.Process(fb_pid).cpu_times()
                    cur.callproc('sp_' + sp_name + '_test', (n_count_per_measure, long_text_form) )
                    fb_info_curr = psutil.Process(fb_pid).cpu_times()

                    sp_time[ sp_name ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)
                try:
                    ratio_list.append( sp_time['sim2'] / sp_time['like'] )
                    sp_call_data[long_text_form, i] = (sp_time['sim2'], sp_time['like'])
                except ZeroDivisionError as e:
                    print(e)

            max_allowed_ratio = SIM2_LIKE_RATIO_MAP[long_text_form]

            if median(ratio_list) <= max_allowed_ratio:
                # String ENDS WITH pattern, result: acceptable.
                # String STARTS WITH pattern, result: acceptable.
                print('Check: ' + long_text_form + ' - acceptable.')
            else:
                print('\n/* perf_issue_tag */ TOO SLOW search when use "%s": median greater %12.2f' % (long_text_form.upper().replace('_',' '), max_allowed_ratio) )

                print("\nCheck sp_call_data values (k=[", long_text_form, ", i], v = (sp_time['sim2'], sp_time['like'])):" )
                for k,v in sorted(sp_call_data.items()):
                    #print( k,':', 'time of "SIMILAR_TO" = %12.6f' % v[0], 'time of "LIKE" = %12.6f' % v[1], '; ratio = %12.6f' % v[0]/v[1] )
                    print( k,':', '"SIMILAR_TO" = %12.6f' % v[0], ',  "LIKE" = %12.6f' % v[1], '; ratio = ', v[0]/v[1] )

                print('\nTime ratios SIMILAR_TO vs LIKE for "%s":' % long_text_form)
                for i,p in enumerate(ratio_list):
                    print( "%d : %12.2f" % (i,p) )
                print('\nMedian value of time ratios for "%s": %12.2f' % (long_text_form, median(ratio_list)))

        act.expected_stdout = '\n'.join( [ 'Check: ' + k + ' - acceptable.' for k in sorted(N_COUNT_PER_ITER_MAP.keys()) ] )
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
