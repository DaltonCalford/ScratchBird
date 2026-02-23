#coding:utf-8

"""
ID:          issue-7461
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7461
TITLE:       Differences in field metadata descriptions between Firebird 2.5 and Firebird 4
NOTES:
    [21.01.2024] pzotov
        NB: original title of ticket was changed. Commits refer to pull reuest rather than to this ticket number:
            FB 4.x (16.11.2023): https://github.com/FirebirdSQL/firebird/commit/2886cd78991209842ee7e3065bde83ab75571af4
            FB 5.x (17.11.2023): https://github.com/FirebirdSQL/firebird/commit/1ed7f81f168b643a29357fce2e1f49156e9f5a1f
            FB 6.x (17.11.2023): https://github.com/FirebirdSQL/firebird/commit/ab6aced05723dc1b2e6bb96bfdaa86cb3090daf2
        Before fix:
            01: sqltype: 580 INT64 Nullable scale: -2 subtype: 1 len: 8
              :  name:   alias: SALARY                                       ---
              : table:   owner: 
            02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name:   alias: EMP_NO                                       ---
              : table:   owner:                                              ---

        After fix:
            01: sqltype: 580 INT64 Nullable scale: -2 subtype: 1 len: 8
              :  name: MAX  alias: SALARY                                    +++
              : table:   owner: 
            02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
              :  name: EMP_NO  alias: EMP_NO                                 +++
              : table: SALARY_HISTORY  owner: SYSDBA                         +++

        Confirmed bug on 4.0.4.3016, 5.0.0.1268
        Checked on 4.0.4.3021 (build date: 17-nov-2023), 5.0.0.1271 (build date: 18-nov-2023); 6.0.0.219

    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table employee(emp_no int);
    recreate table salary_history(emp_no int, new_salary numeric(10,2));

    set sqlda_display on;
    set planonly;

    select t.*
    from employee e
    left outer join
        (
            select max(h.new_salary) as salary,h.emp_no
            from salary_history h
            group by h.emp_no
        ) t on e.emp_no = t.emp_no
    ;
"""

act = isql_act('db', test_script, substitutions=[('^((?!name:|table:).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):

    expected_stdout_5x = f"""
        :  name: MAX  alias: SALARY
        : table:   owner:
        :  name: EMP_NO  alias: EMP_NO
        : table: SALARY_HISTORY  owner: {act.db.user}
    """
    expected_stdout_6x = f"""
        : name: MAX alias: SALARY
        : table: schema: owner:
        : name: EMP_NO alias: EMP_NO
        : table: SALARY_HISTORY schema: PUBLIC owner: SYSDBA
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
