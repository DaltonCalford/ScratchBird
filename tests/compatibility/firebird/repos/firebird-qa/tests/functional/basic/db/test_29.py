#coding:utf-8

"""
ID:          new-database-29
TITLE:       New DB - RDB$TRIGGERS content
DESCRIPTION: Check the correct content of RDB$TRIGGERS in new database.
FBTEST:      functional.basic.db.29
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
    set blob all;
    set count on;
    select * from rdb$triggers rt order by rt.rdb$trigger_name;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$TRIGGER_BLR.*', ''),
                                                 ('RDB\\$TRIGGER_NAME[\\s]+RDB\\$TRIGGER.*',
                                                  'RDB\\$TRIGGER_NAME RDB\\$TRIGGER')])

# version: 3.0

expected_stdout_1 = """
    RDB$TRIGGER_NAME                RDB$TRIGGER_1
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:0
            	blr_version5,
            	blr_leave, 0,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_10
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:7
            	blr_version5,
            	blr_begin,
            	   blr_label, 0,
            	      blr_begin,
            	         blr_begin,
            	            blr_if,
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text2, 0,0, 6,0, 'U','N','I','Q','U','E',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_if,
            	                        blr_any,
            	                           blr_rse, 1,
            	                              blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 2,
            	                              blr_boolean,
            	                                 blr_eql,
            	                                    blr_field, 2, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
            	                                    blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                              blr_end,
            	                        blr_begin,
            	                           blr_begin,
            	                              blr_leave, 1,
            	                              blr_end,
            	                           blr_end,
            	                        blr_end,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text2, 0,0, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_for,
            	                        blr_rse, 1,
            	                           blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	                           blr_boolean,
            	                              blr_eql,
            	                                 blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                 blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                           blr_end,
            	                        blr_erase, 3,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text2, 0,0, 8,0, 'N','O','T',32,'N','U','L','L',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_label, 1,
            	                        blr_for,
            	                           blr_rse, 5,
            	                              blr_relation2, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S',
            	                                 9, 'C','1',32,'C','H','K','C','O','N', 4,
            	                              blr_relation2, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S',
            	                                 6, 'C','1',32,'R','F','L', 5,
            	                              blr_relation2, 10, 'R','D','B','$','F','I','E','L','D','S',
            	                                 6, 'C','1',32,'F','L','D', 6,
            	                              blr_relation2, 11, 'R','D','B','$','I','N','D','I','C','E','S',
            	                                 6, 'C','1',32,'I','N','D', 7,
            	                              blr_relation2, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S',
            	                                 9, 'C','1',32,'I','D','X','S','E','G', 8,
            	                              blr_boolean,
            	                                 blr_and,
            	                                    blr_and,
            	                                       blr_and,
            	                                          blr_and,
            	                                             blr_and,
            	                                                blr_and,
            	                                                   blr_and,
            	                                                      blr_eql,
            	                                                         blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                                         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                                      blr_eql,
            	                                                         blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                         blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                                   blr_eql,
            	                                                      blr_field, 5, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                      blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                blr_eql,
            	                                                   blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                   blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                             blr_eql,
            	                                                blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                blr_field, 5, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                                          blr_or,
            	                                             blr_missing,
            	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                                             blr_eql,
            	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                                                blr_literal, blr_long, 0, 0,0,0,0,
            	                                       blr_eql,
            	                                          blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_eql,
            	                                       blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                       blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                              blr_end,
            	                           blr_begin,
            	                              blr_begin,
            	                                 blr_begin,
            	                                    blr_if,
            	                                       blr_any,
            	                                          blr_rse, 1,
            	                                             blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 9,
            	                                             blr_boolean,
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 9, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                                      blr_derived_expr, 1, 8,
            	                                                         blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                                   blr_eql,
            	                                                      blr_field, 9, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                                                      blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                                             blr_end,
            	                                       blr_begin,
            	                                          blr_begin,
            	                                             blr_leave, 2,
            	                                             blr_end,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                              blr_end,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_11
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:8
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_or,
            	         blr_eql,
            	            blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	            blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	         blr_or,
            	            blr_eql,
            	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	            blr_eql,
            	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 3,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 2,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 5,
            	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 5, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 5, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 5,
            	               blr_modify, 6, 7,
            	                  blr_begin,
            	                     blr_assignment,
            	                        blr_literal, blr_long, 0, 0,0,0,0,
            	                        blr_field, 7, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 8,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 9,
            	                     blr_boolean,
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 9, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                              blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_eql,
            	                              blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                              blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 9,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_12
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:9
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_not,
            	         blr_any,
            	            blr_rse, 1,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	               blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_if,
            	      blr_not,
            	         blr_any,
            	            blr_rse, 1,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
            	                     blr_or,
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	               blr_end,
            	      blr_leave, 2,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_13
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:a
            	blr_version5,
            	blr_begin,
            	   blr_leave, 1,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_14
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:b
            	blr_version5,
            	blr_begin,
            	   blr_leave, 1,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_15
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:c
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_16
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:d
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	                     blr_eql,
            	                        blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_modify, 3, 5,
            	            blr_begin,
            	               blr_assignment,
            	                  blr_literal, blr_long, 0, 0,0,0,0,
            	                  blr_field, 5, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	               blr_end,
            	         blr_end,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 6,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 6, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 6,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_17
    RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:e
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_18
    RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c0
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_if,
            	         blr_not,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                     blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 0, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
            	                     blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
            	         blr_leave, 1,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_19
    RDB$RELATION_NAME               RDB$INDICES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c1
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_2
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3
            	blr_version5,
            	blr_if,
            	   blr_eql,
            	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	      blr_literal, blr_short, 0, 1,0,
            	   blr_leave, 0,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_20
    RDB$RELATION_NAME               RDB$INDICES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c2
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 0, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
            	                           blr_field, 1, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 0, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
            	                              blr_field, 1, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
            	                           blr_eql,
            	                              blr_field, 0, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	                              blr_field, 1, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	            blr_leave, 1,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 5,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 6,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 6, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	                        blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                           blr_literal, blr_long, 0, 1,0,0,0,
            	                        blr_or,
            	                           blr_eql,
            	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                              blr_literal, blr_long, 0, 0,0,0,0,
            	                           blr_missing,
            	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	         blr_end,
            	      blr_begin,
            	         blr_leave, 2,
            	         blr_end,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_or,
            	                     blr_eql,
            	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                     blr_or,
            	                        blr_eql,
            	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	                        blr_eql,
            	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                  blr_literal, blr_long, 0, 1,0,0,0,
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                     blr_literal, blr_long, 0, 0,0,0,0,
            	                  blr_missing,
            	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	            blr_begin,
            	               blr_if,
            	                  blr_eql,
            	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_leave, 2,
            	                  blr_leave, 3,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_21
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c3
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	         blr_end,
            	      blr_begin,
            	         blr_leave, 1,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_22
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c4
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_and,
            	                     blr_and,
            	                        blr_and,
            	                           blr_and,
            	                              blr_and,
            	                                 blr_and,
            	                                    blr_and,
            	                                       blr_equiv,
            	                                          blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                          blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                       blr_equiv,
            	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_equiv,
            	                                       blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
            	                                       blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
            	                                 blr_equiv,
            	                                    blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
            	                                    blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
            	                              blr_equiv,
            	                                 blr_field, 0, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
            	                                 blr_field, 1, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
            	                           blr_equiv,
            	                              blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
            	                              blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
            	                        blr_equiv,
            	                           blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	                           blr_field, 1, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	                     blr_equiv,
            	                        blr_field, 0, 9, 'R','D','B','$','F','L','A','G','S',
            	                        blr_field, 1, 9, 'R','D','B','$','F','L','A','G','S',
            	                  blr_equiv,
            	                     blr_field, 0, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
            	                     blr_field, 1, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
            	            blr_begin,
            	               blr_leave, 1,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_23
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c5
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 6,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_neq,
            	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 1,
            	            blr_erase, 4,
            	         blr_end,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
            	         blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 9,
            	         blr_boolean,
            	            blr_and,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
            	                              blr_literal, blr_long, 0, 2,0,0,0,
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
            	                                 blr_literal, blr_long, 0, 0,0,0,0,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
            	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_eql,
            	                                    blr_field, 9, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_eql,
            	                  blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 10,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
            	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
            	                              blr_literal, blr_long, 0, 2,0,0,0,
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
            	                                 blr_literal, blr_long, 0, 0,0,0,0,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
            	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_neq,
            	                                    blr_field, 10, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 1,
            	            blr_erase, 7,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_24
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c6
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 3,
            	            blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	            blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_neq,
            	               blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_leave, 1,
            	            blr_end,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                     blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	            blr_leave, 2,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_25
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:6
            	blr_version5,
            	blr_begin,
            	   blr_leave, 1,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_26
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:5
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_not,
            	                     blr_missing,
            	                        blr_field, 3, 15, 'R','D','B','$','V','I','E','W','_','S','O','U','R','C','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_if,
            	      blr_not,
            	         blr_or,
            	            blr_eql,
            	               blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	            blr_or,
            	               blr_eql,
            	                  blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_or,
            	                     blr_eql,
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	                     blr_eql,
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	      blr_leave, 2,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                        blr_eql,
            	                           blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 3,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_27
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c7
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 4,
            	         blr_erase, 3,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_3
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:4
            	blr_version5,
            	blr_if,
            	   blr_eql,
            	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	      blr_literal, blr_short, 0, 1,0,
            	   blr_leave, 0,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_31
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c8
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_and,
            	               blr_neq,
            	                  blr_user_name,
            	                  blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	               blr_and,
            	                  blr_neq,
            	                     blr_current_role,
            	                     blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                  blr_not,
            	                     blr_any,
            	                        blr_rse, 1,
            	                           blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 4,
            	                           blr_boolean,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                 blr_eql,
            	                                    blr_field, 4, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                    blr_user_name,
            	                           blr_end,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_32
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c9
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_and,
            	               blr_neq,
            	                  blr_user_name,
            	                  blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	               blr_and,
            	                  blr_neq,
            	                     blr_current_role,
            	                     blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                  blr_not,
            	                     blr_any,
            	                        blr_rse, 1,
            	                           blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 4,
            	                           blr_boolean,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                 blr_eql,
            	                                    blr_field, 4, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                    blr_user_name,
            	                           blr_end,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_33
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3ca
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_and,
            	               blr_neq,
            	                  blr_user_name,
            	                  blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	               blr_and,
            	                  blr_neq,
            	                     blr_current_role,
            	                     blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                  blr_not,
            	                     blr_any,
            	                        blr_rse, 1,
            	                           blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 4,
            	                           blr_boolean,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                 blr_eql,
            	                                    blr_field, 4, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                    blr_user_name,
            	                           blr_end,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_34
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3cb
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 3,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       2
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_35
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3cc
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_eql,
            	                     blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                     blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 3,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       2
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_36
    RDB$RELATION_NAME               RDB$FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3cd
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_not,
            	         blr_and,
            	            blr_eql,
            	               blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
            	               blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
            	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                  blr_eql,
            	                     blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
            	                     blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
            	      blr_if,
            	         blr_any,
            	            blr_rse, 4,
            	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	               blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                              blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                 blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                              blr_eql,
            	                                 blr_field, 6, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                                 blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_end,
            	         blr_leave, 1,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_8
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:1
            	blr_version5,
            	blr_if,
            	   blr_not,
            	      blr_missing,
            	         blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	   blr_for,
            	      blr_rse, 1,
            	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_starting,
            	               blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	               blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	            blr_begin,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 20, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S','E','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_modify, 3, 5,
            	                  blr_begin,
            	                     blr_assignment,
            	                        blr_null,
            	                        blr_field, 5, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_end,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:2
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_missing,
            	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	      blr_assignment,
            	         blr_user_name,
            	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	         blr_literal, blr_long, 0, 0,0,0,0,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 6,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                           blr_user_name,
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                              blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                           blr_eql,
            	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                              blr_field, 1, 8, 'R','D','B','$','U','S','E','R',
            	                     blr_or,
            	                        blr_eql,
            	                           blr_user_name,
            	                           blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                        blr_or,
            	                           blr_eql,
            	                              blr_current_role,
            	                              blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                           blr_any,
            	                              blr_rse, 1,
            	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 28,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 28, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                       blr_eql,
            	                                          blr_field, 28, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                          blr_user_name,
            	                                 blr_end,
            	                  blr_begin,
            	                     blr_end,
            	                  blr_if,
            	                     blr_neq,
            	                        blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                     blr_begin,
            	                        blr_if,
            	                           blr_not,
            	                              blr_any,
            	                                 blr_rse, 1,
            	                                    blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 7,
            	                                    blr_boolean,
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                             blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_and,
            	                                             blr_eql,
            	                                                blr_field, 7, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                blr_literal, blr_long, 0, 0,0,0,0,
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 7, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                   blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 7, 8, 'R','D','B','$','U','S','E','R',
            	                                                      blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                                                   blr_and,
            	                                                      blr_eql,
            	                                                         blr_field, 7, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                         blr_literal, blr_long, 0, 8,0,0,0,
            	                                                      blr_and,
            	                                                         blr_neq,
            	                                                            blr_field, 7, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                            blr_literal, blr_long, 0, 0,0,0,0,
            	                                                         blr_or,
            	                                                            blr_missing,
            	                                                               blr_field, 7, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                            blr_eql,
            	                                                               blr_field, 7, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                               blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_end,
            	                           blr_leave, 2,
            	                           blr_end,
            	                        blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_missing,
            	                              blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_begin,
            	                           blr_for,
            	                              blr_rse, 3,
            	                                 blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 8,
            	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 9,
            	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 10,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                          blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                             blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_and,
            	                                             blr_not,
            	                                                blr_missing,
            	                                                   blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 9, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
            	                                                   blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 9, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
            	                                                      blr_field, 8, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
            	                                                   blr_eql,
            	                                                      blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                      blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_end,
            	                              blr_begin,
            	                                 blr_if,
            	                                    blr_and,
            	                                       blr_neq,
            	                                          blr_field, 10, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                       blr_and,
            	                                          blr_neq,
            	                                             blr_user_name,
            	                                             blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                                          blr_and,
            	                                             blr_neq,
            	                                                blr_current_role,
            	                                                blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                                             blr_not,
            	                                                blr_any,
            	                                                   blr_rse, 1,
            	                                                      blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 29,
            	                                                      blr_boolean,
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 29, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                               blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                                            blr_eql,
            	                                                               blr_field, 29, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                               blr_user_name,
            	                                                      blr_end,
            	                                    blr_begin,
            	                                       blr_if,
            	                                          blr_not,
            	                                             blr_any,
            	                                                blr_rse, 1,
            	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 11,
            	                                                   blr_boolean,
            	                                                      blr_and,
            	                                                         blr_eql,
            	                                                            blr_field, 11, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                            blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 11, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                               blr_literal, blr_long, 0, 0,0,0,0,
            	                                                            blr_and,
            	                                                               blr_eql,
            	                                                                  blr_field, 11, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                               blr_and,
            	                                                                  blr_eql,
            	                                                                     blr_field, 11, 8, 'R','D','B','$','U','S','E','R',
            	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                                  blr_and,
            	                                                                     blr_eql,
            	                                                                        blr_field, 11, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
            	                                                                     blr_and,
            	                                                                        blr_neq,
            	                                                                           blr_field, 11, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
            	                                                                        blr_or,
            	                                                                           blr_missing,
            	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                                           blr_eql,
            	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                                              blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
            	                                                   blr_end,
            	                                          blr_leave, 5,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                           blr_end,
            	                        blr_begin,
            	                           blr_for,
            	                              blr_rse, 2,
            	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 12,
            	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 13,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 12, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
            	                                          blr_field, 1, 17, 'R','D','B','$',
            	'R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                       blr_eql,
            	                                          blr_field, 12, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_end,
            	                              blr_begin,
            	                                 blr_if,
            	                                    blr_and,
            	                                       blr_neq,
            	                                          blr_field, 13, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                       blr_and,
            	                                          blr_neq,
            	                                             blr_user_name,
            	                                             blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                                          blr_and,
            	                                             blr_neq,
            	                                                blr_current_role,
            	                                                blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                                             blr_not,
            	                                                blr_any,
            	                                                   blr_rse, 1,
            	                                                      blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 30,
            	                                                      blr_boolean,
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 30, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                               blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                                            blr_eql,
            	                                                               blr_field, 30, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                               blr_user_name,
            	                                                      blr_end,
            	                                    blr_begin,
            	                                       blr_if,
            	                                          blr_not,
            	                                             blr_any,
            	                                                blr_rse, 1,
            	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 14,
            	                                                   blr_boolean,
            	                                                      blr_and,
            	                                                         blr_eql,
            	                                                            blr_field, 14, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                            blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 14, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                               blr_literal, blr_long, 0, 0,0,0,0,
            	                                                            blr_and,
            	                                                               blr_eql,
            	                                                                  blr_field, 14, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                               blr_and,
            	                                                                  blr_eql,
            	                                                                     blr_field, 14, 8, 'R','D','B','$','U','S','E','R',
            	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                                  blr_and,
            	                                                                     blr_eql,
            	                                                                        blr_field, 14, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
            	                                                                     blr_and,
            	                                                                        blr_neq,
            	                                                                           blr_field, 14, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
            	                                                                        blr_missing,
            	                                                                           blr_field, 14, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                   blr_end,
            	                                          blr_leave, 5,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                           blr_end,
            	               blr_if,
            	                  blr_missing,
            	                     blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                  blr_modify, 6, 15,
            	                     blr_begin,
            	                        blr_assignment,
            	                           blr_cast, blr_varying2, 3,0, 31,0,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                           blr_field, 15, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_end,
            	                  blr_if,
            	                     blr_not,
            	                        blr_starting,
            	                           blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_cast, blr_varying2, 3,0, 31,0,
            	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                     blr_leave, 3,
            	                     blr_end,
            	               blr_end,
            	         blr_if,
            	            blr_not,
            	               blr_missing,
            	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_for,
            	               blr_rse, 1,
            	                  blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 16,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 16, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 16, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	               blr_begin,
            	                  blr_if,
            	                     blr_missing,
            	                        blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_modify, 16, 17,
            	                        blr_begin,
            	                           blr_assignment,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                              blr_field, 17, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_starting,
            	                              blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	                        blr_leave, 4,
            	                        blr_end,
            	                  blr_end,
            	            blr_end,
            	         blr_end,
            	      blr_if,
            	         blr_eql,
            	            blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	            blr_literal, blr_long, 0, 5,0,0,0,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 14, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S', 18,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_missing,
            	                        blr_field, 18, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 18, 18, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_and,
            	                     blr_neq,
            	                        blr_field, 18, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                     blr_and,
            	                        blr_neq,
            	                           blr_user_name,
            	                           blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                        blr_and,
            	                           blr_neq,
            	                              blr_current_role,
            	                              blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                           blr_not,
            	                              blr_any,
            	                                 blr_rse, 1,
            	                                    blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 31,
            	                                    blr_boolean,
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 31, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                             blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                          blr_eql,
            	                                             blr_field, 31, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                             blr_user_name,
            	                                    blr_end,
            	                  blr_if,
            	                     blr_not,
            	                        blr_any,
            	                           blr_rse, 1,
            	                              blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 19,
            	                              blr_boolean,
            	                                 blr_and,
            	                                    blr_eql,
            	                                       blr_field, 19, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                       blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 19, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                          blr_literal, blr_long, 0, 5,0,0,0,
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 19, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                             blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                          blr_and,
            	                                             blr_eql,
            	                                                blr_field, 19, 8, 'R','D','B','$','U','S','E','R',
            	                                                blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 19, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                   blr_literal, blr_long, 0, 8,0,0,0,
            	                                                blr_and,
            	                                                   blr_neq,
            	                                                      blr_field, 19, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                      blr_literal, blr_long, 0, 0,0,0,0,
            	                                                   blr_or,
            	                                                      blr_missing,
            	                                                         blr_field, 19, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                      blr_eql,
            	                                                         blr_field, 19, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                         blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                              blr_end,
            	                     blr_leave, 2,
            	                     blr_end,
            	                  blr_end,
            	               blr_if,
            	                  blr_missing,
            	                     blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                  blr_modify, 18, 20,
            	                     blr_begin,
            	                        blr_assignment,
            	                           blr_cast, blr_varying2, 3,0, 31,0,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                           blr_field, 20, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_end,
            	                  blr_if,
            	                     blr_not,
            	                        blr_starting,
            	                           blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_cast, blr_varying2, 3,0, 31,0,
            	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                     b
            	lr_leave, 3,
            	                     blr_end,
            	               blr_end,
            	         blr_if,
            	            blr_eql,
            	               blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	               blr_literal, blr_long, 0, 15,0,0,0,
            	            blr_for,
            	               blr_rse, 1,
            	                  blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 26,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_missing,
            	                           blr_field, 26, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 26, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_end,
            	               blr_begin,
            	                  blr_if,
            	                     blr_and,
            	                        blr_neq,
            	                           blr_field, 26, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                           blr_user_name,
            	                        blr_and,
            	                           blr_neq,
            	                              blr_user_name,
            	                              blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                           blr_and,
            	                              blr_neq,
            	                                 blr_current_role,
            	                                 blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                              blr_not,
            	                                 blr_any,
            	                                    blr_rse, 1,
            	                                       blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 32,
            	                                       blr_boolean,
            	                                          blr_and,
            	                                             blr_eql,
            	                                                blr_field, 32, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                             blr_eql,
            	                                                blr_field, 32, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                blr_user_name,
            	                                       blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_any,
            	                              blr_rse, 1,
            	                                 blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 27,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 27, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 27, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                             blr_literal, blr_long, 0, 5,0,0,0,
            	                                          blr_and,
            	                                             blr_eql,
            	                                                blr_field, 27, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 27, 8, 'R','D','B','$','U','S','E','R',
            	                                                   blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 27, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                      blr_literal, blr_long, 0, 8,0,0,0,
            	                                                   blr_and,
            	                                                      blr_neq,
            	                                                         blr_field, 27, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                         blr_literal, blr_long, 0, 0,0,0,0,
            	                                                      blr_or,
            	                                                         blr_missing,
            	                                                            blr_field, 27, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                         blr_eql,
            	                                                            blr_field, 27, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                            blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                 blr_end,
            	                        blr_leave, 2,
            	                        blr_end,
            	                     blr_end,
            	                  blr_if,
            	                     blr_missing,
            	                        blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_modify, 26, 28,
            	                        blr_begin,
            	                           blr_assignment,
            	                              blr_cast, blr_varying2, 3,0, 31,0,
            	                                 blr_concatenate,
            	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                    blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                       blr_literal, blr_long, 0, 1,0,0,0,
            	                              blr_field, 28, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_starting,
            	                              blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_cast, blr_varying2, 3,0, 31,0,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                        blr_leave, 3,
            	                        blr_end,
            	                  blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                  blr_literal, blr_long, 0, 18,0,0,0,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','P','A','C','K','A','G','E','S', 22,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 22, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_if,
            	                        blr_and,
            	                           blr_neq,
            	                              blr_field, 22, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                              blr_user_name,
            	                           blr_and,
            	                              blr_neq,
            	                                 blr_user_name,
            	                                 blr_literal, blr_text, 6,0, 'S','Y','S','D','B','A',
            	                              blr_and,
            	                                 blr_neq,
            	                                    blr_current_role,
            	                                    blr_literal, blr_text, 9,0, 'R','D','B','$','A','D','M','I','N',
            	                                 blr_not,
            	                                    blr_any,
            	                                       blr_rse, 1,
            	                                          blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 33,
            	                                          blr_boolean,
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 33, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                   blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	                                                blr_eql,
            	                                                   blr_field, 33, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                   blr_user_name,
            	                                          blr_end,
            	                        blr_if,
            	                           blr_not,
            	                              blr_any,
            	                                 blr_rse, 1,
            	                                    blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 24,
            	                                    blr_boolean,
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 24, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                             blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_and,
            	                                             blr_eql,
            	                                                blr_field, 24, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                blr_literal, blr_long, 0, 18,0,0,0,
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 24, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                   blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 24, 8, 'R','D','B','$','U','S','E','R',
            	                                                      blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                                                   blr_and,
            	                                                      blr_eql,
            	                                                         blr_field, 24, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                         blr_literal, blr_long, 0, 8,0,0,0,
            	                                                      blr_and,
            	                                                         blr_neq,
            	                                                            blr_field, 24, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                            blr_literal, blr_long, 0, 0,0,0,0,
            	                                                         blr_or,
            	                                                            blr_missing,
            	                                                               blr_field, 24, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                            blr_eql,
            	                                                               blr_field, 24, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                               blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_end,
            	                           blr_leave, 2,
            	                           blr_end,
            	                        blr_end,
            	                     blr_if,
            	                        blr_missing,
            	                           blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_modify, 22, 23,
            	                           blr_begin,
            	                              blr_assignment,
            	                                 blr_cast, blr_varying2, 3,0, 31,0,
            	                                    blr_concatenate,
            	                                       blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                       blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                          blr_literal, blr_long, 0, 1,0,0,0,
            	                                 blr_field, 23, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_end,
            	                        blr_if,
            	                           blr_not,
            	                              blr_starting,
            	                                 blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                 blr_cast, blr_varying2, 3,0, 31,0,
            	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                           blr_leave, 3,
            	                           blr_end,
            	                     blr_end,
            	               blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>


    Records affected: 29

