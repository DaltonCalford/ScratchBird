#coding:utf-8

"""
ID:          new-database-28
TITLE:       New DB - RDB$TRIGGER_MESSAGES content
DESCRIPTION: Check the correct content of RDB$TRIGGER_MESSAGES in new database.
FBTEST:      functional.basic.db.28
NOTES:
[17.01.2023] pzotov
    DISABLED after discussion with dimitr, letters 17-sep-2022 11:23.
    Reasons:
        * There is no much sense to keep such tests because they fails extremely often during new major FB developing.
        * There is no chanse to get successful outcome for the whole test suite is some of system table became invalid,
          i.e. lot of other tests will be failed in such case.
    Single test for check DDL (type of columns, their order and total number) will be implemented for all RDB-tables.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    set blob all;
    select * from rdb$trigger_messages order by rdb$trigger_name, rdb$message_number;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$TRIGGER_NAME[\\s]+RDB\\$TRIGGER.*',
                                                  'RDB\\$TRIGGER_NAME RDB\\$TRIGGER')])

# version: 3.0

expected_stdout_1 = """

    RDB$TRIGGER_NAME                RDB$TRIGGER_1
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     existing_priv_mod

    RDB$TRIGGER_NAME                RDB$TRIGGER_10
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     primary_key_ref

    RDB$TRIGGER_NAME                RDB$TRIGGER_10
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     primary_key_notnull

    RDB$TRIGGER_NAME                RDB$TRIGGER_12
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     ref_cnstrnt_notfound

    RDB$TRIGGER_NAME                RDB$TRIGGER_12
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     foreign_key_notfound

    RDB$TRIGGER_NAME                RDB$TRIGGER_13
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     ref_cnstrnt_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_14
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     check_cnstrnt_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_15
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     check_cnstrnt_del

    RDB$TRIGGER_NAME                RDB$TRIGGER_17
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     integ_index_seg_del

    RDB$TRIGGER_NAME                RDB$TRIGGER_18
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     integ_index_seg_mod

    RDB$TRIGGER_NAME                RDB$TRIGGER_19
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     integ_index_del

    RDB$TRIGGER_NAME                RDB$TRIGGER_2
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     systrig_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_20
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     integ_index_mod

    RDB$TRIGGER_NAME                RDB$TRIGGER_20
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     integ_index_deactivate

    RDB$TRIGGER_NAME                RDB$TRIGGER_20
    RDB$MESSAGE_NUMBER              3
    RDB$MESSAGE                     integ_deactivate_primary

    RDB$TRIGGER_NAME                RDB$TRIGGER_21
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     check_trig_del

    RDB$TRIGGER_NAME                RDB$TRIGGER_22
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     check_trig_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_23
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     cnstrnt_fld_del

    RDB$TRIGGER_NAME                RDB$TRIGGER_24
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     cnstrnt_fld_rename

    RDB$TRIGGER_NAME                RDB$TRIGGER_24
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     integ_index_seg_mod

    RDB$TRIGGER_NAME                RDB$TRIGGER_25
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     rel_cnstrnt_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_26
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     constaint_on_view

    RDB$TRIGGER_NAME                RDB$TRIGGER_26
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     invld_cnstrnt_type

    RDB$TRIGGER_NAME                RDB$TRIGGER_26
    RDB$MESSAGE_NUMBER              3
    RDB$MESSAGE                     primary_key_exists

    RDB$TRIGGER_NAME                RDB$TRIGGER_3
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     systrig_update

    RDB$TRIGGER_NAME                RDB$TRIGGER_31
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     no_write_user_priv

    RDB$TRIGGER_NAME                RDB$TRIGGER_32
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     no_write_user_priv

    RDB$TRIGGER_NAME                RDB$TRIGGER_33
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     no_write_user_priv

    RDB$TRIGGER_NAME                RDB$TRIGGER_36
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     integ_index_seg_mod

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              0
    RDB$MESSAGE                     grant_obj_notfound

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              1
    RDB$MESSAGE                     grant_fld_notfound

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              2
    RDB$MESSAGE                     grant_nopriv

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              3
    RDB$MESSAGE                     nonsql_security_rel

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              4
    RDB$MESSAGE                     nonsql_security_fld

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$MESSAGE_NUMBER              5
    RDB$MESSAGE                     grant_nopriv_on_base


    Records affected: 35
