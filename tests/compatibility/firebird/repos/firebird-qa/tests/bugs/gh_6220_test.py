#coding:utf-8

"""
ID:          issue-6220
ISSUE:       6220
TITLE:       Slow performance when executing SQL scripts as non-SYSDBA user
DESCRIPTION:
    Confirmed poor ratio between sysdba/non_dba time on 3.0.4.33054 (03-oct-2018): ratio was about 3.5 ... 4.0.
    Bug was fixed in 3.0.5, 11-nov-2018, build #33081 ( https://www.sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1295388&msg=21800371 ).
    Recent FB versions have no problem, ratio is about 1.05 ... 1.15.

    Problem can be preproduced if each DML prepare will require lot of work, for example - check access rights for many nested views before
    update underlying table.
  NOTES:
JIRA:        CORE-5966
FBTEST:      bugs.gh_6220
NOTES:
    [30.11.2021] pzotov
    Completely reworked: use psutil package in order to take in account CPU User Time instead of evaluating datediff.
    Difference between CPU User Time values which are received before and after each call of stored procedure can be considered as much more
    accurate way to evaluate time that was spent for job.

    Test creates table and lot of nested views based on this table and (furter) on each other.
    Then stored procedure is created with DYNAMIC SQL, and we intentionally do NOT use parameters in it (because we want engine to compile
    statement on each iteration).
    Procedure performs loop for <N_COUNT_PER_MEASURE> times with executing statement like 'UPDATE <top_level_view> SET <new values here on each iteration>'.
    Engine needs to determine access rights not only for <top_level_view> but also for all NESTED views and, eventually, for underlying table which must be
    updated.
    We call this procedure N_MEASURES times for SYSDBA and the same (N_MEASURES times) for NON_DBA user, and store difference values between CPU User Time
    counters for each call. These differences then are divided (value that was received for NON_DBA is divided for apropriate value of SYSDBA).
    MEDIAN of these ratios must be LESS than threshold MAX_TIME_RATIO.

    [21.06.2022] pzotov
    Confirmed problem on  3.0.4.32972 (build of 11-may-2018).
    Refactored initial DDL (made it shorter because of dynamic list/string creation - see 'NESTED_LIMIT' variable).
    Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.509 -- both Windows and Linux.

    [09.03.2023] pzotov
    Increased threshold from 1.3 to 1.8
    NB. Confirmed poor ratio between sysdba/non_dba time on 3.0.4.33054 (03-oct-2018): ratio was about 3.5 ... 4.0.
"""
import psutil
import pytest
from firebird.qa import *

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################
# How many times we call stored procedure 'sp_run':
#
N_MEASURES = 30

# How many iterations (EXECUTE STATEMENT invocations) must be done in SP
#
N_COUNT_PER_MEASURE = 100

# Maximal allowable threshold for MEDIAN of ratios:
#
# before 09.03.2023 MAX_TIME_RATIO = 1.3
MAX_TIME_RATIO = 1.8

# Max level of views nesting:
#
NESTED_LIMIT=99

tmp_user = user_factory('db', name='tmp$gh_6220', password='123', plugin = 'Srp')

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

MSG_PREFIX = 'Durations ratio for execution by NON-DBA vs SYSDBA:'
expected_stdout = f"""
    {MSG_PREFIX} acceptable.
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, tmp_user: User, capsys):

    init_ddl = [
        """
            set bail on;
            recreate sequence g;
            create table test(id int primary key, measure int, iter_no int);

            create view v_test1 as select * from test;
        """
        ,'\n'.join( ['create view v_test%s as select * from v_test%s;' % (str(i), str(i-1)) for i in range(2,NESTED_LIMIT+1) ] )
        ,f"""
            commit;

            set term ^;
            create procedure sp_run(a_measure smallint, n_iterations int) as
            begin
                while ( n_iterations > 0 ) do
                begin
                    execute statement 'update v_test{NESTED_LIMIT} set measure = ' || a_measure || ', iter_no = ' || n_iterations;
                    n_iterations = n_iterations - 1;
                end

            end
            ^
            set term ;^
            commit;

            grant execute on procedure sp_run to {tmp_user.name};
            grant select, update on v_test{NESTED_LIMIT} to {tmp_user.name};
            grant usage on sequence g to {tmp_user.name};
            commit;

            insert into v_test{NESTED_LIMIT}(id, measure, iter_no) values(0,-1,-1);
            commit;
        """
    ]

    act.expected_stdout = ''
    act.isql(input = '\n'.join(init_ddl), combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #------------------------------------------------------------

    pid_sttm = 'select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection'
    with act.db.connect() as con_sysdba, act.db.connect(user = tmp_user.name, password = tmp_user.password) as con_nondba:
        cur_sysdba = con_sysdba.cursor()
        cur_nondba = con_nondba.cursor()
        cur_sysdba.execute(pid_sttm)
        cur_nondba.execute(pid_sttm)
        pid_sysdba = cur_sysdba.fetchone()[0]
        pid_nondba = cur_nondba.fetchone()[0]

        sp_time = {}
        for who in (act.db.user, tmp_user.name):
            fb_pid = pid_sysdba if who == act.db.user else pid_nondba
            con = con_sysdba if who == act.db.user else con_nondba
            cur = cur_sysdba if who == act.db.user else cur_nondba

            for iter_no in range(0, N_MEASURES):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( 'sp_run', (iter_no, N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ who, iter_no ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)
            con.rollback()


    ratio_lst = []
    for iter_no in range(0, N_MEASURES):
        ratio_lst.append( sp_time[tmp_user.name,iter_no]  / sp_time[act.db.user,iter_no]  )

    median_ratio = median(ratio_lst)

    if median_ratio < MAX_TIME_RATIO:
        print(MSG_PREFIX + ' acceptable.')
    else:
        print(MSG_PREFIX + ' /* perf_issue_tag */ UNACCEPTABLE, greater than threshold = ', MAX_TIME_RATIO)

        #print( 'Check result of %d measures:' % N_MEASURES )
        #for k,v in sorted(sp_time.items()):
        #    print(k,':',v)

        print('MEDIAN of ratios: %12.2f. Values:' % median_ratio)
        for i,p in enumerate(ratio_lst):
            print('%3d' %i, ':', '%12.2f' % p)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
