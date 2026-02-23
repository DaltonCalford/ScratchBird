#coding:utf-8

"""
ID:          issue-6416
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6416
TITLE:       Engine cant determine datatype in SQL: Unknown SQL Data type (32752) [CORE6168]
DESCRIPTION:
    Test creates table with columns belonging to "new datatypes" family: int128, decfloat and time[stamp] with time zone.
    Also, one record is added into this table with values which are valid for numeric types in FB 4.x+ (time zone fields
    can remain null or arbitrary).
    Further, this DB is copied to another DB using file-level call of shutil.copy2().
    Another DB filename must match to the specified in the databases.conf (alias defined by 'REQUIRED_ALIAS' variable).
    Its alias has special value for DataTypeCompatibility parameter. Connection to this DB and query to a table with 'new datatypes'
    must return SQLDA with *old* types which are known for FB versions prior 4.x.
    
    Then we repeat same query to 'initial' test DB and must get SQLDA with actual values for all new columns (known since FB 4.x).
NOTES:
    [18.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Database file for REQUIRED_ALIAS must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Checked on 6.0.0.438, 5.0.2.1479, 4.0.6.3142.

    [24.05.2025] pzotov
    Splitted expected* variables for versions up to 5.x and 6.x+
    This is needed after 11d5d5 ("Fix for #8082 ... user buffers directly (#8145)") by Dmitry Sibiryakov.
    Discussed in email 24.05.2025 22:06, subj: "one more consequence of 11d5d5 ..." (since 15.05.2025 17:25).
"""

import re
from pathlib import Path
import shutil

import pytest
from firebird.qa import *

# Pre-defined alias for test DB in the QA_root/files/qa-databases.conf.
# This file (qa-databases.conf) must be copied manually to each testing
# FB home folder, with replacing databases.conf there:
#
REQUIRED_ALIAS = 'tmp_gh_6416_alias'

init_sql = f'''
   set bail on;
   recreate table test(
      f_sml smallint default -32768
     ,f_int int default -2147483648
     ,f_big bigint default -9223372036854775808
     ,f_128 int128 default -170141183460469231731687303715884105728
     ,f_num numeric(38) default -170141183460469231731687303715884105728
     ,f_dec decfloat default -9.999999999999999999999999999999999E+6144
     ,f_tz time with time zone default '01:02:03 Indian/Cocos'
     ,f_tsz timestamp with time zone default '22.09.2023 01:02:03 Indian/Cocos'
   );
   insert into test default values;
   commit;
'''

db = db_factory(init = init_sql)

substitutions = [('^((?!(SQLSTATE|error|Floating-point overflow|sqltype)).)*$', ''), ('[ \t]+', ' ')]
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):


    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_db_for_3x_client).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_6416_alias = $(dir_sampleDb)/qa/tmp_gh_6416.fdb
                # - then we extract filename: 'tmp_gh_6416.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_db_for_3x_client = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )
    shutil.copy2(act.db.db_path, tmp_db_for_3x_client)

    test_sql = f'''
       set bail on;
       set list on;
       connect '{REQUIRED_ALIAS}' user {act.db.user};
       -- select mon$database_name from mon$database;
       set sqlda_display on;
       select *
       from test;
    '''

    expected_out_5x = f"""
        01: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
        02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        04: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        05: sqltype: 580 INT64 Nullable scale: 0 subtype: 1 len: 8
        06: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
        07: sqltype: 560 TIME Nullable scale: 0 subtype: 0 len: 4
        08: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
        Statement failed, SQLSTATE = 22003
        -SQL error code = -303
        -Floating-point overflow. The exponent of a floating-point operation is greater than the magnitude allowed.
    """

    expected_out_6x = f"""
        01: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
        02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        04: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        05: sqltype: 580 INT64 Nullable scale: 0 subtype: 1 len: 8
        06: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
        07: sqltype: 560 TIME Nullable scale: 0 subtype: 0 len: 4
        08: sqltype: 510 TIMESTAMP Nullable scale: 0 subtype: 0 len: 8
        Statement failed, SQLSTATE = 22003
        Floating-point overflow. The exponent of a floating-point operation is greater than the magnitude allowed.
    """

    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.isql(switches = ['-q'], input = test_sql, combine_output = True, credentials = False, connect_db = False)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
    tmp_db_for_3x_client.unlink()
    
    #-------------------------------------------------------------

    test_sql = f'''
       set bail on;
       set list on;
       set sqlda_display on;
       select *
       from test;
    '''
    act.expected_stdout = """
        01: sqltype: 500 SHORT Nullable scale: 0 subtype: 0 len: 2
        02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
        03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        04: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
        05: sqltype: 32752 INT128 Nullable scale: 0 subtype: 1 len: 16
        06: sqltype: 32762 DECFLOAT(34) Nullable scale: 0 subtype: 0 len: 16
        07: sqltype: 32756 TIME WITH TIME ZONE Nullable scale: 0 subtype: 0 len: 8
        08: sqltype: 32754 TIMESTAMP WITH TIME ZONE Nullable scale: 0 subtype: 0 len: 12
    """
    act.isql(switches = ['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
