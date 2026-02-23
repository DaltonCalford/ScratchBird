#coding:utf-8

"""
ID:          issue-6183
ISSUE:       6183
TITLE:       Attempt to create mapping with non-ascii user name which is encoded in
  SINGLE-BYTE codepage leads to '-Malformed string'
DESCRIPTION:
  NB: different data are used for FB 3.x and 4.x because DDL in 4.x allows to store names with length up to 63 character.
  See variables 'mapping_name' and 'non_ascii_user_name'.
  FB 3.x restricts max_length of DB object name with value = 31 (bytes, not character!).
JIRA:        CORE-5926
FBTEST:      bugs.core_5926
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory(charset='WIN1252')

act = python_act('db')

expected_stdout = """
    RDB$MAP_USING                   P
    RDB$MAP_DB                      <null>
    RDB$MAP_FROM_TYPE               USER
    RDB$MAP_TO_TYPE                 0

    Records affected: 1
"""

test_script = temp_file('test_script.sql')

@pytest.mark.intl
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, test_script: Path):
    if act.is_version('<4'):
        # Maximal length of user name in FB 3.x is 31 (charset unicode_fss).
        #mapping_name = 'áâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ'
        mapping_name  = 'áâãäåæçèéêëìíîï1'
        # mapping_name  = 'áâãäåæçèéêëìíîïð'
        non_ascii_user_name = 'ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞ'
        ascii_only_user_name = 'ABCDEFGHIJKLMNOPQRSTUWYXYZ12345'
    else:
        # Maximal length of user name in FB 4.x is 63 (charset utf8).
        #
        mapping_name = 'áâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿŒ'
        non_ascii_user_name = 'ÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿŒœŠšŸŽžƒ'
        ascii_only_user_name = 'ABCDEFGHIJKLMNOPQRSTUWYXYZ12345ABCDEFGHIJKLMNOPQRSTUWYXYZ123456'
    #
    plugin_for_mapping = 'Srp'
    test_script.write_text(f"""
        create or alter mapping "{mapping_name}" using plugin {plugin_for_mapping} from user '{non_ascii_user_name}' to user "{ascii_only_user_name}";
        commit;
        -- show mapping;
        set count on;
        set list on;
        select
        rdb$map_using
        ,rdb$map_db
        ,rdb$map_from_type
        ,rdb$map_to_type
        -- ,rdb$map_plugin
        -- 03.03.2021: do NOT show because it differs for FB 3.x and 4.x: ,rdb$map_from
        -- 03.03.2021: do NOT show because it differs for FB 3.x and 4.x: ,rdb$map_to
        from rdb$auth_mapping
        where
        upper(rdb$map_name) = upper('{mapping_name}')
        and rdb$map_plugin = upper('{plugin_for_mapping}')
        and rdb$map_from = '{non_ascii_user_name}'
        and rdb$map_to containing '{ascii_only_user_name}'
        ;
        commit;
        """, encoding='cp1252')
    act.expected_stdout = expected_stdout
    act.isql(switches=['-b', '-q'], input_file=test_script, charset='WIN1252')
    assert act.clean_stdout == act.clean_expected_stdout
