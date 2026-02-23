#coding:utf-8

"""
ID:          issue-4080
ISSUE:       4080
TITLE:       Unprivileged user can delete from RDB$DATABASE, RDB$COLLATIONS, RDB$CHARACTER_SETS
DESCRIPTION:
JIRA:        CORE-3735
FBTEST:      bugs.core_3735
NOTES:
    [28.06.2025] pzotov
    Reimplemented: use variables to be used (via f-notations) in expected_out_* instead of hard-coding.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c3735', password='123')

substitutions = [('[ \t]+', ' '), ('/\\* Grant permissions for.*','')]

act = isql_act('db', substitutions=substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        -- See also more complex test in CORE-4731 // Prohibit an ability to issue DML or DDL statements on RDB$ tables

        connect '{act.db.dsn}' user '{tmp_user.name}' password '{tmp_user.password}';

        set list on;
        set blob all;
        select current_user from rdb$database;
        show grants;
        set count on;

        insert into rdb$character_sets(
            rdb$character_set_name
            ,rdb$form_of_use
            ,rdb$number_of_characters
            ,rdb$default_collate_name
            ,rdb$character_set_id
            ,rdb$system_flag
            ,rdb$description
            ,rdb$function_name
            ,rdb$bytes_per_character
        )values (
            'ISO-8859-15',
            null,
            null,
            'ISO-8859-15',
            ( select max(rdb$character_set_id) from rdb$character_sets ) + 1,
            1,
            null,
            null,
            1
        ) returning
            rdb$character_set_name,
            rdb$character_set_id,
            rdb$default_collate_name
        ;

        insert into rdb$collations(
            rdb$collation_name
            ,rdb$collation_id
            ,rdb$character_set_id
            ,rdb$collation_attributes
            ,rdb$system_flag
            ,rdb$description
            ,rdb$function_name
            ,rdb$base_collation_name
            ,rdb$specific_attributes
        ) values(
            'SUPER_SMART_ORDER'
            ,( select max(rdb$collation_id) from rdb$collations ) + 1
            ,( select rdb$character_set_id from rdb$character_sets where upper(rdb$character_set_name) = upper('ISO-8859-15')  )
            ,1
            ,1
            ,null
            ,null
            ,null
            ,null
        ) returning
            rdb$collation_name
            ,rdb$collation_id
            ,rdb$character_set_id
        ;


        insert into rdb$database(
            rdb$description
            ,rdb$relation_id
            ,rdb$security_class
            ,rdb$character_set_name
        ) values (
            'This is special record, do not delete it!'
           ,( select max(rdb$relation_id) from rdb$relations ) + 1
           ,null
           ,'ISO_HE_HE'
        ) returning
            rdb$description
            ,rdb$relation_id
            ,rdb$security_class
            ,rdb$character_set_name
        ;


        update rdb$collations set rdb$description = null rows 1
        returning
            rdb$collation_id
        ;

        update rdb$character_sets set rdb$description = null rows 1
        returning
            rdb$character_set_id
        ;

        update rdb$database set rdb$character_set_name = 'ISO_HA_HA'
        returning
            rdb$relation_id
        ;

        delete from rdb$collations order by rdb$collation_id desc rows 1
        returning
            rdb$collation_name
            ,rdb$collation_id
            ,rdb$character_set_id
        ;

        delete from rdb$character_sets order by rdb$character_set_id desc rows 1
        returning
            rdb$character_set_name,
            rdb$character_set_id,
            rdb$default_collate_name
        ;

        delete from rdb$database order by rdb$relation_id desc rows 1
        returning
            rdb$description
            ,rdb$relation_id
            ,rdb$security_class
            ,rdb$character_set_name
        ;

        commit;
    """

    expected_stdout_3x = f"""
        USER                            {tmp_user.name.upper()}
        There is no privilege granted in this database
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$CHARACTER_SETS
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$COLLATIONS
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$DATABASE
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$COLLATIONS
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$CHARACTER_SETS
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$DATABASE
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$COLLATIONS
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$CHARACTER_SETS
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$DATABASE
    """

    expected_stdout_5x = f"""
        USER                            {tmp_user.name.upper()}
        There is no privilege granted in this database
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$CHARACTER_SETS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$COLLATIONS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$COLLATIONS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$CHARACTER_SETS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$COLLATIONS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$CHARACTER_SETS
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE RDB$DATABASE
        -Effective user is {tmp_user.name.upper()}
    """

    expected_stdout_6x = f"""
        USER                            {tmp_user.name.upper()}
        
        GRANT USAGE ON SCHEMA PUBLIC TO PUBLIC
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE "SYSTEM"."RDB$CHARACTER_SETS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE "SYSTEM"."RDB$COLLATIONS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for INSERT access to TABLE "SYSTEM"."RDB$DATABASE"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE "SYSTEM"."RDB$COLLATIONS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE "SYSTEM"."RDB$CHARACTER_SETS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for UPDATE access to TABLE "SYSTEM"."RDB$DATABASE"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE "SYSTEM"."RDB$COLLATIONS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE "SYSTEM"."RDB$CHARACTER_SETS"
        -Effective user is {tmp_user.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for DELETE access to TABLE "SYSTEM"."RDB$DATABASE"
        -Effective user is {tmp_user.name.upper()}
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], connect_db = False, input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
