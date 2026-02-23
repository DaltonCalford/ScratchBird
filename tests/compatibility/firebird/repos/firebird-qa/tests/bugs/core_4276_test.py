#coding:utf-8

"""
ID:          issue-4600
ISSUE:       4600
TITLE:       Error on create table with "CHARACTER SET DOS775" field
DESCRIPTION:
JIRA:        CORE-4276
FBTEST:      bugs.core_4276
NOTES:
    [29.06.2025] pzotov
    Removed 'SHOW' commands as having no sense in this test (it is enough to query just created table and check its data).

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

substitutions = [('[ \t]+', ' '), ('BLOB_ID.*', '')]
act = python_act('db', substitutions = substitutions)

DATA_IN_VCHR = 'ÓßŌŃõÕµńĶķĻļņĒŅ'
DATA_IN_BLOB = """
    Green - viens no trim primārās krāsas, zaļā tiek uzskatīts diapazontsvetov spektrs ar viļņa
    garumu aptuveni 500-565 nanometri. Sistēma CMYK druka zaļā iegūst, sajaucot dzelteno un
    zilganzaļi (cyan).Dabā, Catalpa - zaļa augs.
    Krāsu zaļie augi ir dabiski, ka cilvēks etalonomzeleni.
    Zaļā koku varde.
    Ir plaši izplatīti dabā. Lielākā daļa augu ir zaļā krāsā, jo tie satur pigmentu fotosintēzes -
    hlorofilu (hlorofils absorbē lielu daļu no sarkano stariem saules spektra, atstājot uztveri
    atstarotās un filtrē zaļā krāsā). Dzīvnieki ar zaļo krāsu tā izmantošanu maskēties fona augiem.
"""

test_script = f"""
    recreate table "ĄČĘĢÆĖŠŚÖÜØ£"(
            "ąčęėįšųūž" varchar(50) character set dos775
            ,"Õisu ja kariste järved" blob sub_type 1 character set dos775
    );
    commit;

    insert into "ĄČĘĢÆĖŠŚÖÜØ£"("ąčęėįšųūž", "Õisu ja kariste järved")
    values(
            '{DATA_IN_VCHR}',
            '{DATA_IN_BLOB}'
    );
    set list on;
    set blob all;
    select "ąčęėįšųūž", "Õisu ja kariste järved" as BLOB_ID
    from "ĄČĘĢÆĖŠŚÖÜØ£";
"""

expected_stdout = f"""
	ąčęėįšųūž {DATA_IN_VCHR}
	BLOB_ID 80:0
	{DATA_IN_BLOB}
"""

script_file = temp_file('test-script.sql')

@pytest.mark.intl
@pytest.mark.version('>=3')
def test_1(act: Action, script_file: Path):
    script_file.write_text(test_script, encoding='cp775')
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', '-b'], combine_output = True, input_file=script_file, charset='DOS775')
    assert act.clean_stdout == act.clean_expected_stdout
