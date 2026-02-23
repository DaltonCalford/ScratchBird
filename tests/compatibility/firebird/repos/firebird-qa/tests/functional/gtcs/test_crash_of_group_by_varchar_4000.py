#coding:utf-8

"""
ID:          gtcs.crash-of-group-by-varchar-4000
FBTEST:      functional.gtcs.crash_of_group_by_varchar_4000
TITLE:       Crash on attempt to GROUP BY on table with varchar(4000) field
DESCRIPTION:
  ::: NB :::
  ### Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_33.script

  Source description (dominikfaessler, message of 2004-05-27 13:11:09; FB 1.5.1.4443):
  https://sourceforge.net/p/firebird/mailman/message/17071981/

  Issue in original test:
  bug #961543 Server Crash ISO8859_1 and DE_DE

  NB: it is enough in 'expected_stdout' to show only name of resulting field ('F01')
  rather than the whole output.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='ISO8859_1')

test_script = """
    create table snippets (
        f01 varchar(4000) collate de_de
    );

    set term ^;

    insert into snippets values('
      function JsShowZeroFilled(inValue) {
      if(inValue > 9) {
      return inValue;
      } else {
      return ''0'' + inValue;
      }
      }




      function JsGetWochentagname(wochentag,wochentagtyp,langcode) {
      var wochentagname;


      array_DE = new Array("SO,Son.,Sonntag", "MO,Mon.,Montag",
      "DI,Di.,Dienstag", "MI,Mi.,Mittwoch",
      "DO,Don.,Donnerstag","FR,Fr.,Freitag", "SA,Sam.,Samstag");
      array_EN = new Array("SU,Su.,Sunday", "MO,Mon.,Monday",
      "TU,Tu.,Tuesday", "WE,We.,Wednesday", "DO,Th.,Thursday",
      "FR,Fr.,Friday", "SA,Sa.,Saturday");


      if (langcode.toUpperCase() == ''DE'') {
      array_wochentagname = array_DE[wochentag].split('','');
      wochentagname = array_wochentagname[wochentagtyp-1];
      } else {
      array_wochentagname = array_EN[wochentag].split('','');
      wochentagname = array_wochentagname[wochentagtyp-1];
      }
      return wochentagname;
      }
    '
    ) ^
    set term ;^
    commit;

    set count on;
    set list on;
    select f01 from snippets group by f01;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ' '), ('^((?![Er]rror\\s+(reading|writing)|SQLSTATE|F01|Records affected).)*$', '') ] )

expected_stdout = """
    F01
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