"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$TRIGGER_NAME                RDB$TRIGGER_1
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:0
            	blr_version5,
            	blr_leave, 0,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_10
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:7
            	blr_version5,
            	blr_begin,
            	   blr_label, 0,
            	      blr_begin,
            	         blr_begin,
            	            blr_if,
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text2, 0,0, 6,0, 'U','N','I','Q','U','E',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_if,
            	                        blr_any,
            	                           blr_rse, 1,
            	                              blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 2,
            	                              blr_boolean,
            	                                 blr_eql,
            	                                    blr_field, 2, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
            	                                    blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                              blr_end,
            	                        blr_begin,
            	                           blr_begin,
            	                              blr_leave, 1,
            	                              blr_end,
            	                           blr_end,
            	                        blr_end,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text2, 0,0, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_for,
            	                        blr_rse, 1,
            	                           blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	                           blr_boolean,
            	                              blr_eql,
            	                                 blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                 blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                           blr_end,
            	                        blr_erase, 3,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text2, 0,0, 8,0, 'N','O','T',32,'N','U','L','L',
            	               blr_begin,
            	                  blr_begin,
            	                     blr_label, 1,
            	                        blr_for,
            	                           blr_rse, 5,
            	                              blr_relation2, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S',
            	                                 9, 'C','1',32,'C','H','K','C','O','N', 4,
            	                              blr_relation2, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S',
            	                                 6, 'C','1',32,'R','F','L', 5,
            	                              blr_relation2, 10, 'R','D','B','$','F','I','E','L','D','S',
            	                                 6, 'C','1',32,'F','L','D', 6,
            	                              blr_relation2, 11, 'R','D','B','$','I','N','D','I','C','E','S',
            	                                 6, 'C','1',32,'I','N','D', 7,
            	                              blr_relation2, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S',
            	                                 9, 'C','1',32,'I','D','X','S','E','G', 8,
            	                              blr_boolean,
            	                                 blr_and,
            	                                    blr_and,
            	                                       blr_and,
            	                                          blr_and,
            	                                             blr_and,
            	                                                blr_and,
            	                                                   blr_and,
            	                                                      blr_eql,
            	                                                         blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                                         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                                                      blr_eql,
            	                                                         blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                         blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                                   blr_eql,
            	                                                      blr_field, 5, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                      blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                blr_eql,
            	                                                   blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                   blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                             blr_eql,
            	                                                blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                blr_field, 5, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                                          blr_or,
            	                                             blr_missing,
            	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                                             blr_eql,
            	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                                                blr_literal, blr_long, 0, 0,0,0,0,
            	                                       blr_eql,
            	                                          blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_eql,
            	                                       blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                       blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                              blr_end,
            	                           blr_begin,
            	                              blr_begin,
            	                                 blr_begin,
            	                                    blr_if,
            	                                       blr_any,
            	                                          blr_rse, 1,
            	                                             blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 9,
            	                                             blr_boolean,
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 9, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                                      blr_derived_expr, 1, 8,
            	                                                         blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                                                   blr_eql,
            	                                                      blr_field, 9, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                                                      blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                                             blr_end,
            	                                       blr_begin,
            	                                          blr_begin,
            	                                             blr_leave, 2,
            	                                             blr_end,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                              blr_end,
            	                     blr_end,
            	                  blr_end,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_11
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:8
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_or,
            	         blr_eql,
            	            blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	            blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	         blr_or,
            	            blr_eql,
            	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	            blr_eql,
            	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 3,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 2,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 5,
            	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 5, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 5, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 5,
            	               blr_modify, 6, 7,
            	                  blr_begin,
            	                     blr_assignment,
            	                        blr_literal, blr_long, 0, 0,0,0,0,
            	                        blr_field, 7, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 8,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 9,
            	                     blr_boolean,
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 9, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                              blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_eql,
            	                              blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                              blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 9,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_12
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:9
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_not,
            	         blr_any,
            	            blr_rse, 1,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	               blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_if,
            	      blr_not,
            	         blr_any,
            	            blr_rse, 1,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
            	                     blr_or,
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	               blr_end,
            	      blr_leave, 2,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_13
    RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:a
            	blr_version5,
            	blr_begin,
            	   blr_leave, 1,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_14
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:b
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_and,
            	                  blr_not,
            	                     blr_eql,
            	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	                  blr_eql,
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_15
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:c
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_16
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:d
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	                     blr_eql,
            	                        blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_modify, 3, 5,
            	            blr_begin,
            	               blr_assignment,
            	                  blr_literal, blr_long, 0, 0,0,0,0,
            	                  blr_field, 5, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
            	               blr_end,
            	         blr_end,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 6,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 6, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 6,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_17
    RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:e
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_18
    RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:f
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_if,
            	         blr_not,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                     blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 0, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
            	                     blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
            	         blr_leave, 1,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_19
    RDB$RELATION_NAME               RDB$INDICES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:10
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_2
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3
            	blr_version5,
            	blr_if,
            	   blr_eql,
            	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	      blr_literal, blr_short, 0, 1,0,
            	   blr_leave, 0,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_20
    RDB$RELATION_NAME               RDB$INDICES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:11
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	            blr_boolean,
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 0, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
            	                           blr_field, 1, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 0, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
            	                              blr_field, 1, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
            	                           blr_eql,
            	                              blr_field, 0, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	                              blr_field, 1, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	            blr_leave, 1,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 5,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 6,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 6, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
            	                        blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                           blr_literal, blr_long, 0, 1,0,0,0,
            	                        blr_or,
            	                           blr_eql,
            	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                              blr_literal, blr_long, 0, 0,0,0,0,
            	                           blr_missing,
            	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	         blr_end,
            	      blr_begin,
            	         blr_leave, 2,
            	         blr_end,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_or,
            	                     blr_eql,
            	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                     blr_or,
            	                        blr_eql,
            	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	                        blr_eql,
            	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                  blr_literal, blr_long, 0, 1,0,0,0,
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	                     blr_literal, blr_long, 0, 0,0,0,0,
            	                  blr_missing,
            	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
            	            blr_begin,
            	               blr_if,
            	                  blr_eql,
            	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_leave, 2,
            	                  blr_leave, 3,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_21
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:12
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	         blr_end,
            	      blr_begin,
            	         blr_leave, 1,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_22
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c0
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_and,
            	                     blr_and,
            	                        blr_and,
            	                           blr_and,
            	                              blr_and,
            	                                 blr_and,
            	                                    blr_and,
            	                                       blr_equiv,
            	                                          blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                          blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                                       blr_equiv,
            	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                    blr_equiv,
            	                                       blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
            	                                       blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
            	                                 blr_equiv,
            	                                    blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
            	                                    blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
            	                              blr_equiv,
            	                                 blr_field, 0, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
            	                                 blr_field, 1, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
            	                           blr_equiv,
            	                              blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
            	                              blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
            	                        blr_equiv,
            	                           blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	                           blr_field, 1, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	                     blr_equiv,
            	                        blr_field, 0, 9, 'R','D','B','$','F','L','A','G','S',
            	                        blr_field, 1, 9, 'R','D','B','$','F','L','A','G','S',
            	                  blr_equiv,
            	                     blr_field, 0, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
            	                     blr_field, 1, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
            	            blr_begin,
            	               blr_leave, 1,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_23
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c1
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 6,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_neq,
            	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 1,
            	            blr_erase, 4,
            	         blr_end,
            	   blr_for,
            	      blr_rse, 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
            	         blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 9,
            	         blr_boolean,
            	            blr_and,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
            	                              blr_literal, blr_long, 0, 2,0,0,0,
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
            	                                 blr_literal, blr_long, 0, 0,0,0,0,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
            	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_eql,
            	                                    blr_field, 9, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_eql,
            	                  blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 10,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
            	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
            	                              blr_literal, blr_long, 0, 2,0,0,0,
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
            	                                 blr_literal, blr_long, 0, 0,0,0,0,
            	                              blr_and,
            	                                 blr_eql,
            	                                    blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
            	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_neq,
            	                                    blr_field, 10, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 1,
            	            blr_erase, 7,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_24
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c2
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 3,
            	            blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	            blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_neq,
            	               blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_leave, 1,
            	            blr_end,
            	         blr_if,
            	            blr_not,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                     blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	            blr_leave, 2,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_25
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:6
            	blr_version5,
            	blr_begin,
            	   blr_leave, 1,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_26
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:5
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_any,
            	         blr_rse, 1,
            	            blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	            blr_boolean,
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_not,
            	                     blr_missing,
            	                        blr_field, 3, 15, 'R','D','B','$','V','I','E','W','_','S','O','U','R','C','E',
            	            blr_end,
            	      blr_leave, 1,
            	      blr_end,
            	   blr_if,
            	      blr_not,
            	         blr_or,
            	            blr_eql,
            	               blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
            	            blr_or,
            	               blr_eql,
            	                  blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                  blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	               blr_or,
            	                  blr_eql,
            	                     blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_or,
            	                     blr_eql,
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	                     blr_eql,
            	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
            	      blr_leave, 2,
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	      blr_begin,
            	         blr_if,
            	            blr_any,
            	               blr_rse, 1,
            	                  blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
            	                        blr_eql,
            	                           blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_end,
            	            blr_leave, 3,
            	            blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_27
    RDB$RELATION_NAME               RDB$RELATION_FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c3
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 4,
            	         blr_erase, 3,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_3
    RDB$RELATION_NAME               RDB$TRIGGERS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:4
            	blr_version5,
            	blr_if,
            	   blr_eql,
            	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
            	      blr_literal, blr_short, 0, 1,0,
            	   blr_leave, 0,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_31
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c4
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_neq,
            	               blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                  blr_literal, blr_short, 0, 20,0,
            	               blr_literal, blr_bool, 1,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_32
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c5
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_neq,
            	               blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                  blr_literal, blr_short, 0, 20,0,
            	               blr_literal, blr_bool, 1,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_33
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c6
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_and,
            	         blr_not,
            	            blr_missing,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	         blr_and,
            	            blr_neq,
            	               blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	               blr_user_name,
            	            blr_neq,
            	               blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                  blr_literal, blr_short, 0, 20,0,
            	               blr_literal, blr_bool, 1,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_missing,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                     blr_neq,
            	                        blr_field, 3, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                  blr_leave, 0,
            	                  blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_34
    RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c7
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	         blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_erase, 3,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                           blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_end,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       2
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_35
    RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                6
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c8
            	blr_version5,
            	blr_begin,
            	   blr_for,
            	      blr_rse, 2,
            	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 3,
            	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
            	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
            	                  blr_eql,
            	                     blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	                     blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_erase, 3,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       2
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_36
    RDB$RELATION_NAME               RDB$FIELDS
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                3
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:3c9
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_not,
            	         blr_and,
            	            blr_eql,
            	               blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
            	               blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
            	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
            	               blr_and,
            	                  blr_eql,
            	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
            	                  blr_eql,
            	                     blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
            	                     blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
            	      blr_if,
            	         blr_any,
            	            blr_rse, 4,
            	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
            	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
            	               blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
            	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_eql,
            	                        blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                              blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
            	                           blr_and,
            	                              blr_eql,
            	                                 blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                 blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                              blr_eql,
            	                                 blr_field, 6, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
            	                                 blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	               blr_end,
            	         blr_leave, 1,
            	         blr_end,
            	      blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_8
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                5
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:1
            	blr_version5,
            	blr_if,
            	   blr_not,
            	      blr_missing,
            	         blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	   blr_for,
            	      blr_rse, 1,
            	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
            	         blr_boolean,
            	            blr_and,
            	               blr_eql,
            	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_eql,
            	                  blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	         blr_end,
            	      blr_begin,
            	         blr_if,
            	            blr_starting,
            	               blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	               blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	            blr_begin,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 20, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S','E','S', 4,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 4, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_erase, 4,
            	                     blr_end,
            	               blr_modify, 3, 5,
            	                  blr_begin,
            	                     blr_assignment,
            	                        blr_null,
            	                        blr_field, 5, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_end,
            	               blr_end,
            	            blr_end,
            	         blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>

    RDB$TRIGGER_NAME                RDB$TRIGGER_9
    RDB$RELATION_NAME               RDB$USER_PRIVILEGES
    RDB$TRIGGER_SEQUENCE            0
    RDB$TRIGGER_TYPE                1
    RDB$TRIGGER_SOURCE              <null>
    RDB$TRIGGER_BLR                 c:2
            	blr_version5,
            	blr_begin,
            	   blr_if,
            	      blr_missing,
            	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	      blr_assignment,
            	         blr_user_name,
            	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	      blr_end,
            	   blr_if,
            	      blr_eql,
            	         blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	         blr_literal, blr_long, 0, 0,0,0,0,
            	      blr_begin,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 6,
            	               blr_boolean,
            	                  blr_eql,
            	                     blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_or,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                           blr_user_name,
            	                        blr_and,
            	                           blr_eql,
            	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                              blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
            	                           blr_eql,
            	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                              blr_field, 1, 8, 'R','D','B','$','U','S','E','R',
            	                     blr_eql,
            	                        blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                           blr_literal, blr_short, 0, 21,0,
            	                        blr_literal, blr_bool, 1,
            	                  blr_begin,
            	                     blr_end,
            	                  blr_if,
            	                     blr_neq,
            	                        blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                        blr_user_name,
            	                     blr_begin,
            	                        blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_missing,
            	                              blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                        blr_begin,
            	                           blr_for,
            	                              blr_rse, 3,
            	                                 blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 8,
            	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 9,
            	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 10,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                          blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                       blr_and,
            	                                          blr_eql,
            	                                             blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                             blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_and,
            	                                             blr_not,
            	                                                blr_missing,
            	                                                   blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
            	                                             blr_and,
            	                                                blr_eql,
            	                                                   blr_field, 9, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
            	                                                   blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                blr_and,
            	                                                   blr_eql,
            	                                                      blr_field, 9, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
            	                                                      blr_field, 8, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
            	                                                   blr_eql,
            	                                                      blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                      blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_end,
            	                              blr_begin,
            	                                 blr_if,
            	                                    blr_and,
            	                                       blr_neq,
            	                                          blr_field, 10, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                       blr_neq,
            	                                          blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                                             blr_literal, blr_short, 0, 21,0,
            	                                          blr_literal, blr_bool, 1,
            	                                    blr_begin,
            	                                       blr_if,
            	                                          blr_not,
            	                                             blr_any,
            	                                                blr_rse, 1,
            	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 11,
            	                                                   blr_boolean,
            	                                                      blr_and,
            	                                                         blr_eql,
            	                                                            blr_field, 11, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                            blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 11, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                               blr_literal, blr_long, 0, 0,0,0,0,
            	                                                            blr_and,
            	                                                               blr_eql,
            	                                                                  blr_field, 11, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                               blr_and,
            	                                                                  blr_eql,
            	                                                                     blr_field, 11, 8, 'R','D','B','$','U','S','E','R',
            	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                                  blr_and,
            	                                                                     blr_eql,
            	                                                                        blr_field, 11, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
            	                                                                     blr_and,
            	                                                                        blr_neq,
            	                                                                           blr_field, 11, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
            	                                                                        blr_or,
            	                                                                           blr_missing,
            	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                                           blr_eql,
            	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                                              blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
            	                                                   blr_end,
            	                                          blr_leave, 5,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                           blr_end,
            	                        blr_begin,
            	                           blr_for,
            	                              blr_rse, 2,
            	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 12,
            	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 13,
            	                                 blr_boolean,
            	                                    blr_and,
            	                                       blr_eql,
            	                                          blr_field, 12, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
            	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                       blr_eql,
            	                                          blr_field, 12, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                          blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                 blr_end,
            	                              blr_begin,
            	                                 blr_if,
            	                                    blr_and,
            	                                       blr_neq,
            	                                          blr_field, 13, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                       blr_neq,
            	                                          blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
            	                                             blr_literal, blr_short, 0, 21,0,
            	                                          blr_literal, blr_bool, 1,
            	                                    blr_begin,
            	                                       blr_if,
            	                                          blr_not,
            	                                             blr_any,
            	                                                blr_rse, 1,
            	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 14,
            	                                                   blr_boolean,
            	                                                      blr_and,
            	                                                         blr_eql,
            	                                                            blr_field, 14, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                            blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                                                         blr_and,
            	                                                            blr_eql,
            	                                                               blr_field, 14, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                                                               blr_literal, blr_long, 0, 0,0,0,0,
            	                                                            blr_and,
            	                                                               blr_eql,
            	                                                                  blr_field, 14, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
            	                                                               blr_and,
            	                                                                  blr_eql,
            	                                                                     blr_field, 14, 8, 'R','D','B','$','U','S','E','R',
            	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
            	                                                                  blr_and,
            	                                                                     blr_eql,
            	                                                                        blr_field, 14, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
            	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
            	                                                                     blr_and,
            	                                                                        blr_neq,
            	                                                                           blr_field, 14, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
            	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
            	                                                                        blr_missing,
            	                                                                           blr_field, 14, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                                                   blr_end,
            	                                          blr_leave, 5,
            	                                          blr_end,
            	                                       blr_end,
            	                                    blr_end,
            	                                 blr_end,
            	                           blr_end,
            	               blr_if,
            	                  blr_missing,
            	                     blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                  blr_modify, 6, 15,
            	                     blr_begin,
            	                        blr_assignment,
            	                           blr_cast, blr_varying2, 4,0, 252,0,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                           blr_field, 15, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_end,

            	           blr_if,
            	                     blr_not,
            	                        blr_starting,
            	                           blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_cast, blr_varying2, 4,0, 252,0,
            	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                     blr_leave, 3,
            	                     blr_end,
            	               blr_end,
            	         blr_if,
            	            blr_not,
            	               blr_missing,
            	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	            blr_for,
            	               blr_rse, 1,
            	                  blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 16,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_eql,
            	                           blr_field, 16, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 16, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                           blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
            	                  blr_end,
            	               blr_begin,
            	                  blr_if,
            	                     blr_missing,
            	                        blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_modify, 16, 17,
            	                        blr_begin,
            	                           blr_assignment,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                              blr_field, 17, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_starting,
            	                              blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
            	                        blr_leave, 4,
            	                        blr_end,
            	                  blr_end,
            	            blr_end,
            	         blr_end,
            	      blr_if,
            	         blr_eql,
            	            blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	            blr_literal, blr_long, 0, 5,0,0,0,
            	         blr_for,
            	            blr_rse, 1,
            	               blr_relation, 14, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S', 18,
            	               blr_boolean,
            	                  blr_and,
            	                     blr_missing,
            	                        blr_field, 18, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                     blr_eql,
            	                        blr_field, 18, 18, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','N','A','M','E',
            	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	               blr_end,
            	            blr_begin,
            	               blr_if,
            	                  blr_missing,
            	                     blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                  blr_modify, 18, 20,
            	                     blr_begin,
            	                        blr_assignment,
            	                           blr_cast, blr_varying2, 4,0, 252,0,
            	                              blr_concatenate,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                    blr_literal, blr_long, 0, 1,0,0,0,
            	                           blr_field, 20, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_end,
            	                  blr_if,
            	                     blr_not,
            	                        blr_starting,
            	                           blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_cast, blr_varying2, 4,0, 252,0,
            	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                     blr_leave, 3,
            	                     blr_end,
            	               blr_end,
            	         blr_if,
            	            blr_eql,
            	               blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	               blr_literal, blr_long, 0, 15,0,0,0,
            	            blr_for,
            	               blr_rse, 1,
            	                  blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 26,
            	                  blr_boolean,
            	                     blr_and,
            	                        blr_missing,
            	                           blr_field, 26, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                        blr_eql,
            	                           blr_field, 26, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                  blr_end,
            	               blr_begin,
            	                  blr_if,
            	                     blr_missing,
            	                        blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                     blr_modify, 26, 28,
            	                        blr_begin,
            	                           blr_assignment,
            	                              blr_cast, blr_varying2, 4,0, 252,0,
            	                                 blr_concatenate,
            	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                    blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                       blr_literal, blr_long, 0, 1,0,0,0,
            	                              blr_field, 28, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                           blr_end,
            	                     blr_if,
            	                        blr_not,
            	                           blr_starting,
            	                              blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_cast, blr_varying2, 4,0, 252,0,
            	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                        blr_leave, 3,
            	                        blr_end,
            	                  blr_end,
            	            blr_if,
            	               blr_eql,
            	                  blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
            	                  blr_literal, blr_long, 0, 18,0,0,0,
            	               blr_for,
            	                  blr_rse, 1,
            	                     blr_relation, 12, 'R','D','B','$','P','A','C','K','A','G','E','S', 22,
            	                     blr_boolean,
            	                        blr_eql,
            	                           blr_field, 22, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
            	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
            	                     blr_end,
            	                  blr_begin,
            	                     blr_if,
            	                        blr_missing,
            	                           blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                        blr_modify, 22, 23,
            	                           blr_begin,
            	                              blr_assignment,
            	                                 blr_cast, blr_varying2, 4,0, 252,0,
            	                                    blr_concatenate,
            	                                       blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                                       blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                          blr_literal, blr_long, 0, 1,0,0,0,
            	                                 blr_field, 23, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                              blr_end,
            	                        blr_if,
            	                           blr_not,
            	                              blr_starting,
            	                                 blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
            	                                 blr_cast, blr_varying2, 4,0, 252,0,
            	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
            	                           blr_leave, 3,
            	                           blr_end,
            	                     blr_end,
            	               blr_end,
            	   blr_end,
            	blr_eoc

    RDB$DESCRIPTION                 <null>
    RDB$TRIGGER_INACTIVE            <null>
    RDB$SYSTEM_FLAG                 1
    RDB$FLAGS                       0
    RDB$VALID_BLR                   <null>
    RDB$DEBUG_INFO                  <null>
    RDB$ENGINE_NAME                 <null>
    RDB$ENTRYPOINT                  <null>
    RDB$SQL_SECURITY                <null>


    Records affected: 29
