#coding:utf-8

"""
ID:          intfunc.list-02
ISSUE:       1367
TITLE:       List function with delimiter specified
DESCRIPTION:
JIRA:        CORE-964
FBTEST:      functional.intfunc.list.02
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(
      rel_name char(31) character set unicode_fss
      ,idx_name char(31) character set unicode_fss
    );
    commit;

    insert into test values ('RDB$RELATIONS', 'RDB$INDEX_0');
    insert into test values ('RDB$RELATIONS', 'RDB$INDEX_1');
    insert into test values ('RDB$FIELDS', 'RDB$INDEX_2');
    insert into test values ('RDB$RELATION_FIELDS', 'RDB$INDEX_3');
    insert into test values ('RDB$RELATION_FIELDS', 'RDB$INDEX_4');
    insert into test values ('RDB$INDICES', 'RDB$INDEX_5');
    insert into test values ('RDB$INDEX_SEGMENTS', 'RDB$INDEX_6');
    insert into test values ('RDB$SECURITY_CLASSES', 'RDB$INDEX_7');
    insert into test values ('RDB$TRIGGERS', 'RDB$INDEX_8');
    insert into test values ('RDB$FUNCTIONS', 'RDB$INDEX_9');
    insert into test values ('RDB$FUNCTION_ARGUMENTS', 'RDB$INDEX_10');
    insert into test values ('RDB$GENERATORS', 'RDB$INDEX_11');
    insert into test values ('RDB$RELATION_CONSTRAINTS', 'RDB$INDEX_12');
    insert into test values ('RDB$REF_CONSTRAINTS', 'RDB$INDEX_13');
    insert into test values ('RDB$CHECK_CONSTRAINTS', 'RDB$INDEX_14');
    insert into test values ('RDB$RELATION_FIELDS', 'RDB$INDEX_15');
    insert into test values ('RDB$FORMATS', 'RDB$INDEX_16');
    insert into test values ('RDB$FILTERS', 'RDB$INDEX_17');
    insert into test values ('RDB$PROCEDURE_PARAMETERS', 'RDB$INDEX_18');
    insert into test values ('RDB$CHARACTER_SETS', 'RDB$INDEX_19');
    insert into test values ('RDB$COLLATIONS', 'RDB$INDEX_20');
    insert into test values ('RDB$PROCEDURES', 'RDB$INDEX_21');
    insert into test values ('RDB$PROCEDURES', 'RDB$INDEX_22');
    insert into test values ('RDB$EXCEPTIONS', 'RDB$INDEX_23');
    insert into test values ('RDB$EXCEPTIONS', 'RDB$INDEX_24');
    insert into test values ('RDB$CHARACTER_SETS', 'RDB$INDEX_25');
    insert into test values ('RDB$COLLATIONS', 'RDB$INDEX_26');
    insert into test values ('RDB$DEPENDENCIES', 'RDB$INDEX_27');
    insert into test values ('RDB$DEPENDENCIES', 'RDB$INDEX_28');
    insert into test values ('RDB$USER_PRIVILEGES', 'RDB$INDEX_29');
    insert into test values ('RDB$USER_PRIVILEGES', 'RDB$INDEX_30');
    insert into test values ('RDB$INDICES', 'RDB$INDEX_31');
    insert into test values ('RDB$TRANSACTIONS', 'RDB$INDEX_32');
    insert into test values ('RDB$VIEW_RELATIONS', 'RDB$INDEX_33');
    insert into test values ('RDB$VIEW_RELATIONS', 'RDB$INDEX_34');
    insert into test values ('RDB$TRIGGER_MESSAGES', 'RDB$INDEX_35');
    insert into test values ('RDB$FIELD_DIMENSIONS', 'RDB$INDEX_36');
    insert into test values ('RDB$TYPES', 'RDB$INDEX_37');
    insert into test values ('RDB$TRIGGERS', 'RDB$INDEX_38');
    insert into test values ('RDB$ROLES', 'RDB$INDEX_39');
    insert into test values ('RDB$CHECK_CONSTRAINTS', 'RDB$INDEX_40');
    insert into test values ('RDB$INDICES', 'RDB$INDEX_41');
    insert into test values ('RDB$RELATION_CONSTRAINTS', 'RDB$INDEX_42');
    insert into test values ('RDB$RELATION_CONSTRAINTS', 'RDB$INDEX_43');
    insert into test values ('RDB$BACKUP_HISTORY', 'RDB$INDEX_44');
    insert into test values ('RDB$FILTERS', 'RDB$INDEX_45');
    insert into test values ('RDB$GENERATORS', 'RDB$INDEX_46');
    insert into test values ('RDB$PACKAGES', 'RDB$INDEX_47');
    insert into test values ('RDB$PROCEDURE_PARAMETERS', 'RDB$INDEX_48');
    insert into test values ('RDB$FUNCTION_ARGUMENTS', 'RDB$INDEX_49');
    insert into test values ('RDB$PROCEDURE_PARAMETERS', 'RDB$INDEX_50');
    insert into test values ('RDB$FUNCTION_ARGUMENTS', 'RDB$INDEX_51');
    insert into test values ('RDB$AUTH_MAPPING', 'RDB$INDEX_52');
    commit;

    create index test_rel_name_idx on test(rel_name);
    commit;
  """

