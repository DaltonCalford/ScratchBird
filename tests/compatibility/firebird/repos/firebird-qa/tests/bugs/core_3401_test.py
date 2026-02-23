#coding:utf-8

"""
ID:          issue-3766
ISSUE:       3766
TITLE:       Collation errors with [type of] <domain>, type of column
DESCRIPTION:
JIRA:        CORE-3401
FBTEST:      bugs.core_3401
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -----------------------------------------------------------------------------------------------
    -- NB-1. Initial test (which is in the tracker) considered only ASCII characters 'a' vs 'A'.
    -- This test compares many NON-ascii characters with diacritical marks - it was done intentionally.
	-- All the cases  successfully PASSED on WI-T3.0.0.31767 (Win XP) and LI-T3.0.0.31719 (Ubuntu 14.10).
	-- Firebird WI-V2.5.4.26857 FAILS in test-cases 1,2 and 4.
    -- NB-2. Attribute 'connection_character_set': 'UTF8' - is MANDATORY in this test,
    -- otherwise fbt_run raises exception:
    -- traceback:
    --    File "c:\\mix\\firebird\\qa\\fbtest\\fbtest.py", line 2103, in run
    --      test_recipe.run(self,result)
    --    File "c:\\mix\\firebird\\qa\\ftest\\fbtest.py", line 731, in run
    --      script = self.test_script.encode('ascii')
    --  ----------------------------------------------------------------------
    --  exception:
    --  UnicodeEncodeError:
    --  ascii
    -- . . .
    -- 505
    -- 511
    -- ordinal not in range(128)
    -- (no any exceptions when run corresponding script in ISQL with absent charset of connection).
   -----------------------------------------------------------------------------------------------

    create or alter procedure selproc_dom as begin end;
    create or alter procedure selproc_typeof_col_ci_dir as begin end;
    commit;

    recreate table t(id int);
    commit;

    set term ^;
    execute block as
    begin
      execute statement 'drop domain dom_ci';
    when any do begin end
    end
    ^
    set term ;^
    commit;

    -- Following letters was included in the character literals:
	-- ÁÉÍÓÚÝ ÀÈÌÒÙ ÂÊÎÔÛ ÃÑÕ ÄËÏÖÜŸ ÇŠ ==> all latin letters with diacritical marks
    -- (with acute, grave, circumflex, tilde, umlaute, cedil and caron)
    -- ΔΘΛΞΣΨΩ ==> few letters from greek alphabet
    -- ĄĘŁŹŻ ==> few letters from polish alphabet
    -- ЙЁ ==> two letters from cyrillic (russian) with diacritical marks
    -- ЊЋЏ ==> few letters from serbian alphabet
    -- ĂŞŢ ==> few letters from romanian alphabet

    -- create a case-insensitive domain:
    create domain dom_ci as varchar(150) character set utf8 collate unicode_ci;
    commit;

    -- create two case-insensitive columns: one via the domain, one directly:
    recreate table t(
      col_ci_dom dom_ci,
      col_ci_dir varchar(150) character set utf8 collate unicode_ci
    );
    commit;

    set list on;

    -- (Case 1)  test <lower> = <UPPER> when casting to type of column col_ci_dir:
    with q(a_lower, a_upper) as (
      select cast('áéíóúý àèìòù âêîôû ãñõ äëïöüÿ çš δθλξσψω ąęłźż йё њћџ ăşţ' as type of column t.col_ci_dir),
             cast('ÁÉÍÓÚÝ ÀÈÌÒÙ ÂÊÎÔÛ ÃÑÕ ÄËÏÖÜŸ ÇŠ ΔΘΛΞΣΨΩ ĄĘŁŹŻ ЙЁ ЊЋЏ ĂŞŢ' as type of column t.col_ci_dir)
      from rdb$database
    )
    select 'case-1' msg, case when a_lower = a_upper then 1 else 0 end equal
    from q;

    ------------------------------------------------------------------------------------

    -- (Case 2) test <lower> = <UPPER> in domain dom_ci, using local vars in an executable block:
    -- (With "type of dom_ci" and "type of column t.col_ci_dom", the result is also 1.
    -- But with "type of column t.col_ci_dir", the result is 0.)
    set term ^;
    execute block returns (msg varchar(10), equal smallint)
    as
      -- this worked fine:
      -- declare ü dom_ci = <lower>;
      -- declare y dom_ci = <UPPER>;
      -- "But with "type of column t.col_ci_dir", the result is 0."
      declare x type of column t.col_ci_dir = 'áéíóúý àèìòù âêîôû ãñõ äëïöüÿ çš δθλξσψω ąęłźż йё њћџ ăşţ';
      declare y type of column t.col_ci_dir = 'ÁÉÍÓÚÝ ÀÈÌÒÙ ÂÊÎÔÛ ÃÑÕ ÄËÏÖÜŸ ÇŠ ΔΘΛΞΣΨΩ ĄĘŁŹŻ ЙЁ ЊЋЏ ĂŞŢ';
    begin
      msg='case-2';
      equal = case when x = y then 1 else 0 end;
      suspend;
    end
    ^
    set term ;^


    ------------------------------------------------------------------------------------

    -- (Case 3) test <lower> = <UPPER> in domain dom_ci, using stored procedure with input params:
    set term ^;
    create or alter procedure selproc_dom(x dom_ci, y dom_ci) returns (equal smallint)
    as
    begin
      equal = case when x = y then 1 else 0 end;
      suspend;
    end
    ^
    set term ;^
    commit;
    select 'case-3' msg, p.equal
    from selproc_dom(
      'áéíóúý àèìòù âêîôû ãñõ äëïöüÿ çš δθλξσψω ąęłźż йё њћџ ăşţ',
      'ÁÉÍÓÚÝ ÀÈÌÒÙ ÂÊÎÔÛ ÃÑÕ ÄËÏÖÜŸ ÇŠ ΔΘΛΞΣΨΩ ĄĘŁŹŻ ЙЁ ЊЋЏ ĂŞŢ'
    ) p;


    ---------------------------------------------------------------------------------------

    set term ^;
    create or alter procedure selproc_typeof_col_ci_dir(
      x type of column t.col_ci_dir,
      y type of column t.col_ci_dir
    )
    returns (equal smallint) as
    begin
      equal = case when x = y then 1 else 0 end;
      suspend;
    end
    ^
    set term ;^
    commit;
    select 'case-4' msg, p.*
    from selproc_typeof_col_ci_dir(
      'áéíóúý àèìòù âêîôû ãñõ äëïöüÿ çš δθλξσψω ąęłźż йё њћџ ăşţ',
      'ÁÉÍÓÚÝ ÀÈÌÒÙ ÂÊÎÔÛ ÃÑÕ ÄËÏÖÜŸ ÇŠ ΔΘΛΞΣΨΩ ĄĘŁŹŻ ЙЁ ЊЋЏ ĂŞŢ'
    ) p;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             case-1
    EQUAL                           1
    MSG                             case-2
    EQUAL                           1
    MSG                             case-3
    EQUAL                           1
    MSG                             case-4
    EQUAL                           1
"""

@pytest.mark.intl
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(charset='utf8')
    assert act.clean_stdout == act.clean_expected_stdout

