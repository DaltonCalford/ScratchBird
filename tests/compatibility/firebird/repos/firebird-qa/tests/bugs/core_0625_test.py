#coding:utf-8

"""
ID:          issue-987
ISSUE:       987
TITLE:       Token unknown in simple SELECT with GROUP BY and ORDER BY
DESCRIPTION: Getting SQL error code = -104, Token unknown count.
JIRA:        CORE-625
FBTEST:      bugs.core_0625
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=4096)

test_script = """
    recreate table customers(cust_id int primary key using index customers_pk, country int);
    commit;

    insert into customers
    select r.rdb$relation_id, rand()*10
    from rdb$relations r;
    commit;

    create index customers_country on customers(country);
    commit;

    set planonly;
    --set explain on;
    select country, count(country)
    from customers
    group by country
    order by count(country);
    -- NB: PLAN up to 2.5 contains TWO parenthesis:
    -- PLAN SORT ((CUSTOMERS ORDER CUSTOMERS_COUNTRY))
    --           ^^                                 ^^
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('\\(+', ''), ('\\)+', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN SORT CUSTOMERS ORDER CUSTOMERS_COUNTRY
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

