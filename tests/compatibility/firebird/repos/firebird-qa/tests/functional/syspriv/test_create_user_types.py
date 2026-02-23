#coding:utf-8

"""
ID:          syspriv.create-user-types
TITLE:       Check ability to update content of RDB$TYPES
DESCRIPTION:
FBTEST:      functional.syspriv.create_user_types
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name = 'dba_helper_create_usr_types', password = '1234')
tmp_role = role_factory('db', name = 'role_for_create_user_types')

substitutions=[('RDB\\$DESCRIPTION.*', 'RDB$DESCRIPTION'), ('[ \t]+', ' ')]
act = isql_act('db', substitutions = substitutions)


@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user, tmp_role):

    test_script = f"""
        set wng off;
        set list on;

        create or alter view v_check as
        select
             current_user as who_ami
            ,r.rdb$role_name as my_role
            ,rdb$role_in_use(r.rdb$role_name) as rdb_roles_in_use
            ,r.rdb$system_privileges as sys_privileges
        from mon$database m
        cross join rdb$roles r;
        commit;
        grant select on v_check to public;
        commit;

        revoke all on all from {tmp_user.name};
        commit;

        -- Add/change/delete non-system records in RDB$TYPES
        -- create role {tmp_role.name} set system privileges to CREATE_USER_TYPES;
        alter role {tmp_role.name} set system privileges to CREATE_USER_TYPES;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user.name} password '{tmp_user.password}';
        select * from v_check;
        commit;

        insert into rdb$types(rdb$field_name, rdb$type, rdb$type_name, rdb$description, rdb$system_flag)
          values( 'amount_avaliable',
                  -32767,
                  'stock_amount',
                  'Total number of units that can be sold immediately to any customer',
                  0 -- rdb$system_flag
                )
        returning rdb$field_name, rdb$type, rdb$type_name, rdb$description, rdb$system_flag
        ;

        insert into rdb$types(rdb$field_name, rdb$type, rdb$type_name, rdb$description, rdb$system_flag)
          values( 'amount_ion_reserve',
                  -2,
                  'stock_amount',
                  'Total number of units that is to be sold for customers who previously did order them',
                  1 -- rdb$system_flag
                );

        update rdb$types set rdb$type = -32768, rdb$field_name = null
        where rdb$type < 0
        order by rdb$type
        rows 1
        returning rdb$field_name, rdb$type, rdb$type_name, rdb$description, rdb$system_flag;

        delete from rdb$types where rdb$type < 0
        returning rdb$field_name, rdb$type, rdb$type_name,
        -- rdb$description, -- TODO: uncomment this after #5565 (core-5287) will be fixed
        rdb$system_flag
        ;
        commit;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"SYSTEM".'
    RDB_TYPES_NAME = 'RDB$TYPES' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"RDB$TYPES"'
    expected_stdout = f"""
        WHO_AMI                         DBA_HELPER_CREATE_USR_TYPES
        MY_ROLE                         RDB$ADMIN
        RDB_ROLES_IN_USE                <false>
        SYS_PRIVILEGES                  FFFFFFFFFFFFFFFF

        WHO_AMI                         DBA_HELPER_CREATE_USR_TYPES
        MY_ROLE                         ROLE_FOR_CREATE_USER_TYPES
        RDB_ROLES_IN_USE                <true>
        SYS_PRIVILEGES                  0800000000000000

        RDB$FIELD_NAME                  amount_avaliable
        RDB$TYPE                        -32767
        RDB$TYPE_NAME                   stock_amount
        RDB$DESCRIPTION
        Total number of units that can be sold immediately to any customer
        RDB$SYSTEM_FLAG                 0

        Statement failed, SQLSTATE = 42000
        INSERT operation is not allowed for system table {RDB_TYPES_NAME}

        RDB$FIELD_NAME                  <null>
        RDB$TYPE                        -32768
        RDB$TYPE_NAME                   stock_amount
        RDB$DESCRIPTION
        Total number of units that can be sold immediately to any customer
        RDB$SYSTEM_FLAG                 0

        RDB$FIELD_NAME                  <null>
        RDB$TYPE                        -32768
        RDB$TYPE_NAME                   stock_amount
        RDB$SYSTEM_FLAG                 0
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
