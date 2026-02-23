#coding:utf-8

"""
ID:          issue-1784
ISSUE:       1784
TITLE:       French insensitive collation FR_FR_CI_AI
DESCRIPTION: Add French case-/accent-insensitive collation.
JIRA:        CORE-1366
FBTEST:      bugs.core_1366
NOTES:
    [31.10.2024] pzotov
    Bug was fixed for too old FB (2.1.8), firebird-driver and/or QA-plugin
    will not able to run on this version in order to reproduce problem.

    Checked on 6.0.0.511 (Windows/Linux); 5.0.2.1550;  4.0.6.3165; 3.0.2.32670, 3,0,1,32609
"""
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')
act = isql_act('db', substitutions=[('=.*', ''), ('[ \\t]+', ' ')])
tmp_sql = temp_file('tmp_core_1366.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.0')
def test_1(act: Action, tmp_sql: Path):

    test_script = f"""
        recreate table test(id int);
        commit;

        set term ^;
        execute block as
        begin
                begin execute statement 'drop collation coll_fr'; when any do begin end end
        end
        ^set term ;^
        commit;

        create collation coll_fr for iso8859_1 from external ('FR_FR') case insensitive accent insensitive;
        commit;

        recreate table test(id int, cf varchar(10) collate coll_fr);
        commit;

        recreate table noac(id int, nf varchar(10) collate coll_fr);
        commit;

        -- http://french.about.com/od/pronunciation/a/accents.htm

        -- (cedilla) is found only on the letter "C":
        insert into test(id, cf) values( 1010, 'ç');

        -- (acute accent) can only be on an "E"
        insert into test(id, cf) values( 1020, 'é');

        -- (grave accent) can be found on an "A", "E", "U"
        insert into test(id, cf) values( 1030, 'à');
        insert into test(id, cf) values( 1040, 'è');
        insert into test(id, cf) values( 1050, 'ù');

        -- (dieresis or umlaut) can be on an E, I and U
        insert into test(id, cf) values( 1060, 'ë');
        insert into test(id, cf) values( 1070, 'ï');
        insert into test(id, cf) values( 1080, 'ü');

        -- (circumflex) can be on an A, E, I, O and U
        insert into test(id, cf) values( 1090, 'â');
        insert into test(id, cf) values( 1110, 'ê');
        insert into test(id, cf) values( 1120, 'î');
        insert into test(id, cf) values( 1130, 'û');
        insert into test(id, cf) values( 1140, 'ô');
        commit;

        -- ANSI letters that should be equal to diacritical
        -- when doing comparison CI_AI:
        insert into noac(id, nf) values( 1150, 'A');
        insert into noac(id, nf) values( 1160, 'C');
        insert into noac(id, nf) values( 1170, 'E');
        insert into noac(id, nf) values( 1180, 'I');
        insert into noac(id, nf) values( 1190, 'O');
        insert into noac(id, nf) values( 1200, 'U');
        commit;
        select n.id n_id, n.nf, t.cf, t.id t_id
        from noac n
        left join test t on n.nf is not distinct from t.cf
        order by n_id, t_id;
    """

    # https://github.com/FirebirdSQL/firebird/issues/1784#issuecomment-826188088
    # ::: NB :::
    # For proper output of test, input script must be encoded in ISO8859_1 rather than in UTF-8.
    #
    tmp_sql.write_text(test_script, encoding='iso8859_1')
    act.expected_stdout = """
        N_ID NF         CF                 T_ID
        ============ ========== ========== ============
        1150 A          à                  1030
        1150 A          â                  1090
        1160 C          ç                  1010
        1170 E          é                  1020
        1170 E          è                  1040
        1170 E          ë                  1060
        1170 E          ê                  1110
        1180 I          ï                  1070
        1180 I          î                  1120
        1190 O          ô                  1140
        1200 U          ù                  1050
        1200 U          ü                  1080
        1200 U          û                  1130
    """
    act.isql(switches = ['-q'], input_file = tmp_sql, charset = 'iso8859_1', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