"""

@pytest.mark.version('>=4.0,<5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

expected_stdout_3 = """

RDB$TRIGGER_NAME                RDB$TRIGGER_1
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:0
        	blr_version5,
        	blr_leave, 0,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_10
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:7
        	blr_version5,
        	blr_begin,
        	   blr_label, 0,
        	      blr_begin,
        	         blr_begin,
        	            blr_if,
        	               blr_or,
        	                  blr_eql,
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	                  blr_eql,
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text2, 0,0, 6,0, 'U','N','I','Q','U','E',
        	               blr_begin,
        	                  blr_begin,
        	                     blr_if,
        	                        blr_any,
        	                           blr_rse, 1,
        	                              blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 2,
        	                              blr_boolean,
        	                                 blr_eql,
        	                                    blr_field, 2, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
        	                                    blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                              blr_end,
        	                        blr_begin,
        	                           blr_begin,
        	                              blr_leave, 1,
        	                              blr_end,
        	                           blr_end,
        	                        blr_end,
        	                     blr_end,
        	                  blr_end,
        	               blr_end,
        	            blr_if,
        	               blr_eql,
        	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                  blr_literal, blr_text2, 0,0, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	               blr_begin,
        	                  blr_begin,
        	                     blr_for,
        	                        blr_rse, 1,
        	                           blr_relation, 19, 'R','D','B','$','R','E','F','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	                           blr_boolean,
        	                              blr_eql,
        	                                 blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                                 blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                           blr_end,
        	                        blr_erase, 3,
        	                     blr_end,
        	                  blr_end,
        	               blr_end,
        	            blr_if,
        	               blr_eql,
        	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                  blr_literal, blr_text2, 0,0, 8,0, 'N','O','T',32,'N','U','L','L',
        	               blr_begin,
        	                  blr_begin,
        	                     blr_label, 1,
        	                        blr_for,
        	                           blr_rse, 5,
        	                              blr_relation2, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S',
        	                                 9, 'C','1',32,'C','H','K','C','O','N', 4,
        	                              blr_relation2, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S',
        	                                 6, 'C','1',32,'R','F','L', 5,
        	                              blr_relation2, 10, 'R','D','B','$','F','I','E','L','D','S',
        	                                 6, 'C','1',32,'F','L','D', 6,
        	                              blr_relation2, 11, 'R','D','B','$','I','N','D','I','C','E','S',
        	                                 6, 'C','1',32,'I','N','D', 7,
        	                              blr_relation2, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S',
        	                                 9, 'C','1',32,'I','D','X','S','E','G', 8,
        	                              blr_boolean,
        	                                 blr_and,
        	                                    blr_and,
        	                                       blr_and,
        	                                          blr_and,
        	                                             blr_and,
        	                                                blr_and,
        	                                                   blr_and,
        	                                                      blr_eql,
        	                                                         blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                                                         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                                                      blr_eql,
        	                                                         blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                         blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                                                   blr_eql,
        	                                                      blr_field, 5, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                      blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                blr_eql,
        	                                                   blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                   blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                             blr_eql,
        	                                                blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                blr_field, 5, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
        	                                          blr_or,
        	                                             blr_missing,
        	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
        	                                             blr_eql,
        	                                                blr_field, 6, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
        	                                                blr_literal, blr_long, 0, 0,0,0,0,
        	                                       blr_eql,
        	                                          blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                    blr_eql,
        	                                       blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                                       blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                              blr_end,
        	                           blr_begin,
        	                              blr_begin,
        	                                 blr_begin,
        	                                    blr_if,
        	                                       blr_any,
        	                                          blr_rse, 1,
        	                                             blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 9,
        	                                             blr_boolean,
        	                                                blr_and,
        	                                                   blr_eql,
        	                                                      blr_field, 9, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                                                      blr_derived_expr, 1, 8,
        	                                                         blr_field, 8, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                                                   blr_eql,
        	                                                      blr_field, 9, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                                                      blr_literal, blr_text2, 0,0, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	                                             blr_end,
        	                                       blr_begin,
        	                                          blr_begin,
        	                                             blr_leave, 2,
        	                                             blr_end,
        	                                          blr_end,
        	                                       blr_end,
        	                                    blr_end,
        	                                 blr_end,
        	                              blr_end,
        	                     blr_end,
        	                  blr_end,
        	               blr_end,
        	            blr_end,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_11
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                6
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:8
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_or,
        	         blr_eql,
        	            blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	            blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	         blr_or,
        	            blr_eql,
        	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	               blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	            blr_eql,
        	               blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
        	      blr_begin,
        	         blr_for,
        	            blr_rse, 1,
        	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
        	               blr_boolean,
        	                  blr_eql,
        	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_erase, 3,
        	               blr_for,
        	                  blr_rse, 1,
        	                     blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 4,
        	                     blr_boolean,
        	                        blr_eql,
        	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_end,
        	                  blr_begin,
        	                     blr_erase, 4,
        	                     blr_end,
        	               blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_if,
        	      blr_eql,
        	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	         blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
        	      blr_begin,
        	         blr_for,
        	            blr_rse, 2,
        	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 5,
        	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
        	               blr_boolean,
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                        blr_field, 5, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                           blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_eql,
        	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                           blr_field, 5, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_erase, 5,
        	               blr_modify, 6, 7,
        	                  blr_begin,
        	                     blr_assignment,
        	                        blr_literal, blr_long, 0, 0,0,0,0,
        	                        blr_field, 7, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
        	                     blr_end,
        	               blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_if,
        	      blr_eql,
        	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	         blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	      blr_begin,
        	         blr_for,
        	            blr_rse, 1,
        	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
        	               blr_boolean,
        	                  blr_eql,
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_erase, 8,
        	               blr_for,
        	                  blr_rse, 1,
        	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 9,
        	                     blr_boolean,
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 9, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                              blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                           blr_eql,
        	                              blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                              blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_end,
        	                  blr_begin,
        	                     blr_erase, 9,
        	                     blr_end,
        	               blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_12
RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                1
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:9
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_not,
        	         blr_any,
        	            blr_rse, 1,
        	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	               blr_boolean,
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_eql,
        	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	               blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_if,
        	      blr_not,
        	         blr_any,
        	            blr_rse, 1,
        	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	               blr_boolean,
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                        blr_field, 1, 17, 'R','D','B','$','C','O','N','S','T','_','N','A','M','E','_','U','Q',
        	                     blr_or,
        	                        blr_eql,
        	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	                        blr_eql,
        	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
        	               blr_end,
        	      blr_leave, 2,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_13
RDB$RELATION_NAME               RDB$REF_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:a
        	blr_version5,
        	blr_begin,
        	   blr_leave, 1,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_14
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:b
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_and,
        	                  blr_not,
        	                     blr_eql,
        	                        blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
        	                  blr_eql,
        	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	            blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_15
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:c
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_eql,
        	                  blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	            blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_16
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                6
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:d
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
        	                     blr_eql,
        	                        blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_modify, 3, 5,
        	            blr_begin,
        	               blr_assignment,
        	                  blr_literal, blr_long, 0, 0,0,0,0,
        	                  blr_field, 5, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G',
        	               blr_end,
        	         blr_end,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 6,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_eql,
        	                        blr_field, 6, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                        blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_erase, 6,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_17
RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:e
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_eql,
        	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	            blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_18
RDB$RELATION_NAME               RDB$INDEX_SEGMENTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:f
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_eql,
        	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	            blr_end,
        	      blr_if,
        	         blr_not,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                     blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_eql,
        	                     blr_field, 0, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
        	                     blr_field, 1, 18, 'R','D','B','$','F','I','E','L','D','_','P','O','S','I','T','I','O','N',
        	         blr_leave, 1,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_19
RDB$RELATION_NAME               RDB$INDICES
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:10
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_eql,
        	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	            blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_2
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3
        	blr_version5,
        	blr_if,
        	   blr_eql,
        	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
        	      blr_literal, blr_short, 0, 1,0,
        	   blr_leave, 0,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_20
RDB$RELATION_NAME               RDB$INDICES
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:11
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	            blr_boolean,
        	               blr_eql,
        	                  blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	            blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_not,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_field, 1, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 0, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
        	                           blr_field, 1, 12, 'R','D','B','$','I','N','D','E','X','_','I','D',
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 0, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
        	                              blr_field, 1, 17, 'R','D','B','$','S','E','G','M','E','N','T','_','C','O','U','N','T',
        	                           blr_eql,
        	                              blr_field, 0, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
        	                              blr_field, 1, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
        	            blr_leave, 1,
        	            blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_for,
        	      blr_rse, 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 5,
        	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 6,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 6, 15, 'R','D','B','$','F','O','R','E','I','G','N','_','K','E','Y',
        	                        blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	                           blr_literal, blr_long, 0, 1,0,0,0,
        	                        blr_or,
        	                           blr_eql,
        	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	                              blr_literal, blr_long, 0, 0,0,0,0,
        	                           blr_missing,
        	                              blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	         blr_end,
        	      blr_begin,
        	         blr_leave, 2,
        	         blr_end,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
        	            blr_boolean,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 7, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_field, 0, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_or,
        	                     blr_eql,
        	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	                     blr_or,
        	                        blr_eql,
        	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                           blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
        	                        blr_eql,
        	                           blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                           blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	            blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 1, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	                  blr_literal, blr_long, 0, 1,0,0,0,
        	               blr_or,
        	                  blr_eql,
        	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	                     blr_literal, blr_long, 0, 0,0,0,0,
        	                  blr_missing,
        	                     blr_field, 0, 18, 'R','D','B','$','I','N','D','E','X','_','I','N','A','C','T','I','V','E',
        	            blr_begin,
        	               blr_if,
        	                  blr_eql,
        	                     blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	                  blr_leave, 2,
        	                  blr_leave, 3,
        	               blr_end,
        	            blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_21
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:12
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	         blr_end,
        	      blr_begin,
        	         blr_leave, 1,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_22
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c0
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                  blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	         blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_not,
        	               blr_and,
        	                  blr_and,
        	                     blr_and,
        	                        blr_and,
        	                           blr_and,
        	                              blr_and,
        	                                 blr_and,
        	                                    blr_and,
        	                                       blr_equiv,
        	                                          blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                                          blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                                       blr_equiv,
        	                                          blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                    blr_equiv,
        	                                       blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
        	                                       blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','S','E','Q','U','E','N','C','E',
        	                                 blr_equiv,
        	                                    blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
        	                                    blr_field, 1, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','T','Y','P','E',
        	                              blr_equiv,
        	                                 blr_field, 0, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
        	                                 blr_field, 1, 15, 'R','D','B','$','T','R','I','G','G','E','R','_','B','L','R',
        	                           blr_equiv,
        	                              blr_field, 0, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
        	                              blr_field, 1, 20, 'R','D','B','$','T','R','I','G','G','E','R','_','I','N','A','C','T','I','V','E',
        	                        blr_equiv,
        	                           blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
        	                           blr_field, 1, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
        	                     blr_equiv,
        	                        blr_field, 0, 9, 'R','D','B','$','F','L','A','G','S',
        	                        blr_field, 1, 9, 'R','D','B','$','F','L','A','G','S',
        	                  blr_equiv,
        	                     blr_field, 0, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
        	                     blr_field, 1, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O',
        	            blr_begin,
        	               blr_leave, 1,
        	               blr_end,
        	            blr_end,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_23
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c1
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 3,
        	         blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_eql,
        	                        blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                        blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_any,
        	               blr_rse, 1,
        	                  blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 6,
        	                  blr_boolean,
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 6, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                        blr_neq,
        	                           blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_end,
        	            blr_leave, 1,
        	            blr_erase, 4,
        	         blr_end,
        	   blr_for,
        	      blr_rse, 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 7,
        	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 8,
        	         blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 9,
        	         blr_boolean,
        	            blr_and,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 7, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                           blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 9, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
        	                              blr_literal, blr_long, 0, 2,0,0,0,
        	                           blr_and,
        	                              blr_eql,
        	                                 blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
        	                                 blr_literal, blr_long, 0, 0,0,0,0,
        	                              blr_and,
        	                                 blr_eql,
        	                                    blr_field, 9, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
        	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                 blr_eql,
        	                                    blr_field, 9, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	               blr_eql,
        	                  blr_field, 8, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_field, 7, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_any,
        	               blr_rse, 1,
        	                  blr_relation, 16, 'R','D','B','$','D','E','P','E','N','D','E','N','C','I','E','S', 10,
        	                  blr_boolean,
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','N','A','M','E',
        	                           blr_field, 8, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 10, 18, 'R','D','B','$','D','E','P','E','N','D','E','N','T','_','T','Y','P','E',
        	                              blr_literal, blr_long, 0, 2,0,0,0,
        	                           blr_and,
        	                              blr_eql,
        	                                 blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','T','Y','P','E',
        	                                 blr_literal, blr_long, 0, 0,0,0,0,
        	                              blr_and,
        	                                 blr_eql,
        	                                    blr_field, 10, 20, 'R','D','B','$','D','E','P','E','N','D','E','D','_','O','N','_','N','A','M','E',
        	                                    blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                 blr_neq,
        	                                    blr_field, 10, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                    blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_end,
        	            blr_leave, 1,
        	            blr_erase, 7,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_24
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c2
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 3,
        	            blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
        	            blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	            blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
        	            blr_boolean,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                        blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                        blr_eql,
        	                           blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                           blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	            blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_neq,
        	               blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	               blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	            blr_leave, 1,
        	            blr_end,
        	         blr_if,
        	            blr_not,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
        	                     blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
        	                  blr_eql,
        	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
        	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
        	            blr_leave, 2,
        	            blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_25
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:6
        	blr_version5,
        	blr_begin,
        	   blr_leave, 1,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_26
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                1
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:5
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_any,
        	         blr_rse, 1,
        	            blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 3,
        	            blr_boolean,
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_not,
        	                     blr_missing,
        	                        blr_field, 3, 15, 'R','D','B','$','V','I','E','W','_','S','O','U','R','C','E',
        	            blr_end,
        	      blr_leave, 1,
        	      blr_end,
        	   blr_if,
        	      blr_not,
        	         blr_or,
        	            blr_eql,
        	               blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	               blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
        	            blr_or,
        	               blr_eql,
        	                  blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                  blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	               blr_or,
        	                  blr_eql,
        	                     blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	                  blr_or,
        	                     blr_eql,
        	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
        	                     blr_eql,
        	                        blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 5,0, 'C','H','E','C','K',
        	      blr_leave, 2,
        	      blr_end,
        	   blr_if,
        	      blr_eql,
        	         blr_field, 1, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	         blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	      blr_begin,
        	         blr_if,
        	            blr_any,
        	               blr_rse, 1,
        	                  blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	                  blr_boolean,
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                           blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
        	                        blr_eql,
        	                           blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_end,
        	            blr_leave, 3,
        	            blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_27