"""

@pytest.mark.version('>=3.0,<5')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

expected_stdout_2 = """
RDB$TRIGGER_NAME                RDB$TRIGGER_1
RDB$MESSAGE_NUMBER              0
RDB$MESSAGE                     existing_priv_mod

RDB$TRIGGER_NAME                RDB$TRIGGER_10
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     primary_key_ref

RDB$TRIGGER_NAME                RDB$TRIGGER_10
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     primary_key_notnull

RDB$TRIGGER_NAME                RDB$TRIGGER_12
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     ref_cnstrnt_notfound

RDB$TRIGGER_NAME                RDB$TRIGGER_12
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     foreign_key_notfound

RDB$TRIGGER_NAME                RDB$TRIGGER_13
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     ref_cnstrnt_update

RDB$TRIGGER_NAME                RDB$TRIGGER_14
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     check_cnstrnt_update

RDB$TRIGGER_NAME                RDB$TRIGGER_15
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     check_cnstrnt_del

RDB$TRIGGER_NAME                RDB$TRIGGER_17
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     integ_index_seg_del

RDB$TRIGGER_NAME                RDB$TRIGGER_18
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     integ_index_seg_mod

RDB$TRIGGER_NAME                RDB$TRIGGER_19
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     integ_index_del

RDB$TRIGGER_NAME                RDB$TRIGGER_2
RDB$MESSAGE_NUMBER              0
RDB$MESSAGE                     systrig_update

RDB$TRIGGER_NAME                RDB$TRIGGER_20
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     integ_index_mod

RDB$TRIGGER_NAME                RDB$TRIGGER_20
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     integ_index_deactivate

RDB$TRIGGER_NAME                RDB$TRIGGER_20
RDB$MESSAGE_NUMBER              3
RDB$MESSAGE                     integ_deactivate_primary

RDB$TRIGGER_NAME                RDB$TRIGGER_21
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     check_trig_del

RDB$TRIGGER_NAME                RDB$TRIGGER_22
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     check_trig_update

RDB$TRIGGER_NAME                RDB$TRIGGER_23
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     cnstrnt_fld_del

RDB$TRIGGER_NAME                RDB$TRIGGER_24
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     cnstrnt_fld_rename

RDB$TRIGGER_NAME                RDB$TRIGGER_24
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     integ_index_seg_mod

RDB$TRIGGER_NAME                RDB$TRIGGER_25
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     rel_cnstrnt_update

RDB$TRIGGER_NAME                RDB$TRIGGER_26
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     constaint_on_view

RDB$TRIGGER_NAME                RDB$TRIGGER_26
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     invld_cnstrnt_type

RDB$TRIGGER_NAME                RDB$TRIGGER_26
RDB$MESSAGE_NUMBER              3
RDB$MESSAGE                     primary_key_exists

RDB$TRIGGER_NAME                RDB$TRIGGER_3
RDB$MESSAGE_NUMBER              0
RDB$MESSAGE                     systrig_update

RDB$TRIGGER_NAME                RDB$TRIGGER_36
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     integ_index_seg_mod

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              0
RDB$MESSAGE                     grant_obj_notfound

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              1
RDB$MESSAGE                     grant_fld_notfound

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              2
RDB$MESSAGE                     grant_nopriv

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              3
RDB$MESSAGE                     nonsql_security_rel

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              4
RDB$MESSAGE                     nonsql_security_fld

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$MESSAGE_NUMBER              5
RDB$MESSAGE                     grant_nopriv_on_base


Records affected: 32

"""

@pytest.mark.version('>=5')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
