#coding:utf-8

"""
ID:          issue-6706
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6706
TITLE:       Memory leak when running EXECUTE STATEMENT with named parameters [CORE6475]
DESCRIPTION:
    We create stored procedure with PARAMS_COUNT input parameters.
    Then EXECUTE BLOCK is generated with call of this SP via EXECUTE STATEMENT which applies EXCESS modifier to all arguments.
    Value of memory_info().rss is obtained (for appropriate server process), then run execute block MEASURES_COUNT times
    and after this - again get memory_info().rss value.
    Ratio between current and initial values of memory_info().rss must be less than  MAX_RATIO.
NOTES:
    [17.08.2024] pzotov
    1. Problem did exist in FB 4.x up to snapshot 4.0.0.2336.
       Commit: https://github.com/FirebirdSQL/firebird/commit/4dfb30a45b767994c074bbfcbb8494b8ada19b33 (23-jan-2021, 15:26)
       Before this commit ratio for SS was about 5..6 for SS and about  8..9 for CS.
       Since 4.0.0.2341 memory consumption was reduced to ~1.6 ... 1.9
    2. Database must be created with FW = ON otherwise ratio for all snapshots is about 1.5 (and this seems weird).
    3. Test duration is about 35s.

    Checked on 6.0.0.438, 5.0.2.1478, 4.0.6.3142; 4.0.0.2336, 4.0.0.2341.
"""

import psutil
import pytest
from firebird.qa import *
import time

###########################
###   S E T T I N G S   ###
###########################

# How many input parameters must have procedure:
PARAMS_COUNT = 1000

# How many times we call procedures:
MEASURES_COUNT = 1000

# Maximal value for ratio between
# new and initial memory_info().rss values:
#
MAX_RATIO = 3
#############

db = db_factory(async_write = False)
act = python_act('db')

#--------------------------------------------------------------------

def get_server_pid(con):
    with con.cursor() as cur:
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
    return fb_pid

#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.0')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:

        sp_ddl = """
            create or alter procedure sp_test(
        """
        params_lst = '\n'.join( [  (',' if i else '') +f'p_{i} int' for i in range(PARAMS_COUNT) ] )
        sp_ddl = '\n'.join( ("create or alter procedure sp_test(", params_lst, ") returns(x int) as begin x = 1; suspend; end") )
        con.execute_immediate(sp_ddl)
        con.commit()

        server_process = psutil.Process(get_server_pid(con))

        params_lst = ','.join( [ f':p_{i}' for i in range(PARAMS_COUNT) ] )
        passed_args = ','.join( [ f'excess p_{i} := 1' for i in range(PARAMS_COUNT) ] )

        srv_memo_rss_init = int(server_process.memory_info().rss / 1024)
        srv_memo_vms_init = int(server_process.memory_info().vms / 1024)

        cur = con.cursor()
        for k in range(MEASURES_COUNT):

            es_sql = f"""
                execute block returns(x int) as
                begin
                    execute statement ('select p.x * {k} from sp_test({params_lst}) p') ({passed_args})
                    into x;
                    suspend;
                end
            """
            cur.execute(es_sql)
            for r in cur:
                pass

        srv_memo_rss_curr = int(server_process.memory_info().rss / 1024)
        srv_memo_vms_curr = int(server_process.memory_info().vms / 1024)

    memo_ratio = srv_memo_rss_curr / srv_memo_rss_init

    SUCCESS_MSG = 'Ratio between memory values measured before and after loop: acceptable'
    if memo_ratio < MAX_RATIO:
        print(SUCCESS_MSG)
    else:
        print( 'Ratio: /* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:.2f}'.format(memo_ratio), '{:.2f}'.format(MAX_RATIO) ) )

    act.expected_stdout = SUCCESS_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