RDB$RELATION_NAME               RDB$RELATION_FIELDS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                6
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c3
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_field, 4, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_eql,
        	                        blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                        blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
        	         blr_end,
        	      blr_begin,
        	         blr_erase, 4,
        	         blr_erase, 3,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_3
RDB$RELATION_NAME               RDB$TRIGGERS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:4
        	blr_version5,
        	blr_if,
        	   blr_eql,
        	      blr_field, 0, 15, 'R','D','B','$','S','Y','S','T','E','M','_','F','L','A','G',
        	      blr_literal, blr_short, 0, 1,0,
        	   blr_leave, 0,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_34
RDB$RELATION_NAME               RDB$RELATION_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                6
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c4
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_eql,
        	         blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	         blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	      blr_begin,
        	         blr_for,
        	            blr_rse, 1,
        	               blr_relation, 21, 'R','D','B','$','C','H','E','C','K','_','C','O','N','S','T','R','A','I','N','T','S', 3,
        	               blr_boolean,
        	                  blr_eql,
        	                     blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                     blr_field, 3, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_erase, 3,
        	               blr_for,
        	                  blr_rse, 1,
        	                     blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 4,
        	                     blr_boolean,
        	                        blr_eql,
        	                           blr_field, 4, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                           blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                     blr_end,
        	                  blr_begin,
        	                     blr_erase, 4,
        	                     blr_end,
        	               blr_end,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       2
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_35
RDB$RELATION_NAME               RDB$CHECK_CONSTRAINTS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                6
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c5
        	blr_version5,
        	blr_begin,
        	   blr_for,
        	      blr_rse, 2,
        	         blr_relation, 12, 'R','D','B','$','T','R','I','G','G','E','R','S', 3,
        	         blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	                  blr_field, 0, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','N','A','M','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 4, 19, 'R','D','B','$','C','O','N','S','T','R','A','I','N','T','_','T','Y','P','E',
        	                     blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
        	                  blr_eql,
        	                     blr_field, 3, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	                     blr_field, 0, 16, 'R','D','B','$','T','R','I','G','G','E','R','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_erase, 3,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       2
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_36
RDB$RELATION_NAME               RDB$FIELDS
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                3
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:3c6
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_not,
        	         blr_and,
        	            blr_eql,
        	               blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
        	               blr_field, 1, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H',
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
        	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E',
        	               blr_and,
        	                  blr_eql,
        	                     blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
        	                     blr_field, 1, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D',
        	                  blr_eql,
        	                     blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
        	                     blr_field, 1, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D',
        	      blr_if,
        	         blr_any,
        	            blr_rse, 4,
        	               blr_relation, 11, 'R','D','B','$','I','N','D','I','C','E','S', 3,
        	               blr_relation, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','C','O','N','S','T','R','A','I','N','T','S', 4,
        	               blr_relation, 18, 'R','D','B','$','I','N','D','E','X','_','S','E','G','M','E','N','T','S', 5,
        	               blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 6,
        	               blr_boolean,
        	                  blr_and,
        	                     blr_eql,
        	                        blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 3, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                           blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 4, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                              blr_field, 5, 14, 'R','D','B','$','I','N','D','E','X','_','N','A','M','E',
        	                           blr_and,
        	                              blr_eql,
        	                                 blr_field, 5, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                 blr_field, 6, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                              blr_eql,
        	                                 blr_field, 6, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E',
        	                                 blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	               blr_end,
        	         blr_leave, 1,
        	         blr_end,
        	      blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_8
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                5
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:1
        	blr_version5,
        	blr_if,
        	   blr_not,
        	      blr_missing,
        	         blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	   blr_for,
        	      blr_rse, 1,
        	         blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 3,
        	         blr_boolean,
        	            blr_and,
        	               blr_eql,
        	                  blr_field, 3, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	               blr_eql,
        	                  blr_field, 3, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	         blr_end,
        	      blr_begin,
        	         blr_if,
        	            blr_starting,
        	               blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	               blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
        	            blr_begin,
        	               blr_for,
        	                  blr_rse, 1,
        	                     blr_relation, 20, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S','E','S', 4,
        	                     blr_boolean,
        	                        blr_eql,
        	                           blr_field, 4, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                           blr_field, 3, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                     blr_end,
        	                  blr_begin,
        	                     blr_erase, 4,
        	                     blr_end,
        	               blr_modify, 3, 5,
        	                  blr_begin,
        	                     blr_assignment,
        	                        blr_null,
        	                        blr_field, 5, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                     blr_end,
        	               blr_end,
        	            blr_end,
        	         blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>

