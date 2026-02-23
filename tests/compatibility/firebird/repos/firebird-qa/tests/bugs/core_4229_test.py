#coding:utf-8

"""
ID:          issue-4553
ISSUE:       4553
TITLE:       Bidirectional cursor is not positioned by the first call of FETCH LAST
DESCRIPTION:
JIRA:        CORE-4229
FBTEST:      bugs.core_4229
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test (name char(31) character set unicode_fss);
    commit;
    insert into test (name) values ('rdb$view_context');
    insert into test (name) values ('rdb$context_name');
    insert into test (name) values ('rdb$description');
    insert into test (name) values ('rdb$edit_string');
    insert into test (name) values ('rdb$field_id');
    insert into test (name) values ('rdb$field_name');
    insert into test (name) values ('rdb$system_flag');
    insert into test (name) values ('rdb$system_nullflag');
    insert into test (name) values ('rdb$index_id');
    insert into test (name) values ('rdb$index_name');
    insert into test (name) values ('rdb$field_length');
    insert into test (name) values ('rdb$field_position');
    insert into test (name) values ('rdb$field_scale');
    insert into test (name) values ('rdb$field_type');
    insert into test (name) values ('rdb$format');
    insert into test (name) values ('rdb$dbkey_length');
    insert into test (name) values ('rdb$page_number');
    insert into test (name) values ('rdb$page_sequence');
    insert into test (name) values ('rdb$page_type');
    insert into test (name) values ('rdb$query_header');
    insert into test (name) values ('rdb$relation_id');
    insert into test (name) values ('rdb$relation_name');
    insert into test (name) values ('rdb$segment_count');
    insert into test (name) values ('rdb$segment_length');
    insert into test (name) values ('rdb$source');
    insert into test (name) values ('rdb$field_sub_type');
    insert into test (name) values ('rdb$view_blr');
    insert into test (name) values ('rdb$validation_blr');
    insert into test (name) values ('rdb$value');
    insert into test (name) values ('rdb$security_class');
    insert into test (name) values ('rdb$acl');
    insert into test (name) values ('rdb$file_name');
    insert into test (name) values ('rdb$file_name2');
    insert into test (name) values ('rdb$file_sequence');
    insert into test (name) values ('rdb$file_start');
    insert into test (name) values ('rdb$file_length');
    insert into test (name) values ('rdb$file_flags');
    insert into test (name) values ('rdb$trigger_blr');
    insert into test (name) values ('rdb$trigger_name');
    insert into test (name) values ('rdb$generic_name');
    insert into test (name) values ('rdb$function_name');
    insert into test (name) values ('rdb$external_name');
    insert into test (name) values ('rdb$type_name');
    insert into test (name) values ('rdb$dimensions');
    insert into test (name) values ('rdb$runtime');
    insert into test (name) values ('rdb$trigger_sequence');
    insert into test (name) values ('rdb$generic_type');
    insert into test (name) values ('rdb$trigger_type');
    insert into test (name) values ('rdb$object_type');
    insert into test (name) values ('rdb$mechanism');
    insert into test (name) values ('rdb$descriptor');
    insert into test (name) values ('rdb$function_type');
    insert into test (name) values ('rdb$transaction_id');
    insert into test (name) values ('rdb$transaction_state');
    insert into test (name) values ('rdb$timestamp');
    insert into test (name) values ('rdb$transaction_description');
    insert into test (name) values ('rdb$message');
    insert into test (name) values ('rdb$message_number');
    insert into test (name) values ('rdb$user');
    insert into test (name) values ('rdb$privilege');
    insert into test (name) values ('rdb$external_description');
    insert into test (name) values ('rdb$shadow_number');
    insert into test (name) values ('rdb$generator_name');
    insert into test (name) values ('rdb$generator_id');
    insert into test (name) values ('rdb$bound');
    insert into test (name) values ('rdb$dimension');
    insert into test (name) values ('rdb$statistics');
    insert into test (name) values ('rdb$null_flag');
    insert into test (name) values ('rdb$constraint_name');
    insert into test (name) values ('rdb$constraint_type');
    insert into test (name) values ('rdb$deferrable');
    insert into test (name) values ('rdb$match_option');
    insert into test (name) values ('rdb$rule');
    insert into test (name) values ('rdb$file_partitions');
    insert into test (name) values ('rdb$procedure_blr');
    insert into test (name) values ('rdb$procedure_id');
    insert into test (name) values ('rdb$procedure_parameters');
    insert into test (name) values ('rdb$procedure_name');
    insert into test (name) values ('rdb$parameter_name');
    insert into test (name) values ('rdb$parameter_number');
    insert into test (name) values ('rdb$parameter_type');
    insert into test (name) values ('rdb$character_set_name');
    insert into test (name) values ('rdb$character_set_id');
    insert into test (name) values ('rdb$collation_name');
    insert into test (name) values ('rdb$collation_id');
    insert into test (name) values ('rdb$number_of_characters');
    insert into test (name) values ('rdb$exception_name');
    insert into test (name) values ('rdb$exception_number');
    insert into test (name) values ('rdb$file_p_offset');
    insert into test (name) values ('rdb$field_precision');
    insert into test (name) values ('rdb$backup_id');
    insert into test (name) values ('rdb$backup_level');
    insert into test (name) values ('rdb$guid');
    insert into test (name) values ('rdb$scn');
    insert into test (name) values ('rdb$specific_attributes');
    insert into test (name) values ('rdb$plugin');
    insert into test (name) values ('rdb$relation_type');
    insert into test (name) values ('rdb$procedure_type');
    insert into test (name) values ('rdb$attachment_id');
    insert into test (name) values ('rdb$statement_id');
    insert into test (name) values ('rdb$call_id');
    insert into test (name) values ('rdb$stat_id');
    insert into test (name) values ('rdb$pid');
    insert into test (name) values ('rdb$state');
    insert into test (name) values ('rdb$ods_number');
    insert into test (name) values ('rdb$page_size');
    insert into test (name) values ('rdb$page_buffers');
    insert into test (name) values ('rdb$shutdown_mode');
    insert into test (name) values ('rdb$sql_dialect');
    insert into test (name) values ('rdb$sweep_interval');
    insert into test (name) values ('rdb$counter');
    insert into test (name) values ('rdb$remote_protocol');
    insert into test (name) values ('rdb$remote_address');
    insert into test (name) values ('rdb$isolation_mode');
    insert into test (name) values ('rdb$lock_timeout');
    insert into test (name) values ('rdb$backup_state');
    insert into test (name) values ('rdb$stat_group');
    insert into test (name) values ('rdb$debug_info');
    insert into test (name) values ('rdb$parameter_mechanism');
    insert into test (name) values ('rdb$source_info');
    insert into test (name) values ('rdb$context_var_name');
    insert into test (name) values ('rdb$context_var_value');
    insert into test (name) values ('rdb$engine_name');
    insert into test (name) values ('rdb$package_name');
    insert into test (name) values ('rdb$function_id');
    insert into test (name) values ('rdb$function_blr');
    insert into test (name) values ('rdb$argument_name');
    insert into test (name) values ('rdb$argument_mechanism');
    insert into test (name) values ('rdb$identity_type');
    insert into test (name) values ('rdb$boolean');
    insert into test (name) values ('sec$user_name');
    insert into test (name) values ('sec$key');
    insert into test (name) values ('sec$value');
    insert into test (name) values ('sec$name_part');
    insert into test (name) values ('rdb$client_version');
    insert into test (name) values ('rdb$remote_version');
    insert into test (name) values ('rdb$host_name');
    insert into test (name) values ('rdb$os_user');
    insert into test (name) values ('rdb$generator_value');
    insert into test (name) values ('rdb$auth_method');
    insert into test (name) values ('rdb$linger');
    insert into test (name) values ('rdb$map_name');
    insert into test (name) values ('rdb$map_using');
    insert into test (name) values ('rdb$map_db');
    insert into test (name) values ('rdb$map_from_type');
    insert into test (name) values ('rdb$map_from');
    insert into test (name) values ('rdb$map_to');
    insert into test (name) values ('rdb$generator_increment');
    insert into test (name) values ('rdb$plan');
    insert into test (name) values ('rdb$1');
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;

    set term ^;
    execute block returns (name type of column test.name, rc int)
    as
        declare c scroll cursor for (select t.name from test t);
    begin
      open c;
      fetch first from c
      into :name;
      rc = row_count;
      suspend;

      fetch last from c
      into :name;
      rc = row_count;
      suspend;

      close c;
    end
    ^

    execute block returns (name type of column test.name, rc int)
    as
        declare c scroll cursor for (select t.name from test t);
    begin
      open c;
      fetch last from c
      into :name;
      rc = row_count;
      suspend;

      close c;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    NAME                            rdb$view_context
    RC                              1
    NAME                            rdb$1
    RC                              1
    NAME                            rdb$1
    RC                              1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
