#coding:utf-8

"""
ID:          issue-6875
ISSUE:       6875
TITLE:       Significant performance regression of SIMILAR TO and SUBSTRING(SIMILAR) when
  pattern is taken from variable (rather than directly specified)
DESCRIPTION:
    Confirmed poor performance in 5.0.0.88 (before fix):
                               in PSQL    explicitly
                               variable   specified
                               ----------------------
    * for SIMILAR TO:            11.40"         2.23"
    * for SUBSTRING...SIMILAR:   42.02"         1.37"

    Checked 4.0.1.2523 and 5.0.0.89 -- all OK.
    Performance in 5.0.0.89 (intermediate build 02.07.2021 14:43) after fix:
                               in PSQL    explicitly
                               variable   specified
                               ----------------------
    * for SIMILAR TO:             1.27"         1.15"
    * for SUBSTRING...SIMILAR:    2.02"         2.00"

    Performance in 4.0.1.2523 (intermediate build 02.07.2021 18:55) after fix:
                               in PSQL    explicitly
                               variable   specified
                               ----------------------
    * for SIMILAR TO:             1.03"         1.06"
    * for SUBSTRING...SIMILAR:    1.96"         1.97"

    So, values became almost the same and, moreover, performance was greatly improved.

    Test checks ratio between elapsed times and compares it with thresholds.
    Currently both SIMILAR TO and SUBSTRING...SIMILAR use the same thresholds =  1.30.
FBTEST:      bugs.gh_6875
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create or alter procedure sp_similar_to_test (
        N integer,
        V integer)
    as
        declare nums varchar(16) = '[-+]?[0-9]{1,10}';
        declare b boolean;
    begin
      while (n > 0) do
      begin
        if (v = 1) then
          b = '5' similar to nums; -------------------- [ 1 ]
        else if (v = 2) then
          b = '5' similar to '[-+]?[0-9]{1,10}'; ------ [ 2 ]
        n = n - 1;
      end
    end
    ^
    create or alter procedure sp_substring_similar_test (
        N integer,
        V integer)
    as
        declare p varchar(100) = '%/#*(=){3,}#"%#"(=){3,}#*/%';
        declare s varchar(1000);
    begin
      while (n > 0) do
      begin
        if (v = 1) then
          s = substring('
                            aaa
                            /*==== comment between "equal" characters ====*/
                            bbb
                            ccc
                            ddd
                            eee
                            fff
                            jjj
    	                ' similar p escape '#');

        else if (v = 2) then
          s = substring('
                            aaa
                            /*==== comment between "equal" characters  ====*/
                            bbb
                            ccc
                            ddd
                            eee
                            fff
                            jjj
    	                ' similar '%/#*(=){3,}#"%#"(=){3,}#*/%' escape '#');

        n = n - 1;
      end
    end
    ^
    commit
    ^
    execute block returns(msg_similar2 varchar(255), msg_subs_sim varchar(255)) as
        declare t0_similar_to timestamp;
        declare t0_subs_simil timestamp;
        declare elap_similar2_pattern_in_psql_var int;
        declare elap_similar2_pattern_explicitly int;
        declare elap_subs_sim_pattern_in_psql_var int;
        declare elap_subs_sim_pattern_explicitly int;

        -- ##############################
        -- ###   T H R E S H O L D S  ###
        -- ##############################
        declare SIMILAR2_THRESHOLD double precision = 1.30;
        declare SUBS_SIM_THRESHOLD double precision = 1.30;

    begin
        t0_similar_to = 'now';
        execute procedure sp_similar_to_test(1000000, 1);
        elap_similar2_pattern_in_psql_var = datediff(millisecond from t0_similar_to to  cast('now' as timestamp));

        -- rdb$set_context('USER_SESSION', 'SIMILAR2_PATTERN_DECLARED_IN_PSQL_VAR', datediff(millisecond from t0_similar_to to  cast('now' as timestamp)));

        t0_similar_to = 'now';
        execute procedure sp_similar_to_test(1000000, 2);
        elap_similar2_pattern_explicitly = datediff(millisecond from t0_similar_to to  cast('now' as timestamp));

        -- rdb$set_context('USER_SESSION', 'SIMILAR2_PATTERN_EXPLICITLY_SPECIFIED', datediff(millisecond from t0_similar_to to  cast('now' as timestamp)));

        t0_subs_simil = 'now';
        execute procedure sp_substring_similar_test(100000, 1);
        elap_subs_sim_pattern_in_psql_var = datediff(millisecond from t0_subs_simil to  cast('now' as timestamp));

        --rdb$set_context('USER_SESSION', 'SUBS_SIM_PATTERN_DECLARED_IN_PSQL_VAR', datediff(millisecond from t0_subs_simil to  cast('now' as timestamp)));

        t0_subs_simil = 'now';
        execute procedure sp_substring_similar_test(100000, 2);
        elap_subs_sim_pattern_explicitly = datediff(millisecond from t0_subs_simil to  cast('now' as timestamp));
        -- rdb$set_context('USER_SESSION', 'SUBS_SIM_PATTERN_EXPLICITLY_SPECIFIED', datediff(millisecond from t0_subs_simil to  cast('now' as timestamp)));

        if ( 1.00 * elap_similar2_pattern_in_psql_var / elap_similar2_pattern_explicitly > SIMILAR2_THRESHOLD ) then
            msg_similar2 = 'INACCEPTABLE /* perf_issue_tag */: '
                           || elap_similar2_pattern_in_psql_var
                           || 's  == vs ==  '
                           || elap_similar2_pattern_explicitly || 's'
                           || ' -  greater than treshold = ' || SIMILAR2_THRESHOLD
            ;
        else
            msg_similar2 = 'Acceptable.';


        if ( 1.00 * elap_subs_sim_pattern_in_psql_var / elap_subs_sim_pattern_explicitly > SUBS_SIM_THRESHOLD ) then
            msg_subs_sim = 'INACCEPTABLE /* perf_issue_tag */: '
                           || elap_subs_sim_pattern_in_psql_var
                           || 'ms  == vs ==  '
                           || elap_subs_sim_pattern_explicitly || 'ms'
                           || ' -  greater than treshold = ' || SIMILAR2_THRESHOLD
            ;
        else
            msg_subs_sim = 'Acceptable.';


        suspend;

    end
    ^
    set term ;^
    commit;

"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG_SIMILAR2                    Acceptable.
    MSG_SUBS_SIM                    Acceptable.
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