RDB$TRIGGER_NAME                RDB$TRIGGER_9
RDB$RELATION_NAME               RDB$USER_PRIVILEGES
RDB$TRIGGER_SEQUENCE            0
RDB$TRIGGER_TYPE                1
RDB$TRIGGER_SOURCE              <null>
RDB$TRIGGER_BLR                 c:2
        	blr_version5,
        	blr_begin,
        	   blr_if,
        	      blr_missing,
        	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
        	      blr_assignment,
        	         blr_user_name,
        	         blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
        	      blr_end,
        	   blr_if,
        	      blr_eql,
        	         blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	         blr_literal, blr_long, 0, 0,0,0,0,
        	      blr_begin,
        	         blr_for,
        	            blr_rse, 1,
        	               blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 6,
        	               blr_boolean,
        	                  blr_eql,
        	                     blr_field, 6, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_if,
        	                  blr_or,
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                           blr_user_name,
        	                        blr_and,
        	                           blr_eql,
        	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                              blr_field, 1, 11, 'R','D','B','$','G','R','A','N','T','O','R',
        	                           blr_eql,
        	                              blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                              blr_field, 1, 8, 'R','D','B','$','U','S','E','R',
        	                     blr_eql,
        	                        blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
        	                           blr_literal, blr_short, 0, 21,0,
        	                        blr_literal, blr_bool, 1,
        	                  blr_begin,
        	                     blr_end,
        	                  blr_if,
        	                     blr_neq,
        	                        blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                        blr_user_name,
        	                     blr_begin,
        	                        blr_end,
        	                     blr_if,
        	                        blr_not,
        	                           blr_missing,
        	                              blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                        blr_begin,
        	                           blr_for,
        	                              blr_rse, 3,
        	                                 blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 8,
        	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 9,
        	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 10,
        	                                 blr_boolean,
        	                                    blr_and,
        	                                       blr_eql,
        	                                          blr_field, 8, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                          blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                       blr_and,
        	                                          blr_eql,
        	                                             blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                             blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                          blr_and,
        	                                             blr_not,
        	                                                blr_missing,
        	                                                   blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
        	                                             blr_and,
        	                                                blr_eql,
        	                                                   blr_field, 9, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
        	                                                   blr_field, 8, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                blr_and,
        	                                                   blr_eql,
        	                                                      blr_field, 9, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
        	                                                      blr_field, 8, 16, 'R','D','B','$','V','I','E','W','_','C','O','N','T','E','X','T',
        	                                                   blr_eql,
        	                                                      blr_field, 9, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                      blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                 blr_end,
        	                              blr_begin,
        	                                 blr_if,
        	                                    blr_and,
        	                                       blr_neq,
        	                                          blr_field, 10, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                       blr_neq,
        	                                          blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
        	                                             blr_literal, blr_short, 0, 21,0,
        	                                          blr_literal, blr_bool, 1,
        	                                    blr_begin,
        	                                       blr_if,
        	                                          blr_not,
        	                                             blr_any,
        	                                                blr_rse, 1,
        	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 11,
        	                                                   blr_boolean,
        	                                                      blr_and,
        	                                                         blr_eql,
        	                                                            blr_field, 11, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                            blr_field, 10, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                         blr_and,
        	                                                            blr_eql,
        	                                                               blr_field, 11, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	                                                               blr_literal, blr_long, 0, 0,0,0,0,
        	                                                            blr_and,
        	                                                               blr_eql,
        	                                                                  blr_field, 11, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
        	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
        	                                                               blr_and,
        	                                                                  blr_eql,
        	                                                                     blr_field, 11, 8, 'R','D','B','$','U','S','E','R',
        	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                                                  blr_and,
        	                                                                     blr_eql,
        	                                                                        blr_field, 11, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
        	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
        	                                                                     blr_and,
        	                                                                        blr_neq,
        	                                                                           blr_field, 11, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
        	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
        	                                                                        blr_or,
        	                                                                           blr_missing,
        	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                                           blr_eql,
        	                                                                              blr_field, 11, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                                              blr_field, 8, 14, 'R','D','B','$','B','A','S','E','_','F','I','E','L','D',
        	                                                   blr_end,
        	                                          blr_leave, 5,
        	                                          blr_end,
        	                                       blr_end,
        	                                    blr_end,
        	                                 blr_end,
        	                           blr_end,
        	                        blr_begin,
        	                           blr_for,
        	                              blr_rse, 2,
        	                                 blr_relation, 18, 'R','D','B','$','V','I','E','W','_','R','E','L','A','T','I','O','N','S', 12,
        	                                 blr_relation, 13, 'R','D','B','$','R','E','L','A','T','I','O','N','S', 13,
        	                                 blr_boolean,
        	                                    blr_and,
        	                                       blr_eql,
        	                                          blr_field, 12, 13, 'R','D','B','$','V','I','E','W','_','N','A','M','E',
        	                                          blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                       blr_eql,
        	                                          blr_field, 12, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                          blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                 blr_end,
        	                              blr_begin,
        	                                 blr_if,
        	                                    blr_and,
        	                                       blr_neq,
        	                                          blr_field, 13, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                          blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                       blr_neq,
        	                                          blr_sys_function, 20, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E',1,
        	                                             blr_literal, blr_short, 0, 21,0,
        	                                          blr_literal, blr_bool, 1,
        	                                    blr_begin,
        	                                       blr_if,
        	                                          blr_not,
        	                                             blr_any,
        	                                                blr_rse, 1,
        	                                                   blr_relation, 19, 'R','D','B','$','U','S','E','R','_','P','R','I','V','I','L','E','G','E','S', 14,
        	                                                   blr_boolean,
        	                                                      blr_and,
        	                                                         blr_eql,
        	                                                            blr_field, 14, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                            blr_field, 13, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                                                         blr_and,
        	                                                            blr_eql,
        	                                                               blr_field, 14, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	                                                               blr_literal, blr_long, 0, 0,0,0,0,
        	                                                            blr_and,
        	                                                               blr_eql,
        	                                                                  blr_field, 14, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
        	                                                                  blr_field, 1, 13, 'R','D','B','$','P','R','I','V','I','L','E','G','E',
        	                                                               blr_and,
        	                                                                  blr_eql,
        	                                                                     blr_field, 14, 8, 'R','D','B','$','U','S','E','R',
        	                                                                     blr_field, 6, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E',
        	                                                                  blr_and,
        	                                                                     blr_eql,
        	                                                                        blr_field, 14, 13, 'R','D','B','$','U','S','E','R','_','T','Y','P','E',
        	                                                                        blr_literal, blr_long, 0, 8,0,0,0,
        	                                                                     blr_and,
        	                                                                        blr_neq,
        	                                                                           blr_field, 14, 16, 'R','D','B','$','G','R','A','N','T','_','O','P','T','I','O','N',
        	                                                                           blr_literal, blr_long, 0, 0,0,0,0,
        	                                                                        blr_missing,
        	                                                                           blr_field, 14, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                                                   blr_end,
        	                                          blr_leave, 5,
        	                                          blr_end,
        	                                       blr_end,
        	                                    blr_end,
        	                                 blr_end,
        	                           blr_end,
        	               blr_if,
        	                  blr_missing,
        	                     blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                  blr_modify, 6, 15,
        	                     blr_begin,
        	                        blr_assignment,
        	                           blr_cast, blr_varying2, 4,0, 252,0,
        	                              blr_concatenate,
        	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                    blr_literal, blr_long, 0, 1,0,0,0,
        	                           blr_field, 15, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                        blr_end,

        	           blr_if,
        	                     blr_not,
        	                        blr_starting,
        	                           blr_field, 6, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                           blr_cast, blr_varying2, 4,0, 252,0,
        	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                     blr_leave, 3,
        	                     blr_end,
        	               blr_end,
        	         blr_if,
        	            blr_not,
        	               blr_missing,
        	                  blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	            blr_for,
        	               blr_rse, 1,
        	                  blr_relation, 19, 'R','D','B','$','R','E','L','A','T','I','O','N','_','F','I','E','L','D','S', 16,
        	                  blr_boolean,
        	                     blr_and,
        	                        blr_eql,
        	                           blr_field, 16, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                        blr_eql,
        	                           blr_field, 16, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                           blr_field, 1, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E',
        	                  blr_end,
        	               blr_begin,
        	                  blr_if,
        	                     blr_missing,
        	                        blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                     blr_modify, 16, 17,
        	                        blr_begin,
        	                           blr_assignment,
        	                              blr_concatenate,
        	                                 blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
        	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                    blr_literal, blr_long, 0, 1,0,0,0,
        	                              blr_field, 17, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                           blr_end,
        	                     blr_if,
        	                        blr_not,
        	                           blr_starting,
        	                              blr_field, 16, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                              blr_literal, blr_text, 9,0, 'S','Q','L','$','G','R','A','N','T',
        	                        blr_leave, 4,
        	                        blr_end,
        	                  blr_end,
        	            blr_end,
        	         blr_end,
        	      blr_if,
        	         blr_eql,
        	            blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	            blr_literal, blr_long, 0, 5,0,0,0,
        	         blr_for,
        	            blr_rse, 1,
        	               blr_relation, 14, 'R','D','B','$','P','R','O','C','E','D','U','R','E','S', 18,
        	               blr_boolean,
        	                  blr_and,
        	                     blr_missing,
        	                        blr_field, 18, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
        	                     blr_eql,
        	                        blr_field, 18, 18, 'R','D','B','$','P','R','O','C','E','D','U','R','E','_','N','A','M','E',
        	                        blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	               blr_end,
        	            blr_begin,
        	               blr_if,
        	                  blr_missing,
        	                     blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                  blr_modify, 18, 20,
        	                     blr_begin,
        	                        blr_assignment,
        	                           blr_cast, blr_varying2, 4,0, 252,0,
        	                              blr_concatenate,
        	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                                 blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                    blr_literal, blr_long, 0, 1,0,0,0,
        	                           blr_field, 20, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                        blr_end,
        	                  blr_if,
        	                     blr_not,
        	                        blr_starting,
        	                           blr_field, 18, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                           blr_cast, blr_varying2, 4,0, 252,0,
        	                              blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                     blr_leave, 3,
        	                     blr_end,
        	               blr_end,
        	         blr_if,
        	            blr_eql,
        	               blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	               blr_literal, blr_long, 0, 15,0,0,0,
        	            blr_for,
        	               blr_rse, 1,
        	                  blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 26,
        	                  blr_boolean,
        	                     blr_and,
        	                        blr_missing,
        	                           blr_field, 26, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
        	                        blr_eql,
        	                           blr_field, 26, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E',
        	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                  blr_end,
        	               blr_begin,
        	                  blr_if,
        	                     blr_missing,
        	                        blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                     blr_modify, 26, 28,
        	                        blr_begin,
        	                           blr_assignment,
        	                              blr_cast, blr_varying2, 4,0, 252,0,
        	                                 blr_concatenate,
        	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                                    blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                       blr_literal, blr_long, 0, 1,0,0,0,
        	                              blr_field, 28, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                           blr_end,
        	                     blr_if,
        	                        blr_not,
        	                           blr_starting,
        	                              blr_field, 26, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                              blr_cast, blr_varying2, 4,0, 252,0,
        	                                 blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                        blr_leave, 3,
        	                        blr_end,
        	                  blr_end,
        	            blr_if,
        	               blr_eql,
        	                  blr_field, 1, 15, 'R','D','B','$','O','B','J','E','C','T','_','T','Y','P','E',
        	                  blr_literal, blr_long, 0, 18,0,0,0,
        	               blr_for,
        	                  blr_rse, 1,
        	                     blr_relation, 12, 'R','D','B','$','P','A','C','K','A','G','E','S', 22,
        	                     blr_boolean,
        	                        blr_eql,
        	                           blr_field, 22, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E',
        	                           blr_field, 1, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E',
        	                     blr_end,
        	                  blr_begin,
        	                     blr_if,
        	                        blr_missing,
        	                           blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                        blr_modify, 22, 23,
        	                           blr_begin,
        	                              blr_assignment,
        	                                 blr_cast, blr_varying2, 4,0, 252,0,
        	                                    blr_concatenate,
        	                                       blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                                       blr_gen_id, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                          blr_literal, blr_long, 0, 1,0,0,0,
        	                                 blr_field, 23, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                              blr_end,
        	                        blr_if,
        	                           blr_not,
        	                              blr_starting,
        	                                 blr_field, 22, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S',
        	                                 blr_cast, blr_varying2, 4,0, 252,0,
        	                                    blr_literal, blr_text2, 1,0, 4,0, 'S','Q','L','$',
        	                           blr_leave, 3,
        	                           blr_end,
        	                     blr_end,
        	               blr_end,
        	   blr_end,
        	blr_eoc

RDB$DESCRIPTION                 <null>
RDB$TRIGGER_INACTIVE            <null>
RDB$SYSTEM_FLAG                 1
RDB$FLAGS                       0
RDB$VALID_BLR                   <null>
RDB$DEBUG_INFO                  <null>
RDB$ENGINE_NAME                 <null>
RDB$ENTRYPOINT                  <null>
RDB$SQL_SECURITY                <null>


Records affected: 26

"""

@pytest.mark.version('>=5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_3(act: Action):
    act.expected_stdout = expected_stdout_3
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
