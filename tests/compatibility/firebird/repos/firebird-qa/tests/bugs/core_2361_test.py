#coding:utf-8

"""
ID:          issue-2784
ISSUE:       2784
TITLE:       String truncation reading 8859-1 Spanish column using isc_dsql_fetch with UTF-8 connection
DESCRIPTION:
JIRA:        CORE-2361
FBTEST:      bugs.core_2361
"""

import pytest
from firebird.qa import *

init_scrip = """create table "'Master by Reseller$'" (
    "Tier" VARCHAR(20) CHARACTER SET ISO8859_1 COLLATE ES_ES_CI_AI
);

commit;

insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('(blank)');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Approved');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Bronze');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('DMR');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Domestic Distributor');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('End-User');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Evaluation');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Gold');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('New');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('Silver');
insert into "'Master by Reseller$'" ( "Tier" ) VALUES ('VAM');

commit;
"""

db = db_factory(charset='ISO8859_1', init=init_scrip)

test_script = """select case when 1 = 0 then '(blank)' else "'Master by Reseller$'"."Tier" end from "'Master by Reseller$'";
"""

act = isql_act('db', test_script)

expected_stdout = """
CASE
====================
(blank)
Approved
Bronze
DMR
Domestic Distributor
End-User
Evaluation
Gold
New
Silver
VAM

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

