#coding:utf-8

"""
ID:          issue-4487
ISSUE:       4487
TITLE:       The parameterized exception does not accept not ASCII characters as parameter
DESCRIPTION:
JIRA:        CORE-4160
FBTEST:      bugs.core_4160
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_alert(a_lang char(2), a_new_amount int) as begin end;
    commit;
    recreate exception ex_negative_remainder ' @1 (@2)';
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """
    set term ^;
    create or alter procedure sp_alert(a_lang char(2), a_new_amount int) as
    begin
       if (a_lang = 'cz') then
         exception ex_negative_remainder using ('Czech: New Balance bude menší než nula', a_new_amount);
       else	if (a_lang = 'pt') then
         exception ex_negative_remainder using ('Portuguese: New saldo será menor do que zero', a_new_amount);
       else	if (a_lang = 'dm') then
         exception ex_negative_remainder using ('Danish: New Balance vil være mindre end nul', a_new_amount);
       else	if (a_lang = 'gc') then
         exception ex_negative_remainder using ('Greek: Νέα ισορροπία θα είναι κάτω από το μηδέν', a_new_amount);
       else	if (a_lang = 'fr') then
         exception ex_negative_remainder using ('French: Nouveau solde sera inférieur à zéro', a_new_amount);
       else
         exception ex_negative_remainder using ('Russian: Новый остаток будет меньше нуля', a_new_amount);
    end
    ^
    set term ;^
    commit;
    execute procedure sp_alert('cz', -1);
    execute procedure sp_alert('pt', -2);
    execute procedure sp_alert('dm', -3);
    execute procedure sp_alert('gc', -4);
    execute procedure sp_alert('fr', -5);
    execute procedure sp_alert('jp', -6);
"""

substitutions = [ (r'line(:)?\s+\d+.*', '') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - Czech: New Balance bude menší než nula (-1)
    -At procedure 'SP_ALERT'
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - Portuguese: New saldo será menor do que zero (-2)
    -At procedure 'SP_ALERT'
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - Danish: New Balance vil være mindre end nul (-3)
    -At procedure 'SP_ALERT'
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - Greek: Νέα ισορροπία θα είναι κάτω από το μηδέν (-4)
    -At procedure 'SP_ALERT'
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - French: Nouveau solde sera inférieur à zéro (-5)
    -At procedure 'SP_ALERT'
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_NEGATIVE_REMAINDER
    - Russian: Новый остаток будет меньше нуля (-6)
    -At procedure 'SP_ALERT'
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - Czech: New Balance bude menší než nula (-1)
    -At procedure "PUBLIC"."SP_ALERT"
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - Portuguese: New saldo será menor do que zero (-2)
    -At procedure "PUBLIC"."SP_ALERT"
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - Danish: New Balance vil være mindre end nul (-3)
    -At procedure "PUBLIC"."SP_ALERT"
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - Greek: Νέα ισορροπία θα είναι κάτω από το μηδέν (-4)
    -At procedure "PUBLIC"."SP_ALERT"
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - French: Nouveau solde sera inférieur à zéro (-5)
    -At procedure "PUBLIC"."SP_ALERT"
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_NEGATIVE_REMAINDER"
    - Russian: Новый остаток будет меньше нуля (-6)
    -At procedure "PUBLIC"."SP_ALERT"
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
