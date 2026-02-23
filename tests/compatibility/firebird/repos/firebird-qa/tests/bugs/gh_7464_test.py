#coding:utf-8

"""
ID:          issue-7464
ISSUE:       7464
TITLE:       Crash on repeating update in 5.0
NOTES:
    [15.02.2023] pzotov
    Confirmed crash on 5.0.0.910
    Checked on 5.0.0.920 -- all fine.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table dns_unit_arpscan (
        id bigint not null,
        ip_address varchar(20),
        mac_address varchar(20),
        time_last_scan timestamp,
        constraint pk_dns_unit_arpscan primary key (id)
    );

    insert into dns_unit_arpscan values(
        34021000000011
        ,'224.0.0.25'
        ,'01-00-5e-00-00-fb'
        ,'2023-01-30 09:50:47.0000'
    );

    update DNS_UNIT_ARPSCAN set 
        ID=34021000000011
        ,IP_ADDRESS='224.0.0.25'
        ,MAC_ADDRESS='01-00-5e-00-00-fb'
        ,TIME_LAST_SCAN='2023-01-30 09:50:47'
    WHERE DNS_UNIT_ARPSCAN.ID=34021000000011
    returning TIME_LAST_SCAN,IP_ADDRESS,ID,MAC_ADDRESS
    ;

    update DNS_UNIT_ARPSCAN set 
        ID=34021000000011
        ,IP_ADDRESS='224.0.0.25'
        ,MAC_ADDRESS='01-00-5e-00-00-fb'
        ,TIME_LAST_SCAN='2023-01-30 09:50:47'
    WHERE DNS_UNIT_ARPSCAN.ID=34021000000011
    returning TIME_LAST_SCAN,IP_ADDRESS,ID,MAC_ADDRESS
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TIME_LAST_SCAN                  2023-01-30 09:50:47.0000
    IP_ADDRESS                      224.0.0.25
    ID                              34021000000011
    MAC_ADDRESS                     01-00-5e-00-00-fb

    TIME_LAST_SCAN                  2023-01-30 09:50:47.0000
    IP_ADDRESS                      224.0.0.25
    ID                              34021000000011
    MAC_ADDRESS                     01-00-5e-00-00-fb
"""

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout and act.clean_stderr == act.clean_expected_stderr
