#coding:utf-8

"""
ID:          issue-6386
ISSUE:       6386
TITLE:       Server crashes when it run SQL
DESCRIPTION:
  Confirmed bug on: 4.0.0.1573, 3.0.5.33166
  (got in firebird.log: "internal Firebird consistency check (invalid SEND request (167), file: JrdStatement.cpp line: 327)")
JIRA:        CORE-6137
FBTEST:      bugs.core_6137
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table tmp_labelbarcode (
        id integer not null,
        barcode char(20) not null,
        idlabel integer not null
    );

    alter table tmp_labelbarcode add primary key (id);
    create index tmlb_barcode on tmp_labelbarcode (barcode);
    create index tmlb_idlabel on tmp_labelbarcode (idlabel);
    create unique index tmp_labelbarcode_idx1 on tmp_labelbarcode (barcode, idlabel);
    commit;

    insert into tmp_labelbarcode (id, barcode, idlabel) values (224423, '4627136039368', 278164);
    commit;

    set count on;
    --set echo on;

    select tmp_labelbarcode.BARCODE
    from tmp_labelbarcode
    where tmp_labelbarcode.BARCODE = '462713603936820000004620016596753'
    order by tmp_labelbarcode.BARCODE
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
