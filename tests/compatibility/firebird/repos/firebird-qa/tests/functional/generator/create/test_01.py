#coding:utf-8

"""
ID:          generator.create-01
FBTEST:      functional.generator.create.01
TITLE:       CREATE GENERATOR and query related data from RDB$GENERATORS
DESCRIPTION:
    Run 'CREATE GENERATOR' statement and obtain data about it from system table (rdb$generators).
NOTES:
    [07.08.2020]
    we have to separate test for 3.0 and 4.0 because INITIAL value of new sequence
    in FB 4.x now differs from "old good zero" (this is so since CORE-6084 was fixed).
    See also: doc/README.incompatibilities.3to4.txt
"""

import pytest
from firebird.qa import *

db = db_factory()
substitutions = [  ('[ \t]+', ' ')
                  ,('RDB\\$SECURITY_CLASS[ ]+SQL\\$\\d+', 'RDB$SECURITY_CLASS x')
                  ,('RDB\\$GENERATOR_ID[ ]+\\d+', 'RDB$GENERATOR_ID x')
                ]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_2(act: Action):

    SQL_SCHEMA_IN_RDB_FIELD = '' if act.is_version('<6') else ',g.rdb$schema_name'
    test_script = f"""
        create generator test;
        commit;
        set list on;
        select
            g.rdb$generator_name
           ,g.rdb$generator_id
           ,g.rdb$system_flag
           ,g.rdb$description
           ,g.rdb$security_class
           ,g.rdb$owner_name
           ,g.rdb$initial_value
           ,g.rdb$generator_increment
           {SQL_SCHEMA_IN_RDB_FIELD}
        from rdb$generators g where g.rdb$generator_name = upper('test');
    """
    
    expected_stdout_5x = """
        RDB$GENERATOR_NAME TEST
        RDB$GENERATOR_ID x
        RDB$SYSTEM_FLAG 0
        RDB$DESCRIPTION <null>
        RDB$SECURITY_CLASS x
        RDB$OWNER_NAME SYSDBA
        RDB$INITIAL_VALUE 1
        RDB$GENERATOR_INCREMENT 1
    """

    expected_stdout_6x = """
        RDB$GENERATOR_NAME TEST
        RDB$GENERATOR_ID x
        RDB$SYSTEM_FLAG 0
        RDB$DESCRIPTION <null>
        RDB$SECURITY_CLASS x
        RDB$OWNER_NAME SYSDBA
        RDB$INITIAL_VALUE 1
        RDB$GENERATOR_INCREMENT 1
        RDB$SCHEMA_NAME PUBLIC
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], combine_output = True, input = test_script)
    assert act.clean_stdout == act.clean_expected_stdout