db = db_factory(init=init_script)

test_script = """
    set list on;
    set blob all;
    select x.rel_name, list(trim(x.idx_name), ':') "list_blob_id"
    from test x
    where x.rel_name starting with upper('rdb$')
    group by 1;
"""

act = isql_act('db', test_script, substitutions=[('list_blob_id.*', '')])

expected_stdout = """
    REL_NAME                        RDB$AUTH_MAPPING
    list_blob_id                    0:1
    RDB$INDEX_52

    REL_NAME                        RDB$BACKUP_HISTORY
    list_blob_id                    0:2
    RDB$INDEX_44

    REL_NAME                        RDB$CHARACTER_SETS
    list_blob_id                    0:3
    RDB$INDEX_19:RDB$INDEX_25

    REL_NAME                        RDB$CHECK_CONSTRAINTS
    list_blob_id                    0:4
    RDB$INDEX_14:RDB$INDEX_40

    REL_NAME                        RDB$COLLATIONS
    list_blob_id                    0:5
    RDB$INDEX_20:RDB$INDEX_26

    REL_NAME                        RDB$DEPENDENCIES
    list_blob_id                    0:6
    RDB$INDEX_27:RDB$INDEX_28

    REL_NAME                        RDB$EXCEPTIONS
    list_blob_id                    0:7
    RDB$INDEX_23:RDB$INDEX_24

    REL_NAME                        RDB$FIELDS
    list_blob_id                    0:8
    RDB$INDEX_2

    REL_NAME                        RDB$FIELD_DIMENSIONS
    list_blob_id                    0:9
    RDB$INDEX_36

    REL_NAME                        RDB$FILTERS
    list_blob_id                    0:a
    RDB$INDEX_17:RDB$INDEX_45

    REL_NAME                        RDB$FORMATS
    list_blob_id                    0:b
    RDB$INDEX_16

    REL_NAME                        RDB$FUNCTIONS
    list_blob_id                    0:c
    RDB$INDEX_9

    REL_NAME                        RDB$FUNCTION_ARGUMENTS
    list_blob_id                    0:d
    RDB$INDEX_10:RDB$INDEX_49:RDB$INDEX_51

    REL_NAME                        RDB$GENERATORS
    list_blob_id                    0:e
    RDB$INDEX_11:RDB$INDEX_46

    REL_NAME                        RDB$INDEX_SEGMENTS
    list_blob_id                    0:f
    RDB$INDEX_6

    REL_NAME                        RDB$INDICES
    list_blob_id                    0:10
    RDB$INDEX_5:RDB$INDEX_31:RDB$INDEX_41

    REL_NAME                        RDB$PACKAGES
    list_blob_id                    0:11
    RDB$INDEX_47

    REL_NAME                        RDB$PROCEDURES
    list_blob_id                    0:12
    RDB$INDEX_21:RDB$INDEX_22

    REL_NAME                        RDB$PROCEDURE_PARAMETERS
    list_blob_id                    0:13
    RDB$INDEX_18:RDB$INDEX_48:RDB$INDEX_50

    REL_NAME                        RDB$REF_CONSTRAINTS
    list_blob_id                    0:14
    RDB$INDEX_13

    REL_NAME                        RDB$RELATIONS
    list_blob_id                    0:15
    RDB$INDEX_0:RDB$INDEX_1

    REL_NAME                        RDB$RELATION_CONSTRAINTS
    list_blob_id                    0:16
    RDB$INDEX_12:RDB$INDEX_42:RDB$INDEX_43

    REL_NAME                        RDB$RELATION_FIELDS
    list_blob_id                    0:17
    RDB$INDEX_3:RDB$INDEX_4:RDB$INDEX_15

    REL_NAME                        RDB$ROLES
    list_blob_id                    0:18
    RDB$INDEX_39

    REL_NAME                        RDB$SECURITY_CLASSES
    list_blob_id                    0:19
    RDB$INDEX_7

    REL_NAME                        RDB$TRANSACTIONS
    list_blob_id                    0:1a
    RDB$INDEX_32

    REL_NAME                        RDB$TRIGGERS
    list_blob_id                    0:1b
    RDB$INDEX_8:RDB$INDEX_38

    REL_NAME                        RDB$TRIGGER_MESSAGES
    list_blob_id                    0:1c
    RDB$INDEX_35

    REL_NAME                        RDB$TYPES
    list_blob_id                    0:1d
    RDB$INDEX_37

    REL_NAME                        RDB$USER_PRIVILEGES
    list_blob_id                    0:1e
    RDB$INDEX_29:RDB$INDEX_30

    REL_NAME                        RDB$VIEW_RELATIONS
    list_blob_id                    0:1f
    RDB$INDEX_33:RDB$INDEX_34
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
