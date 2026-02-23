#coding:utf-8

"""
ID:          new-database-26
TITLE:       New DB - RDB$SECURITY_CLASSES content
DESCRIPTION: Check the correct content of RDB$SECURITY_CLASSES in new database.
NOTES:
[28.10.2015]
  Updated expected_stdout, added block to subst-section in order to ignore differences
  in values like "SQL$****" of field RDB$SECURITY_CLASS.
FBTEST:      functional.basic.db.26
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
    set blob 3;
    set count on;
    select *
    from rdb$security_classes
    order by rdb$security_class;
"""

act = isql_act('db', test_script,
               substitutions=[('RDB\\$SECURITY_CLASS[\\s]+SQL\\$.*', 'RDB\\$SECURITY_CLASS SQL\\$'),
                              ('RDB\\$ACL.*', ''), ('[\t ]+', ' ')])

# version: 3.0

expected_stdout_1 = """
    RDB$SECURITY_CLASS              SQL$1
    RDB$ACL                         9:0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$10
    RDB$ACL                         9:9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$100
    RDB$ACL                         9:63
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$101
    RDB$ACL                         9:64
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$102
    RDB$ACL                         9:65
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$103
    RDB$ACL                         9:66
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$104
    RDB$ACL                         9:67
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$105
    RDB$ACL                         9:68
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$106
    RDB$ACL                         9:69
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$107
    RDB$ACL                         9:6a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$108
    RDB$ACL                         9:6b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$109
    RDB$ACL                         9:6c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$11
    RDB$ACL                         9:a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$110
    RDB$ACL                         9:6d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$111
    RDB$ACL                         9:6e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$112
    RDB$ACL                         9:6f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$113
    RDB$ACL                         9:70
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$114
    RDB$ACL                         9:71
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$115
    RDB$ACL                         9:72
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$116
    RDB$ACL                         9:73
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$117
    RDB$ACL                         9:74
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$118
    RDB$ACL                         9:75
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$119
    RDB$ACL                         9:76
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$12
    RDB$ACL                         9:b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$120
    RDB$ACL                         9:77
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$121
    RDB$ACL                         9:78
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$122
    RDB$ACL                         9:79
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$123
    RDB$ACL                         9:7a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$124
    RDB$ACL                         9:7b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$125
    RDB$ACL                         9:7c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$126
    RDB$ACL                         9:7d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$127
    RDB$ACL                         9:7e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$128
    RDB$ACL                         9:7f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$129
    RDB$ACL                         9:80
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$13
    RDB$ACL                         9:c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$130
    RDB$ACL                         9:81
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$131
    RDB$ACL                         9:82
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$132
    RDB$ACL                         9:83
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$133
    RDB$ACL                         9:84
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$134
    RDB$ACL                         9:85
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$135
    RDB$ACL                         9:86
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$136
    RDB$ACL                         9:87
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$137
    RDB$ACL                         9:5a0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$138
    RDB$ACL                         9:5a1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$139
    RDB$ACL                         9:5a2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$14
    RDB$ACL                         9:d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$140
    RDB$ACL                         9:5a3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$141
    RDB$ACL                         9:5a4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$142
    RDB$ACL                         9:5a5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$143
    RDB$ACL                         9:5a6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$144
    RDB$ACL                         9:5a7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$145
    RDB$ACL                         9:5a8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$146
    RDB$ACL                         9:5a9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$147
    RDB$ACL                         9:5aa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$148
    RDB$ACL                         9:5ab
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$149
    RDB$ACL                         9:5ac
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$15
    RDB$ACL                         9:e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$150
    RDB$ACL                         9:5ad
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$151
    RDB$ACL                         9:5ae
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$152
    RDB$ACL                         9:5af
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$153
    RDB$ACL                         9:5b0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$154
    RDB$ACL                         9:5b1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$155
    RDB$ACL                         9:5b2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$156
    RDB$ACL                         9:5b3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$157
    RDB$ACL                         9:5b4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$158
    RDB$ACL                         9:5b5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$159
    RDB$ACL                         9:5b6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$16
    RDB$ACL                         9:f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$160
    RDB$ACL                         9:5b7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$161
    RDB$ACL                         9:5b8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$162
    RDB$ACL                         9:5b9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$163
    RDB$ACL                         9:5ba
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$164
    RDB$ACL                         9:5bb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$165
    RDB$ACL                         9:5bc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$166
    RDB$ACL                         9:5bd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$167
    RDB$ACL                         9:5be
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$168
    RDB$ACL                         9:5bf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$169
    RDB$ACL                         9:5c0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$17
    RDB$ACL                         9:10
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$170
    RDB$ACL                         9:5c1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$171
    RDB$ACL                         9:5c2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$172
    RDB$ACL                         9:5c3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$173
    RDB$ACL                         9:5c4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$174
    RDB$ACL                         9:5c5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$175
    RDB$ACL                         9:5c6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$176
    RDB$ACL                         9:5c7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$177
    RDB$ACL                         9:5c8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$178
    RDB$ACL                         9:5c9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$179
    RDB$ACL                         9:5ca
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$18
    RDB$ACL                         9:11
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$180
    RDB$ACL                         9:5cb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$181
    RDB$ACL                         9:5cc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$182
    RDB$ACL                         9:5cd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$183
    RDB$ACL                         9:5ce
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$184
    RDB$ACL                         9:5cf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$185
    RDB$ACL                         9:5d0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$186
    RDB$ACL                         9:5d1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$187
    RDB$ACL                         9:5d2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$188
    RDB$ACL                         9:5d3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$189
    RDB$ACL                         9:5d4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$19
    RDB$ACL                         9:12
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$190
    RDB$ACL                         9:5d5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$191
    RDB$ACL                         9:5d6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$192
    RDB$ACL                         9:5d7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$193
    RDB$ACL                         9:5d8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$194
    RDB$ACL                         9:5d9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$195
    RDB$ACL                         9:5da
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$196
    RDB$ACL                         9:5db
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$197
    RDB$ACL                         9:5dc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$198
    RDB$ACL                         9:5dd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$199
    RDB$ACL                         9:5de
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$2
    RDB$ACL                         9:1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$20
    RDB$ACL                         9:13
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$200
    RDB$ACL                         9:5df
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$201
    RDB$ACL                         9:5e0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$202
    RDB$ACL                         9:5e1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$203
    RDB$ACL                         9:5e2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$204
    RDB$ACL                         9:5e3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$205
    RDB$ACL                         9:5e4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$206
    RDB$ACL                         9:5e5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$207
    RDB$ACL                         9:5e6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$208
    RDB$ACL                         9:5e7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$209
    RDB$ACL                         9:5e8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$21
    RDB$ACL                         9:14
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$210
    RDB$ACL                         9:5e9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$211
    RDB$ACL                         9:5ea
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$212
    RDB$ACL                         9:5eb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$213
    RDB$ACL                         9:5ec
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$214
    RDB$ACL                         9:5ed
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$215
    RDB$ACL                         9:5ee
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$216
    RDB$ACL                         9:5ef
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$217
    RDB$ACL                         9:5f0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$218
    RDB$ACL                         9:5f1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$219
    RDB$ACL                         9:5f2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$22
    RDB$ACL                         9:15
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$220
    RDB$ACL                         9:5f3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$221
    RDB$ACL                         9:5f4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$222
    RDB$ACL                         9:5f5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$223
    RDB$ACL                         9:5f6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$224
    RDB$ACL                         9:5f7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$225
    RDB$ACL                         9:5f8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$226
    RDB$ACL                         9:5f9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$227
    RDB$ACL                         9:5fa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$228
    RDB$ACL                         9:5fb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$229
    RDB$ACL                         9:5fc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$23
    RDB$ACL                         9:16
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$230
    RDB$ACL                         9:5fd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$231
    RDB$ACL                         9:5fe
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$232
    RDB$ACL                         9:5ff
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$233
    RDB$ACL                         9:600
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$234
    RDB$ACL                         9:601
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$235
    RDB$ACL                         9:602
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$236
    RDB$ACL                         9:603
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$237
    RDB$ACL                         9:604
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$238
    RDB$ACL                         9:605
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$239
    RDB$ACL                         9:606
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$24
    RDB$ACL                         9:17
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$240
    RDB$ACL                         9:607
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$241
    RDB$ACL                         9:608
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$242
    RDB$ACL                         9:609
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$243
    RDB$ACL                         9:60a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$244
    RDB$ACL                         9:60b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$245
    RDB$ACL                         9:60c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$246
    RDB$ACL                         9:60d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$247
    RDB$ACL                         9:60e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$248
    RDB$ACL                         9:60f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$249
    RDB$ACL                         9:610
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$25
    RDB$ACL                         9:18
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$250
    RDB$ACL                         9:611
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$251
    RDB$ACL                         9:612
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$252
    RDB$ACL                         9:613
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$253
    RDB$ACL                         9:614
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$254
    RDB$ACL                         9:615
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$255
    RDB$ACL                         9:616
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$256
    RDB$ACL                         9:617
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$257
    RDB$ACL                         9:618
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$258
    RDB$ACL                         9:619
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$259
    RDB$ACL                         9:61a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$26
    RDB$ACL                         9:19
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$260
    RDB$ACL                         9:61b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$261
    RDB$ACL                         9:61c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$262
    RDB$ACL                         9:61d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$263
    RDB$ACL                         9:61e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$264
    RDB$ACL                         9:61f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$265
    RDB$ACL                         9:620
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$266
    RDB$ACL                         9:621
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$267
    RDB$ACL                         9:622
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$268
    RDB$ACL                         9:623
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$269
    RDB$ACL                         9:624
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$27
    RDB$ACL                         9:1a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$270
    RDB$ACL                         9:625
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$271
    RDB$ACL                         9:626
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$272
    RDB$ACL                         9:627
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$273
    RDB$ACL                         9:960
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$274
    RDB$ACL                         9:961
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$275
    RDB$ACL                         9:962
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$276
    RDB$ACL                         9:963
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$277
    RDB$ACL                         9:964
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$278
    RDB$ACL                         9:965
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$279
    RDB$ACL                         9:966
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$28
    RDB$ACL                         9:1b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$280
    RDB$ACL                         9:967
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$281
    RDB$ACL                         9:968
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$282
    RDB$ACL                         9:969
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$283
    RDB$ACL                         9:96a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$284
    RDB$ACL                         9:96b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$285
    RDB$ACL                         9:96c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$286
    RDB$ACL                         9:96d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$287
    RDB$ACL                         9:96e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$288
    RDB$ACL                         9:96f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$289
    RDB$ACL                         9:970
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$29
    RDB$ACL                         9:1c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$290
    RDB$ACL                         9:971
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$291
    RDB$ACL                         9:972
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$292
    RDB$ACL                         9:973
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$293
    RDB$ACL                         9:974
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$294
    RDB$ACL                         9:975
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$295
    RDB$ACL                         9:976
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$296
    RDB$ACL                         9:977
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$297
    RDB$ACL                         9:978
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$298
    RDB$ACL                         9:979
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$299
    RDB$ACL                         9:97a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$3
    RDB$ACL                         9:2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$30
    RDB$ACL                         9:1d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$300
    RDB$ACL                         9:97b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$301
    RDB$ACL                         9:97c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$302
    RDB$ACL                         9:97d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$303
    RDB$ACL                         9:97e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$304
    RDB$ACL                         9:97f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$305
    RDB$ACL                         9:980
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$306
    RDB$ACL                         9:981
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$307
    RDB$ACL                         9:982
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$308
    RDB$ACL                         9:983
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$309
    RDB$ACL                         9:984
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$31
    RDB$ACL                         9:1e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$310
    RDB$ACL                         9:985
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$311
    RDB$ACL                         9:986
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$312
    RDB$ACL                         9:987
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$313
    RDB$ACL                         9:988
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$314
    RDB$ACL                         9:989
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$315
    RDB$ACL                         9:98a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$316
    RDB$ACL                         9:98b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$317
    RDB$ACL                         9:98c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$318
    RDB$ACL                         9:98d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$319
    RDB$ACL                         9:98e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$32
    RDB$ACL                         9:1f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$320
    RDB$ACL                         9:98f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$321
    RDB$ACL                         9:990
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$322
    RDB$ACL                         9:991
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$323
    RDB$ACL                         9:992
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$324
    RDB$ACL                         9:993
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$325
    RDB$ACL                         9:994
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$326
    RDB$ACL                         9:995
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$327
    RDB$ACL                         9:996
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$328
    RDB$ACL                         9:997
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$329
    RDB$ACL                         9:998
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$33
    RDB$ACL                         9:20
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$330
    RDB$ACL                         9:999
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$331
    RDB$ACL                         9:99a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$332
    RDB$ACL                         9:99b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$333
    RDB$ACL                         9:99c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$334
    RDB$ACL                         9:99d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$335
    RDB$ACL                         9:99e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$336
    RDB$ACL                         9:99f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$337
    RDB$ACL                         9:9a0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$338
    RDB$ACL                         9:9a1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$339
    RDB$ACL                         9:9a2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$34
    RDB$ACL                         9:21
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$340
    RDB$ACL                         9:9a3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$341
    RDB$ACL                         9:9a4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$342
    RDB$ACL                         9:9a5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$343
    RDB$ACL                         9:9a6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$344
    RDB$ACL                         9:9a7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$345
    RDB$ACL                         9:9a8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$346
    RDB$ACL                         9:9a9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$347
    RDB$ACL                         9:9aa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$348
    RDB$ACL                         9:9ab
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$349
    RDB$ACL                         9:9ac
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$35
    RDB$ACL                         9:22
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$350
    RDB$ACL                         9:9ad
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$351
    RDB$ACL                         9:9ae
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$352
    RDB$ACL                         9:9af
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$353
    RDB$ACL                         9:9b0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$354
    RDB$ACL                         9:9b1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$355
    RDB$ACL                         9:9b2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$356
    RDB$ACL                         9:9b3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$357
    RDB$ACL                         9:9b4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$358
    RDB$ACL                         9:9b5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$359
    RDB$ACL                         9:9b6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$36
    RDB$ACL                         9:23
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$360
    RDB$ACL                         9:9b7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$361
    RDB$ACL                         9:9b8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$362
    RDB$ACL                         9:9b9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$363
    RDB$ACL                         9:9ba
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$364
    RDB$ACL                         9:9c7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$365
    RDB$ACL                         9:9c8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$366
    RDB$ACL                         9:9ca
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$367
    RDB$ACL                         9:9cc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$368
    RDB$ACL                         9:9ce
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$369
    RDB$ACL                         9:9d0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$37
    RDB$ACL                         9:24
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$370
    RDB$ACL                         9:9d2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$371
    RDB$ACL                         9:9d4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$372
    RDB$ACL                         9:9d6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$373
    RDB$ACL                         9:9d8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$374
    RDB$ACL                         9:9da
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$375
    RDB$ACL                         9:9dc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$376
    RDB$ACL                         9:9de
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$377
    RDB$ACL                         9:9e0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$378
    RDB$ACL                         9:9e2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$379
    RDB$ACL                         9:9e4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$38
    RDB$ACL                         9:25
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$380
    RDB$ACL                         9:9e6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$381
    RDB$ACL                         9:9e8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$382
    RDB$ACL                         9:d21
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$383
    RDB$ACL                         9:d23
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$384
    RDB$ACL                         9:d25
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$385
    RDB$ACL                         9:d27
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$386
    RDB$ACL                         9:d29
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$387
    RDB$ACL                         9:d2b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$388
    RDB$ACL                         9:d2d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$389
    RDB$ACL                         9:d2f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$39
    RDB$ACL                         9:26
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$390
    RDB$ACL                         9:d31
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$391
    RDB$ACL                         9:d33
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$392
    RDB$ACL                         9:d35
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$393
    RDB$ACL                         9:d37
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$394
    RDB$ACL                         9:d39
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$395
    RDB$ACL                         9:d3b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$396
    RDB$ACL                         9:d3d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$397
    RDB$ACL                         9:d3f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$398
    RDB$ACL                         9:d41
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$399
    RDB$ACL                         9:d43
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$4
    RDB$ACL                         9:3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$40
    RDB$ACL                         9:27
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$400
    RDB$ACL                         9:d45
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$401
    RDB$ACL                         9:d47
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$402
    RDB$ACL                         9:d49
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$403
    RDB$ACL                         9:d4b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$404
    RDB$ACL                         9:d4d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$405
    RDB$ACL                         9:d4f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$406
    RDB$ACL                         9:d51
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$407
    RDB$ACL                         9:d53
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$408
    RDB$ACL                         9:d55
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$409
    RDB$ACL                         9:d57
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$41
    RDB$ACL                         9:28
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$410
    RDB$ACL                         9:d59
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$411
    RDB$ACL                         9:d5b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$412
    RDB$ACL                         9:d5d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$413
    RDB$ACL                         9:d5f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$414
    RDB$ACL                         9:d61
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$42
    RDB$ACL                         9:29
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$43
    RDB$ACL                         9:2a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$44
    RDB$ACL                         9:2b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$45
    RDB$ACL                         9:2c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$46
    RDB$ACL                         9:2d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$47
    RDB$ACL                         9:2e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$48
    RDB$ACL                         9:2f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$49
    RDB$ACL                         9:30
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$5
    RDB$ACL                         9:4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$50
    RDB$ACL                         9:31
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$51
    RDB$ACL                         9:32
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$52
    RDB$ACL                         9:33
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$53
    RDB$ACL                         9:34
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$54
    RDB$ACL                         9:35
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$55
    RDB$ACL                         9:36
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$56
    RDB$ACL                         9:37
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$57
    RDB$ACL                         9:38
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$58
    RDB$ACL                         9:39
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$59
    RDB$ACL                         9:3a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$6
    RDB$ACL                         9:5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$60
    RDB$ACL                         9:3b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$61
    RDB$ACL                         9:3c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$62
    RDB$ACL                         9:3d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$63
    RDB$ACL                         9:3e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$64
    RDB$ACL                         9:3f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$65
    RDB$ACL                         9:40
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$66
    RDB$ACL                         9:41
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$67
    RDB$ACL                         9:42
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$68
    RDB$ACL                         9:43
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$69
    RDB$ACL                         9:44
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$7
    RDB$ACL                         9:6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$70
    RDB$ACL                         9:45
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$71
    RDB$ACL                         9:46
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$72
    RDB$ACL                         9:47
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$73
    RDB$ACL                         9:48
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$74
    RDB$ACL                         9:49
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$75
    RDB$ACL                         9:4a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$76
    RDB$ACL                         9:4b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$77
    RDB$ACL                         9:4c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$78
    RDB$ACL                         9:4d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$79
    RDB$ACL                         9:4e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$8
    RDB$ACL                         9:7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$80
    RDB$ACL                         9:4f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$81
    RDB$ACL                         9:50
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$82
    RDB$ACL                         9:51
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$83
    RDB$ACL                         9:52
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$84
    RDB$ACL                         9:53
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$85
    RDB$ACL                         9:54
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$86
    RDB$ACL                         9:55
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$87
    RDB$ACL                         9:56
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$88
    RDB$ACL                         9:57
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$89
    RDB$ACL                         9:58
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$9
    RDB$ACL                         9:8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$90
    RDB$ACL                         9:59
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$91
    RDB$ACL                         9:5a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$92
    RDB$ACL                         9:5b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$93
    RDB$ACL                         9:5c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$94
    RDB$ACL                         9:5d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$95
    RDB$ACL                         9:5e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$96
    RDB$ACL                         9:5f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$97
    RDB$ACL                         9:60
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$98
    RDB$ACL                         9:61
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$99
    RDB$ACL                         9:62
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$CHARSETS
    RDB$ACL                         9:9c4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$COLLATIONS
    RDB$ACL                         9:9c5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT1
    RDB$ACL                         9:9c9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT10
    RDB$ACL                         9:9db
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT11
    RDB$ACL                         9:9dd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT12
    RDB$ACL                         9:9df
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT13
    RDB$ACL                         9:9e1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT14
    RDB$ACL                         9:9e3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT15
    RDB$ACL                         9:9e5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT16
    RDB$ACL                         9:9e7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT17
    RDB$ACL                         9:d20
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT18
    RDB$ACL                         9:d22
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT19
    RDB$ACL                         9:d24
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT2
    RDB$ACL                         9:9cb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT20
    RDB$ACL                         9:d26
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT21
    RDB$ACL                         9:d28
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT22
    RDB$ACL                         9:d2a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT23
    RDB$ACL                         9:d2c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT24
    RDB$ACL                         9:d2e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT25
    RDB$ACL                         9:d30
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT26
    RDB$ACL                         9:d32
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT27
    RDB$ACL                         9:d34
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT28
    RDB$ACL                         9:d36
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT29
    RDB$ACL                         9:d38
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT3
    RDB$ACL                         9:9cd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT30
    RDB$ACL                         9:d3a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT31
    RDB$ACL                         9:d3c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT32
    RDB$ACL                         9:d3e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT33
    RDB$ACL                         9:d40
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT34
    RDB$ACL                         9:d42
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT35
    RDB$ACL                         9:d44
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT36
    RDB$ACL                         9:d46
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT37
    RDB$ACL                         9:d48
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT38
    RDB$ACL                         9:d4a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT39
    RDB$ACL                         9:d4c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT4
    RDB$ACL                         9:9cf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT40
    RDB$ACL                         9:d4e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT41
    RDB$ACL                         9:d50
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT42
    RDB$ACL                         9:d52
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT43
    RDB$ACL                         9:d54
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT44
    RDB$ACL                         9:d56
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT45
    RDB$ACL                         9:d58
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT46
    RDB$ACL                         9:d5a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT47
    RDB$ACL                         9:d5c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT48
    RDB$ACL                         9:d5e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT49
    RDB$ACL                         9:d60
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT5
    RDB$ACL                         9:9d1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT50
    RDB$ACL                         9:d62
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT6
    RDB$ACL                         9:9d3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT7
    RDB$ACL                         9:9d5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT8
    RDB$ACL                         9:9d7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT9
    RDB$ACL                         9:9d9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DOMAINS
    RDB$ACL                         9:9c1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$EXCEPTIONS
    RDB$ACL                         9:9c2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$FILTERS
    RDB$ACL                         9:9c6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$FUNCTIONS
    RDB$ACL                         9:9be
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$GENERATORS
    RDB$ACL                         9:9c0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$PACKAGES
    RDB$ACL                         9:9bf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$PROCEDURES
    RDB$ACL                         9:9bd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$ROLES
    RDB$ACL                         9:9c3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$TABLES
    RDB$ACL                         9:9bb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$VIEWS
    RDB$ACL                         9:9bc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>


    Records affected: 476
"""

@pytest.mark.version('>=3.0,<4.0')
@pytest.mark.skip("DISABLED: see notes")
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 4.0

expected_stdout_2 = """
    RDB$SECURITY_CLASS              SQL$1
    RDB$ACL                         9:0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$10
    RDB$ACL                         9:9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$100
    RDB$ACL                         9:63
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$101
    RDB$ACL                         9:64
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$102
    RDB$ACL                         9:65
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$103
    RDB$ACL                         9:66
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$104
    RDB$ACL                         9:67
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$105
    RDB$ACL                         9:68
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$106
    RDB$ACL                         9:69
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$107
    RDB$ACL                         9:6a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$108
    RDB$ACL                         9:6b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$109
    RDB$ACL                         9:6c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$11
    RDB$ACL                         9:a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$110
    RDB$ACL                         9:6d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$111
    RDB$ACL                         9:6e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$112
    RDB$ACL                         9:6f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$113
    RDB$ACL                         9:70
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$114
    RDB$ACL                         9:71
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$115
    RDB$ACL                         9:72
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$116
    RDB$ACL                         9:73
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$117
    RDB$ACL                         9:74
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$118
    RDB$ACL                         9:75
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$119
    RDB$ACL                         9:76
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$12
    RDB$ACL                         9:b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$120
    RDB$ACL                         9:77
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$121
    RDB$ACL                         9:78
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$122
    RDB$ACL                         9:79
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$123
    RDB$ACL                         9:7a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$124
    RDB$ACL                         9:7b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$125
    RDB$ACL                         9:7c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$126
    RDB$ACL                         9:7d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$127
    RDB$ACL                         9:7e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$128
    RDB$ACL                         9:7f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$129
    RDB$ACL                         9:80
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$13
    RDB$ACL                         9:c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$130
    RDB$ACL                         9:81
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$131
    RDB$ACL                         9:82
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$132
    RDB$ACL                         9:83
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$133
    RDB$ACL                         9:84
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$134
    RDB$ACL                         9:85
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$135
    RDB$ACL                         9:86
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$136
    RDB$ACL                         9:87
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$137
    RDB$ACL                         9:5a0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$138
    RDB$ACL                         9:5a1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$139
    RDB$ACL                         9:5a2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$14
    RDB$ACL                         9:d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$140
    RDB$ACL                         9:5a3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$141
    RDB$ACL                         9:5a4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$142
    RDB$ACL                         9:5a5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$143
    RDB$ACL                         9:5a6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$144
    RDB$ACL                         9:5a7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$145
    RDB$ACL                         9:5a8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$146
    RDB$ACL                         9:5a9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$147
    RDB$ACL                         9:5aa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$148
    RDB$ACL                         9:5ab
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$149
    RDB$ACL                         9:5ac
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$15
    RDB$ACL                         9:e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$150
    RDB$ACL                         9:5ad
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$151
    RDB$ACL                         9:5ae
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$152
    RDB$ACL                         9:5af
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$153
    RDB$ACL                         9:5b0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$154
    RDB$ACL                         9:5b1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$155
    RDB$ACL                         9:5b2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$156
    RDB$ACL                         9:5b3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$157
    RDB$ACL                         9:5b4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$158
    RDB$ACL                         9:5b5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$159
    RDB$ACL                         9:5b6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$16
    RDB$ACL                         9:f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$160
    RDB$ACL                         9:5b7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$161
    RDB$ACL                         9:5b8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$162
    RDB$ACL                         9:5b9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$163
    RDB$ACL                         9:5ba
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$164
    RDB$ACL                         9:5bb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$165
    RDB$ACL                         9:5bc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$166
    RDB$ACL                         9:5bd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$167
    RDB$ACL                         9:5be
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$168
    RDB$ACL                         9:5bf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$169
    RDB$ACL                         9:5c0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$17
    RDB$ACL                         9:10
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$170
    RDB$ACL                         9:5c1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$171
    RDB$ACL                         9:5c2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$172
    RDB$ACL                         9:5c3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$173
    RDB$ACL                         9:5c4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$174
    RDB$ACL                         9:5c5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$175
    RDB$ACL                         9:5c6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$176
    RDB$ACL                         9:5c7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$177
    RDB$ACL                         9:5c8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$178
    RDB$ACL                         9:5c9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$179
    RDB$ACL                         9:5ca
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$18
    RDB$ACL                         9:11
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$180
    RDB$ACL                         9:5cb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$181
    RDB$ACL                         9:5cc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$182
    RDB$ACL                         9:5cd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$183
    RDB$ACL                         9:5ce
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$184
    RDB$ACL                         9:5cf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$185
    RDB$ACL                         9:5d0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$186
    RDB$ACL                         9:5d1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$187
    RDB$ACL                         9:5d2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$188
    RDB$ACL                         9:5d3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$189
    RDB$ACL                         9:5d4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$19
    RDB$ACL                         9:12
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$190
    RDB$ACL                         9:5d5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$191
    RDB$ACL                         9:5d6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$192
    RDB$ACL                         9:5d7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$193
    RDB$ACL                         9:5d8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$194
    RDB$ACL                         9:5d9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$195
    RDB$ACL                         9:5da
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$196
    RDB$ACL                         9:5db
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$197
    RDB$ACL                         9:5dc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$198
    RDB$ACL                         9:5dd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$199
    RDB$ACL                         9:5de
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$2
    RDB$ACL                         9:1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$20
    RDB$ACL                         9:13
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$200
    RDB$ACL                         9:5df
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$201
    RDB$ACL                         9:5e0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$202
    RDB$ACL                         9:5e1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$203
    RDB$ACL                         9:5e2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$204
    RDB$ACL                         9:5e3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$205
    RDB$ACL                         9:5e4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$206
    RDB$ACL                         9:5e5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$207
    RDB$ACL                         9:5e6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$208
    RDB$ACL                         9:5e7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$209
    RDB$ACL                         9:5e8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$21
    RDB$ACL                         9:14
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$210
    RDB$ACL                         9:5e9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$211
    RDB$ACL                         9:5ea
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$212
    RDB$ACL                         9:5eb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$213
    RDB$ACL                         9:5ec
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$214
    RDB$ACL                         9:5ed
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$215
    RDB$ACL                         9:5ee
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$216
    RDB$ACL                         9:5ef
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$217
    RDB$ACL                         9:5f0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$218
    RDB$ACL                         9:5f1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$219
    RDB$ACL                         9:5f2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$22
    RDB$ACL                         9:15
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$220
    RDB$ACL                         9:5f3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$221
    RDB$ACL                         9:5f4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$222
    RDB$ACL                         9:5f5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$223
    RDB$ACL                         9:5f6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$224
    RDB$ACL                         9:5f7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$225
    RDB$ACL                         9:5f8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$226
    RDB$ACL                         9:5f9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$227
    RDB$ACL                         9:5fa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$228
    RDB$ACL                         9:5fb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$229
    RDB$ACL                         9:5fc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$23
    RDB$ACL                         9:16
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$230
    RDB$ACL                         9:5fd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$231
    RDB$ACL                         9:5fe
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$232
    RDB$ACL                         9:5ff
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$233
    RDB$ACL                         9:600
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$234
    RDB$ACL                         9:601
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$235
    RDB$ACL                         9:602
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$236
    RDB$ACL                         9:603
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$237
    RDB$ACL                         9:604
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$238
    RDB$ACL                         9:605
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$239
    RDB$ACL                         9:606
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$24
    RDB$ACL                         9:17
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$240
    RDB$ACL                         9:607
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$241
    RDB$ACL                         9:608
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$242
    RDB$ACL                         9:609
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$243
    RDB$ACL                         9:60a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$244
    RDB$ACL                         9:60b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$245
    RDB$ACL                         9:60c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$246
    RDB$ACL                         9:60d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$247
    RDB$ACL                         9:60e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$248
    RDB$ACL                         9:60f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$249
    RDB$ACL                         9:610
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$25
    RDB$ACL                         9:18
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$250
    RDB$ACL                         9:611
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$251
    RDB$ACL                         9:612
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$252
    RDB$ACL                         9:613
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$253
    RDB$ACL                         9:614
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$254
    RDB$ACL                         9:615
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$255
    RDB$ACL                         9:616
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$256
    RDB$ACL                         9:617
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$257
    RDB$ACL                         9:618
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$258
    RDB$ACL                         9:619
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$259
    RDB$ACL                         9:61a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$26
    RDB$ACL                         9:19
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$260
    RDB$ACL                         9:61b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$261
    RDB$ACL                         9:61c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$262
    RDB$ACL                         9:61d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$263
    RDB$ACL                         9:61e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$264
    RDB$ACL                         9:61f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$265
    RDB$ACL                         9:620
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$266
    RDB$ACL                         9:621
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$267
    RDB$ACL                         9:622
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$268
    RDB$ACL                         9:623
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$269
    RDB$ACL                         9:624
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$27
    RDB$ACL                         9:1a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$270
    RDB$ACL                         9:625
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$271
    RDB$ACL                         9:626
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$272
    RDB$ACL                         9:627
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$273
    RDB$ACL                         9:960
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$274
    RDB$ACL                         9:961
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$275
    RDB$ACL                         9:962
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$276
    RDB$ACL                         9:963
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$277
    RDB$ACL                         9:964
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$278
    RDB$ACL                         9:965
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$279
    RDB$ACL                         9:966
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$28
    RDB$ACL                         9:1b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$280
    RDB$ACL                         9:967
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$281
    RDB$ACL                         9:968
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$282
    RDB$ACL                         9:969
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$283
    RDB$ACL                         9:96a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$284
    RDB$ACL                         9:96b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$285
    RDB$ACL                         9:96c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$286
    RDB$ACL                         9:96d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$287
    RDB$ACL                         9:96e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$288
    RDB$ACL                         9:96f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$289
    RDB$ACL                         9:970
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$29
    RDB$ACL                         9:1c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$290
    RDB$ACL                         9:971
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$291
    RDB$ACL                         9:972
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$292
    RDB$ACL                         9:973
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$293
    RDB$ACL                         9:974
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$294
    RDB$ACL                         9:975
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$295
    RDB$ACL                         9:976
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$296
    RDB$ACL                         9:977
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$297
    RDB$ACL                         9:978
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$298
    RDB$ACL                         9:979
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$299
    RDB$ACL                         9:97a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$3
    RDB$ACL                         9:2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$30
    RDB$ACL                         9:1d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$300
    RDB$ACL                         9:97b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$301
    RDB$ACL                         9:97c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$302
    RDB$ACL                         9:97d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$303
    RDB$ACL                         9:97e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$304
    RDB$ACL                         9:97f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$305
    RDB$ACL                         9:980
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$306
    RDB$ACL                         9:981
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$307
    RDB$ACL                         9:982
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$308
    RDB$ACL                         9:983
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$309
    RDB$ACL                         9:984
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$31
    RDB$ACL                         9:1e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$310
    RDB$ACL                         9:985
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$311
    RDB$ACL                         9:986
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$312
    RDB$ACL                         9:987
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$313
    RDB$ACL                         9:988
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$314
    RDB$ACL                         9:989
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$315
    RDB$ACL                         9:98a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$316
    RDB$ACL                         9:98b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$317
    RDB$ACL                         9:98c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$318
    RDB$ACL                         9:98d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$319
    RDB$ACL                         9:98e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$32
    RDB$ACL                         9:1f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$320
    RDB$ACL                         9:98f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$321
    RDB$ACL                         9:990
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$322
    RDB$ACL                         9:991
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$323
    RDB$ACL                         9:992
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$324
    RDB$ACL                         9:993
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$325
    RDB$ACL                         9:994
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$326
    RDB$ACL                         9:995
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$327
    RDB$ACL                         9:996
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$328
    RDB$ACL                         9:997
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$329
    RDB$ACL                         9:998
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$33
    RDB$ACL                         9:20
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$330
    RDB$ACL                         9:999
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$331
    RDB$ACL                         9:99a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$332
    RDB$ACL                         9:99b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$333
    RDB$ACL                         9:99c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$334
    RDB$ACL                         9:99d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$335
    RDB$ACL                         9:99e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$336
    RDB$ACL                         9:99f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$337
    RDB$ACL                         9:9a0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$338
    RDB$ACL                         9:9a1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$339
    RDB$ACL                         9:9a2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$34
    RDB$ACL                         9:21
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$340
    RDB$ACL                         9:9a3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$341
    RDB$ACL                         9:9a4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$342
    RDB$ACL                         9:9a5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$343
    RDB$ACL                         9:9a6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$344
    RDB$ACL                         9:9a7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$345
    RDB$ACL                         9:9a8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$346
    RDB$ACL                         9:9a9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$347
    RDB$ACL                         9:9aa
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$348
    RDB$ACL                         9:9ab
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$349
    RDB$ACL                         9:9ac
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$35
    RDB$ACL                         9:22
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$350
    RDB$ACL                         9:9ad
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$351
    RDB$ACL                         9:9ae
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$352
    RDB$ACL                         9:9af
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$353
    RDB$ACL                         9:9b0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$354
    RDB$ACL                         9:9b1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$355
    RDB$ACL                         9:9b2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$356
    RDB$ACL                         9:9b3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$357
    RDB$ACL                         9:9b4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$358
    RDB$ACL                         9:9b5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$359
    RDB$ACL                         9:9b6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$36
    RDB$ACL                         9:23
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$360
    RDB$ACL                         9:9b7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$361
    RDB$ACL                         9:9b8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$362
    RDB$ACL                         9:9b9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$363
    RDB$ACL                         9:9ba
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$364
    RDB$ACL                         9:9bb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$365
    RDB$ACL                         9:9bc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$366
    RDB$ACL                         9:9bd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$367
    RDB$ACL                         9:9be
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$368
    RDB$ACL                         9:9bf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$369
    RDB$ACL                         9:9c0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$37
    RDB$ACL                         9:24
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$370
    RDB$ACL                         9:9c1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$371
    RDB$ACL                         9:9c2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$372
    RDB$ACL                         9:9c3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$373
    RDB$ACL                         9:9c4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$374
    RDB$ACL                         9:9c5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$375
    RDB$ACL                         9:9c6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$376
    RDB$ACL                         9:9c7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$377
    RDB$ACL                         9:9c8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$378
    RDB$ACL                         9:9c9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$379
    RDB$ACL                         9:9ca
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$38
    RDB$ACL                         9:25
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$380
    RDB$ACL                         9:9cb
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$381
    RDB$ACL                         9:9cc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$382
    RDB$ACL                         9:9cd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$383
    RDB$ACL                         9:9ce
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$384
    RDB$ACL                         9:9cf
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (execute)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$385
    RDB$ACL                         9:9dc
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$386
    RDB$ACL                         9:9dd
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$387
    RDB$ACL                         9:9df
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$388
    RDB$ACL                         9:9e1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$389
    RDB$ACL                         9:9e3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$39
    RDB$ACL                         9:26
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$390
    RDB$ACL                         9:9e5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$391
    RDB$ACL                         9:9e7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$392
    RDB$ACL                         9:d20
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$393
    RDB$ACL                         9:d22
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$394
    RDB$ACL                         9:d24
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$395
    RDB$ACL                         9:d26
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$396
    RDB$ACL                         9:d28
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$397
    RDB$ACL                         9:d55
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)3, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$398
    RDB$ACL                         9:d2c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$399
    RDB$ACL                         9:d2e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$4
    RDB$ACL                         9:3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$40
    RDB$ACL                         9:27
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$400
    RDB$ACL                         9:d30
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$401
    RDB$ACL                         9:d32
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$402
    RDB$ACL                         9:d34
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$403
    RDB$ACL                         9:d36
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$404
    RDB$ACL                         9:d38
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$405
    RDB$ACL                         9:d3a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$406
    RDB$ACL                         9:d3c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$407
    RDB$ACL                         9:d3e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$408
    RDB$ACL                         9:d40
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$409
    RDB$ACL                         9:d42
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$41
    RDB$ACL                         9:28
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$410
    RDB$ACL                         9:d44
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$411
    RDB$ACL                         9:d46
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$412
    RDB$ACL                         9:d48
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$413
    RDB$ACL                         9:d4a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$414
    RDB$ACL                         9:d4c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$415
    RDB$ACL                         9:d4e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$416
    RDB$ACL                         9:d50
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$417
    RDB$ACL                         9:d52
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$418
    RDB$ACL                         9:d80
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)4, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$419
    RDB$ACL                         9:d56
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$42
    RDB$ACL                         9:29
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$420
    RDB$ACL                         9:d58
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$421
    RDB$ACL                         9:d5a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$422
    RDB$ACL                         9:d5c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$423
    RDB$ACL                         9:d5e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$424
    RDB$ACL                         9:d60
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$425
    RDB$ACL                         9:d62
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$426
    RDB$ACL                         9:d64
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$427
    RDB$ACL                         9:d66
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$428
    RDB$ACL                         9:d68
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$429
    RDB$ACL                         9:d6a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$43
    RDB$ACL                         9:2a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$430
    RDB$ACL                         9:d6c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$431
    RDB$ACL                         9:d6e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$432
    RDB$ACL                         9:d70
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$433
    RDB$ACL                         9:d2b
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)22, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$434
    RDB$ACL                         9:d74
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$435
    RDB$ACL                         9:d76
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$436
    RDB$ACL                         9:d78
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$437
    RDB$ACL                         9:d7a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$438
    RDB$ACL                         9:d7c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$439
    RDB$ACL                         9:d7e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$44
    RDB$ACL                         9:2b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$45
    RDB$ACL                         9:2c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$46
    RDB$ACL                         9:2d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$47
    RDB$ACL                         9:2e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$48
    RDB$ACL                         9:2f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$49
    RDB$ACL                         9:30
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$5
    RDB$ACL                         9:4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$50
    RDB$ACL                         9:31
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$51
    RDB$ACL                         9:32
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$52
    RDB$ACL                         9:33
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$53
    RDB$ACL                         9:34
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$54
    RDB$ACL                         9:35
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$55
    RDB$ACL                         9:36
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$56
    RDB$ACL                         9:37
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$57
    RDB$ACL                         9:38
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$58
    RDB$ACL                         9:39
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$59
    RDB$ACL                         9:3a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$6
    RDB$ACL                         9:5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$60
    RDB$ACL                         9:3b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$61
    RDB$ACL                         9:3c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$62
    RDB$ACL                         9:3d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$63
    RDB$ACL                         9:3e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$64
    RDB$ACL                         9:3f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$65
    RDB$ACL                         9:40
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$66
    RDB$ACL                         9:41
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$67
    RDB$ACL                         9:42
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$68
    RDB$ACL                         9:43
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$69
    RDB$ACL                         9:44
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$7
    RDB$ACL                         9:6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$70
    RDB$ACL                         9:45
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$71
    RDB$ACL                         9:46
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$72
    RDB$ACL                         9:47
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$73
    RDB$ACL                         9:48
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$74
    RDB$ACL                         9:49
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$75
    RDB$ACL                         9:4a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$76
    RDB$ACL                         9:4b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$77
    RDB$ACL                         9:4c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$78
    RDB$ACL                         9:4d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$79
    RDB$ACL                         9:4e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$8
    RDB$ACL                         9:7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$80
    RDB$ACL                         9:4f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$81
    RDB$ACL                         9:50
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$82
    RDB$ACL                         9:51
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$83
    RDB$ACL                         9:52
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$84
    RDB$ACL                         9:53
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$85
    RDB$ACL                         9:54
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$86
    RDB$ACL                         9:55
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$87
    RDB$ACL                         9:56
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$88
    RDB$ACL                         9:57
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$89
    RDB$ACL                         9:58
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$9
    RDB$ACL                         9:8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$90
    RDB$ACL                         9:59
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$91
    RDB$ACL                         9:5a
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$92
    RDB$ACL                         9:5b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$93
    RDB$ACL                         9:5c
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$94
    RDB$ACL                         9:5d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$95
    RDB$ACL                         9:5e
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$96
    RDB$ACL                         9:5f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$97
    RDB$ACL                         9:60
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$98
    RDB$ACL                         9:61
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$99
    RDB$ACL                         9:62
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, usage)
            		all users: (*.*), privileges: (usage)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$CHARSETS
    RDB$ACL                         9:9d9
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$COLLATIONS
    RDB$ACL                         9:9da
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT1
    RDB$ACL                         9:9de
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT10
    RDB$ACL                         9:d27
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT11
    RDB$ACL                         9:d29
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT12
    RDB$ACL                         9:d2a
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)3, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT13
    RDB$ACL                         9:d2d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT14
    RDB$ACL                         9:d2f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT15
    RDB$ACL                         9:d31
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT16
    RDB$ACL                         9:d33
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT17
    RDB$ACL                         9:d35
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT18
    RDB$ACL                         9:d37
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT19
    RDB$ACL                         9:d39
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT2
    RDB$ACL                         9:9e0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT20
    RDB$ACL                         9:d3b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT21
    RDB$ACL                         9:d3d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT22
    RDB$ACL                         9:d3f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT23
    RDB$ACL                         9:d41
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT24
    RDB$ACL                         9:d43
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT25
    RDB$ACL                         9:d45
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT26
    RDB$ACL                         9:d47
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT27
    RDB$ACL                         9:d49
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT28
    RDB$ACL                         9:d4b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT29
    RDB$ACL                         9:d4d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT3
    RDB$ACL                         9:9e2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT30
    RDB$ACL                         9:d4f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT31
    RDB$ACL                         9:d51
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT32
    RDB$ACL                         9:d53
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT33
    RDB$ACL                         9:d54
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)4, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT34
    RDB$ACL                         9:d57
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT35
    RDB$ACL                         9:d59
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT36
    RDB$ACL                         9:d5b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT37
    RDB$ACL                         9:d5d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT38
    RDB$ACL                         9:d5f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT39
    RDB$ACL                         9:d61
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT4
    RDB$ACL                         9:9e4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT40
    RDB$ACL                         9:d63
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT41
    RDB$ACL                         9:d65
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT42
    RDB$ACL                         9:d67
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT43
    RDB$ACL                         9:d69
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT44
    RDB$ACL                         9:d6b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT45
    RDB$ACL                         9:d6d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT46
    RDB$ACL                         9:d6f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT47
    RDB$ACL                         9:d71
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT48
    RDB$ACL                         9:d72
            	ACL version 1
            		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
            		(null)22, privileges: (insert, update, delete, select, references)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT49
    RDB$ACL                         9:d75
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT5
    RDB$ACL                         9:9e6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT50
    RDB$ACL                         9:d77
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT51
    RDB$ACL                         9:d79
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT52
    RDB$ACL                         9:d7b
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT53
    RDB$ACL                         9:d7d
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT54
    RDB$ACL                         9:d7f
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT6
    RDB$ACL                         9:9e8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT7
    RDB$ACL                         9:d21
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT8
    RDB$ACL                         9:d23
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DEFAULT9
    RDB$ACL                         9:d25
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
            		all users: (*.*), privileges: (select)

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$DOMAINS
    RDB$ACL                         9:9d6
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$EXCEPTIONS
    RDB$ACL                         9:9d7
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$FILTERS
    RDB$ACL                         9:9db
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$FUNCTIONS
    RDB$ACL                         9:9d3
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$GENERATORS
    RDB$ACL                         9:9d5
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$PACKAGES
    RDB$ACL                         9:9d4
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$PROCEDURES
    RDB$ACL                         9:9d2
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$ROLES
    RDB$ACL                         9:9d8
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$TABLES
    RDB$ACL                         9:9d0
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>

    RDB$SECURITY_CLASS              SQL$VIEWS
    RDB$ACL                         9:9d1
            	ACL version 1
            		person: SYSDBA, privileges: (control, alter, drop)
            		all users: (*.*), privileges: ()

    RDB$DESCRIPTION                 <null>


    Records affected: 505
"""

@pytest.mark.version('>=4.0,<5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

# version: 5.0

expected_stdout_3 = """

RDB$SECURITY_CLASS              SQL$1                                                                                                                                                                                                                                                       
RDB$ACL                         9:0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$10                                                                                                                                                                                                                                                      
RDB$ACL                         9:9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$100                                                                                                                                                                                                                                                     
RDB$ACL                         9:63
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$101                                                                                                                                                                                                                                                     
RDB$ACL                         9:64
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$102                                                                                                                                                                                                                                                     
RDB$ACL                         9:65
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$103                                                                                                                                                                                                                                                     
RDB$ACL                         9:66
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$104                                                                                                                                                                                                                                                     
RDB$ACL                         9:67
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$105                                                                                                                                                                                                                                                     
RDB$ACL                         9:68
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$106                                                                                                                                                                                                                                                     
RDB$ACL                         9:69
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$107                                                                                                                                                                                                                                                     
RDB$ACL                         9:6a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$108                                                                                                                                                                                                                                                     
RDB$ACL                         9:6b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$109                                                                                                                                                                                                                                                     
RDB$ACL                         9:6c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$11                                                                                                                                                                                                                                                      
RDB$ACL                         9:a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$110                                                                                                                                                                                                                                                     
RDB$ACL                         9:6d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$111                                                                                                                                                                                                                                                     
RDB$ACL                         9:6e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$112                                                                                                                                                                                                                                                     
RDB$ACL                         9:6f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$113                                                                                                                                                                                                                                                     
RDB$ACL                         9:70
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$114                                                                                                                                                                                                                                                     
RDB$ACL                         9:71
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$115                                                                                                                                                                                                                                                     
RDB$ACL                         9:72
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$116                                                                                                                                                                                                                                                     
RDB$ACL                         9:73
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$117                                                                                                                                                                                                                                                     
RDB$ACL                         9:74
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$118                                                                                                                                                                                                                                                     
RDB$ACL                         9:75
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$119                                                                                                                                                                                                                                                     
RDB$ACL                         9:76
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$12                                                                                                                                                                                                                                                      
RDB$ACL                         9:b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$120                                                                                                                                                                                                                                                     
RDB$ACL                         9:77
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$121                                                                                                                                                                                                                                                     
RDB$ACL                         9:78
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$122                                                                                                                                                                                                                                                     
RDB$ACL                         9:79
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$123                                                                                                                                                                                                                                                     
RDB$ACL                         9:7a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$124                                                                                                                                                                                                                                                     
RDB$ACL                         9:7b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$125                                                                                                                                                                                                                                                     
RDB$ACL                         9:7c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$126                                                                                                                                                                                                                                                     
RDB$ACL                         9:7d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$127                                                                                                                                                                                                                                                     
RDB$ACL                         9:7e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$128                                                                                                                                                                                                                                                     
RDB$ACL                         9:7f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$129                                                                                                                                                                                                                                                     
RDB$ACL                         9:80
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$13                                                                                                                                                                                                                                                      
RDB$ACL                         9:c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$130                                                                                                                                                                                                                                                     
RDB$ACL                         9:81
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$131                                                                                                                                                                                                                                                     
RDB$ACL                         9:82
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$132                                                                                                                                                                                                                                                     
RDB$ACL                         9:83
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$133                                                                                                                                                                                                                                                     
RDB$ACL                         9:84
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$134                                                                                                                                                                                                                                                     
RDB$ACL                         9:85
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$135                                                                                                                                                                                                                                                     
RDB$ACL                         9:86
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$136                                                                                                                                                                                                                                                     
RDB$ACL                         9:87
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$137                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$138                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$139                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$14                                                                                                                                                                                                                                                      
RDB$ACL                         9:d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$140                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$141                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$142                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$143                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$144                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$145                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$146                                                                                                                                                                                                                                                     
RDB$ACL                         9:5a9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$147                                                                                                                                                                                                                                                     
RDB$ACL                         9:5aa
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$148                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ab
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$149                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ac
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$15                                                                                                                                                                                                                                                      
RDB$ACL                         9:e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$150                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ad
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$151                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ae
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$152                                                                                                                                                                                                                                                     
RDB$ACL                         9:5af
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$153                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$154                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$155                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$156                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$157                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$158                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$159                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$16                                                                                                                                                                                                                                                      
RDB$ACL                         9:f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$160                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$161                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$162                                                                                                                                                                                                                                                     
RDB$ACL                         9:5b9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$163                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ba
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$164                                                                                                                                                                                                                                                     
RDB$ACL                         9:5bb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$165                                                                                                                                                                                                                                                     
RDB$ACL                         9:5bc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$166                                                                                                                                                                                                                                                     
RDB$ACL                         9:5bd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$167                                                                                                                                                                                                                                                     
RDB$ACL                         9:5be
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$168                                                                                                                                                                                                                                                     
RDB$ACL                         9:5bf
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$169                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$17                                                                                                                                                                                                                                                      
RDB$ACL                         9:10
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$170                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$171                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$172                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$173                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$174                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$175                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$176                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$177                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$178                                                                                                                                                                                                                                                     
RDB$ACL                         9:5c9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$179                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ca
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$18                                                                                                                                                                                                                                                      
RDB$ACL                         9:11
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$180                                                                                                                                                                                                                                                     
RDB$ACL                         9:5cb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$181                                                                                                                                                                                                                                                     
RDB$ACL                         9:5cc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$182                                                                                                                                                                                                                                                     
RDB$ACL                         9:5cd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$183                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ce
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$184                                                                                                                                                                                                                                                     
RDB$ACL                         9:5cf
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$185                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$186                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$187                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$188                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$189                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$19                                                                                                                                                                                                                                                      
RDB$ACL                         9:12
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$190                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$191                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$192                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$193                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$194                                                                                                                                                                                                                                                     
RDB$ACL                         9:5d9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$195                                                                                                                                                                                                                                                     
RDB$ACL                         9:5da
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$196                                                                                                                                                                                                                                                     
RDB$ACL                         9:5db
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$197                                                                                                                                                                                                                                                     
RDB$ACL                         9:5dc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$198                                                                                                                                                                                                                                                     
RDB$ACL                         9:5dd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$199                                                                                                                                                                                                                                                     
RDB$ACL                         9:5de
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$2                                                                                                                                                                                                                                                       
RDB$ACL                         9:1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$20                                                                                                                                                                                                                                                      
RDB$ACL                         9:13
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$200                                                                                                                                                                                                                                                     
RDB$ACL                         9:5df
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$201                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$202                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$203                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$204                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$205                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$206                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$207                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$208                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$209                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$21                                                                                                                                                                                                                                                      
RDB$ACL                         9:14
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$210                                                                                                                                                                                                                                                     
RDB$ACL                         9:5e9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$211                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ea
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$212                                                                                                                                                                                                                                                     
RDB$ACL                         9:5eb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$213                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ec
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$214                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ed
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$215                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ee
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$216                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ef
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$217                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$218                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$219                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$22                                                                                                                                                                                                                                                      
RDB$ACL                         9:15
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$220                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$221                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$222                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$223                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$224                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$225                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$226                                                                                                                                                                                                                                                     
RDB$ACL                         9:5f9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$227                                                                                                                                                                                                                                                     
RDB$ACL                         9:5fa
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$228                                                                                                                                                                                                                                                     
RDB$ACL                         9:5fb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$229                                                                                                                                                                                                                                                     
RDB$ACL                         9:5fc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$23                                                                                                                                                                                                                                                      
RDB$ACL                         9:16
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$230                                                                                                                                                                                                                                                     
RDB$ACL                         9:5fd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$231                                                                                                                                                                                                                                                     
RDB$ACL                         9:5fe
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$232                                                                                                                                                                                                                                                     
RDB$ACL                         9:5ff
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$233                                                                                                                                                                                                                                                     
RDB$ACL                         9:600
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$234                                                                                                                                                                                                                                                     
RDB$ACL                         9:601
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$235                                                                                                                                                                                                                                                     
RDB$ACL                         9:602
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$236                                                                                                                                                                                                                                                     
RDB$ACL                         9:603
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$237                                                                                                                                                                                                                                                     
RDB$ACL                         9:604
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$238                                                                                                                                                                                                                                                     
RDB$ACL                         9:605
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$239                                                                                                                                                                                                                                                     
RDB$ACL                         9:606
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$24                                                                                                                                                                                                                                                      
RDB$ACL                         9:17
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$240                                                                                                                                                                                                                                                     
RDB$ACL                         9:607
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$241                                                                                                                                                                                                                                                     
RDB$ACL                         9:608
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$242                                                                                                                                                                                                                                                     
RDB$ACL                         9:609
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$243                                                                                                                                                                                                                                                     
RDB$ACL                         9:60a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$244                                                                                                                                                                                                                                                     
RDB$ACL                         9:60b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$245                                                                                                                                                                                                                                                     
RDB$ACL                         9:60c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$246                                                                                                                                                                                                                                                     
RDB$ACL                         9:60d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$247                                                                                                                                                                                                                                                     
RDB$ACL                         9:60e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$248                                                                                                                                                                                                                                                     
RDB$ACL                         9:60f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$249                                                                                                                                                                                                                                                     
RDB$ACL                         9:610
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$25                                                                                                                                                                                                                                                      
RDB$ACL                         9:18
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$250                                                                                                                                                                                                                                                     
RDB$ACL                         9:611
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$251                                                                                                                                                                                                                                                     
RDB$ACL                         9:612
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$252                                                                                                                                                                                                                                                     
RDB$ACL                         9:613
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$253                                                                                                                                                                                                                                                     
RDB$ACL                         9:614
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$254                                                                                                                                                                                                                                                     
RDB$ACL                         9:615
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$255                                                                                                                                                                                                                                                     
RDB$ACL                         9:616
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$256                                                                                                                                                                                                                                                     
RDB$ACL                         9:617
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$257                                                                                                                                                                                                                                                     
RDB$ACL                         9:618
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$258                                                                                                                                                                                                                                                     
RDB$ACL                         9:619
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$259                                                                                                                                                                                                                                                     
RDB$ACL                         9:61a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$26                                                                                                                                                                                                                                                      
RDB$ACL                         9:19
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$260                                                                                                                                                                                                                                                     
RDB$ACL                         9:61b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$261                                                                                                                                                                                                                                                     
RDB$ACL                         9:61c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$262                                                                                                                                                                                                                                                     
RDB$ACL                         9:61d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$263                                                                                                                                                                                                                                                     
RDB$ACL                         9:61e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$264                                                                                                                                                                                                                                                     
RDB$ACL                         9:61f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$265                                                                                                                                                                                                                                                     
RDB$ACL                         9:620
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$266                                                                                                                                                                                                                                                     
RDB$ACL                         9:621
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$267                                                                                                                                                                                                                                                     
RDB$ACL                         9:622
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$268                                                                                                                                                                                                                                                     
RDB$ACL                         9:623
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$269                                                                                                                                                                                                                                                     
RDB$ACL                         9:624
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$27                                                                                                                                                                                                                                                      
RDB$ACL                         9:1a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$270                                                                                                                                                                                                                                                     
RDB$ACL                         9:625
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$271                                                                                                                                                                                                                                                     
RDB$ACL                         9:626
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$272                                                                                                                                                                                                                                                     
RDB$ACL                         9:627
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$273                                                                                                                                                                                                                                                     
RDB$ACL                         9:960
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$274                                                                                                                                                                                                                                                     
RDB$ACL                         9:961
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$275                                                                                                                                                                                                                                                     
RDB$ACL                         9:962
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$276                                                                                                                                                                                                                                                     
RDB$ACL                         9:963
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$277                                                                                                                                                                                                                                                     
RDB$ACL                         9:964
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$278                                                                                                                                                                                                                                                     
RDB$ACL                         9:965
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$279                                                                                                                                                                                                                                                     
RDB$ACL                         9:966
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$28                                                                                                                                                                                                                                                      
RDB$ACL                         9:1b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$280                                                                                                                                                                                                                                                     
RDB$ACL                         9:967
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$281                                                                                                                                                                                                                                                     
RDB$ACL                         9:968
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$282                                                                                                                                                                                                                                                     
RDB$ACL                         9:969
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$283                                                                                                                                                                                                                                                     
RDB$ACL                         9:96a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$284                                                                                                                                                                                                                                                     
RDB$ACL                         9:96b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$285                                                                                                                                                                                                                                                     
RDB$ACL                         9:96c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$286                                                                                                                                                                                                                                                     
RDB$ACL                         9:96d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$287                                                                                                                                                                                                                                                     
RDB$ACL                         9:96e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$288                                                                                                                                                                                                                                                     
RDB$ACL                         9:96f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$289                                                                                                                                                                                                                                                     
RDB$ACL                         9:970
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$29                                                                                                                                                                                                                                                      
RDB$ACL                         9:1c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$290                                                                                                                                                                                                                                                     
RDB$ACL                         9:971
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$291                                                                                                                                                                                                                                                     
RDB$ACL                         9:972
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$292                                                                                                                                                                                                                                                     
RDB$ACL                         9:973
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$293                                                                                                                                                                                                                                                     
RDB$ACL                         9:974
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$294                                                                                                                                                                                                                                                     
RDB$ACL                         9:975
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$295                                                                                                                                                                                                                                                     
RDB$ACL                         9:976
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$296                                                                                                                                                                                                                                                     
RDB$ACL                         9:977
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$297                                                                                                                                                                                                                                                     
RDB$ACL                         9:978
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$298                                                                                                                                                                                                                                                     
RDB$ACL                         9:979
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$299                                                                                                                                                                                                                                                     
RDB$ACL                         9:97a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$3                                                                                                                                                                                                                                                       
RDB$ACL                         9:2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$30                                                                                                                                                                                                                                                      
RDB$ACL                         9:1d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$300                                                                                                                                                                                                                                                     
RDB$ACL                         9:97b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$301                                                                                                                                                                                                                                                     
RDB$ACL                         9:97c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$302                                                                                                                                                                                                                                                     
RDB$ACL                         9:97d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$303                                                                                                                                                                                                                                                     
RDB$ACL                         9:97e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$304                                                                                                                                                                                                                                                     
RDB$ACL                         9:97f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$305                                                                                                                                                                                                                                                     
RDB$ACL                         9:980
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$306                                                                                                                                                                                                                                                     
RDB$ACL                         9:981
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$307                                                                                                                                                                                                                                                     
RDB$ACL                         9:982
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$308                                                                                                                                                                                                                                                     
RDB$ACL                         9:983
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$309                                                                                                                                                                                                                                                     
RDB$ACL                         9:984
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$31                                                                                                                                                                                                                                                      
RDB$ACL                         9:1e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$310                                                                                                                                                                                                                                                     
RDB$ACL                         9:985
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$311                                                                                                                                                                                                                                                     
RDB$ACL                         9:986
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$312                                                                                                                                                                                                                                                     
RDB$ACL                         9:987
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$313                                                                                                                                                                                                                                                     
RDB$ACL                         9:988
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$314                                                                                                                                                                                                                                                     
RDB$ACL                         9:989
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$315                                                                                                                                                                                                                                                     
RDB$ACL                         9:98a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$316                                                                                                                                                                                                                                                     
RDB$ACL                         9:98b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$317                                                                                                                                                                                                                                                     
RDB$ACL                         9:98c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$318                                                                                                                                                                                                                                                     
RDB$ACL                         9:98d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$319                                                                                                                                                                                                                                                     
RDB$ACL                         9:98e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$32                                                                                                                                                                                                                                                      
RDB$ACL                         9:1f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$320                                                                                                                                                                                                                                                     
RDB$ACL                         9:98f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$321                                                                                                                                                                                                                                                     
RDB$ACL                         9:990
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$322                                                                                                                                                                                                                                                     
RDB$ACL                         9:991
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$323                                                                                                                                                                                                                                                     
RDB$ACL                         9:992
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$324                                                                                                                                                                                                                                                     
RDB$ACL                         9:993
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$325                                                                                                                                                                                                                                                     
RDB$ACL                         9:994
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$326                                                                                                                                                                                                                                                     
RDB$ACL                         9:995
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$327                                                                                                                                                                                                                                                     
RDB$ACL                         9:996
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$328                                                                                                                                                                                                                                                     
RDB$ACL                         9:997
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$329                                                                                                                                                                                                                                                     
RDB$ACL                         9:998
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$33                                                                                                                                                                                                                                                      
RDB$ACL                         9:20
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$330                                                                                                                                                                                                                                                     
RDB$ACL                         9:999
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$331                                                                                                                                                                                                                                                     
RDB$ACL                         9:99a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$332                                                                                                                                                                                                                                                     
RDB$ACL                         9:99b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$333                                                                                                                                                                                                                                                     
RDB$ACL                         9:99c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$334                                                                                                                                                                                                                                                     
RDB$ACL                         9:99d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$335                                                                                                                                                                                                                                                     
RDB$ACL                         9:99e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$336                                                                                                                                                                                                                                                     
RDB$ACL                         9:99f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$337                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$338                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$339                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$34                                                                                                                                                                                                                                                      
RDB$ACL                         9:21
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$340                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$341                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$342                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$343                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$344                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$345                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$346                                                                                                                                                                                                                                                     
RDB$ACL                         9:9a9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$347                                                                                                                                                                                                                                                     
RDB$ACL                         9:9aa
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$348                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ab
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$349                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ac
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$35                                                                                                                                                                                                                                                      
RDB$ACL                         9:22
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$350                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ad
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$351                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ae
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$352                                                                                                                                                                                                                                                     
RDB$ACL                         9:9af
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$353                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$354                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$355                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$356                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$357                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$358                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$359                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$36                                                                                                                                                                                                                                                      
RDB$ACL                         9:23
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$360                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$361                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$362                                                                                                                                                                                                                                                     
RDB$ACL                         9:9b9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$363                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ba
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$364                                                                                                                                                                                                                                                     
RDB$ACL                         9:9bb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$365                                                                                                                                                                                                                                                     
RDB$ACL                         9:9bc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$366                                                                                                                                                                                                                                                     
RDB$ACL                         9:9bd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$367                                                                                                                                                                                                                                                     
RDB$ACL                         9:9be
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$368                                                                                                                                                                                                                                                     
RDB$ACL                         9:9bf
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$369                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$37                                                                                                                                                                                                                                                      
RDB$ACL                         9:24
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$370                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$371                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$372                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$373                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$374                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$375                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$376                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$377                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$378                                                                                                                                                                                                                                                     
RDB$ACL                         9:9c9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$379                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ca
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$38                                                                                                                                                                                                                                                      
RDB$ACL                         9:25
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$380                                                                                                                                                                                                                                                     
RDB$ACL                         9:9cb
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$381                                                                                                                                                                                                                                                     
RDB$ACL                         9:9cc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$382                                                                                                                                                                                                                                                     
RDB$ACL                         9:9cd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$383                                                                                                                                                                                                                                                     
RDB$ACL                         9:9ce
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$384                                                                                                                                                                                                                                                     
RDB$ACL                         9:9cf
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$385                                                                                                                                                                                                                                                     
RDB$ACL                         9:9d0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$386                                                                                                                                                                                                                                                     
RDB$ACL                         9:9d1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (execute)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$387                                                                                                                                                                                                                                                     
RDB$ACL                         9:9e0
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$388                                                                                                                                                                                                                                                     
RDB$ACL                         9:9e1
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$389                                                                                                                                                                                                                                                     
RDB$ACL                         9:9e3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$39                                                                                                                                                                                                                                                      
RDB$ACL                         9:26
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$390                                                                                                                                                                                                                                                     
RDB$ACL                         9:9e5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$391                                                                                                                                                                                                                                                     
RDB$ACL                         9:9e7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$392                                                                                                                                                                                                                                                     
RDB$ACL                         9:d20
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$393                                                                                                                                                                                                                                                     
RDB$ACL                         9:d22
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$394                                                                                                                                                                                                                                                     
RDB$ACL                         9:d24
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$395                                                                                                                                                                                                                                                     
RDB$ACL                         9:d26
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$396                                                                                                                                                                                                                                                     
RDB$ACL                         9:d28
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$397                                                                                                                                                                                                                                                     
RDB$ACL                         9:d2a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$398                                                                                                                                                                                                                                                     
RDB$ACL                         9:d2c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$399                                                                                                                                                                                                                                                     
RDB$ACL                         9:d59
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)3, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$4                                                                                                                                                                                                                                                       
RDB$ACL                         9:3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$40                                                                                                                                                                                                                                                      
RDB$ACL                         9:27
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$400                                                                                                                                                                                                                                                     
RDB$ACL                         9:d30
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$401                                                                                                                                                                                                                                                     
RDB$ACL                         9:d32
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$402                                                                                                                                                                                                                                                     
RDB$ACL                         9:d34
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$403                                                                                                                                                                                                                                                     
RDB$ACL                         9:d36
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$404                                                                                                                                                                                                                                                     
RDB$ACL                         9:d38
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$405                                                                                                                                                                                                                                                     
RDB$ACL                         9:d3a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$406                                                                                                                                                                                                                                                     
RDB$ACL                         9:d3c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$407                                                                                                                                                                                                                                                     
RDB$ACL                         9:d3e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$408                                                                                                                                                                                                                                                     
RDB$ACL                         9:d40
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$409                                                                                                                                                                                                                                                     
RDB$ACL                         9:d42
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$41                                                                                                                                                                                                                                                      
RDB$ACL                         9:28
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$410                                                                                                                                                                                                                                                     
RDB$ACL                         9:d44
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$411                                                                                                                                                                                                                                                     
RDB$ACL                         9:d46
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$412                                                                                                                                                                                                                                                     
RDB$ACL                         9:d48
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$413                                                                                                                                                                                                                                                     
RDB$ACL                         9:d4a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$414                                                                                                                                                                                                                                                     
RDB$ACL                         9:d4c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$415                                                                                                                                                                                                                                                     
RDB$ACL                         9:d4e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$416                                                                                                                                                                                                                                                     
RDB$ACL                         9:d50
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$417                                                                                                                                                                                                                                                     
RDB$ACL                         9:d52
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$418                                                                                                                                                                                                                                                     
RDB$ACL                         9:d54
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$419                                                                                                                                                                                                                                                     
RDB$ACL                         9:d56
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$42                                                                                                                                                                                                                                                      
RDB$ACL                         9:29
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$420                                                                                                                                                                                                                                                     
RDB$ACL                         9:d88
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)4, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$421                                                                                                                                                                                                                                                     
RDB$ACL                         9:d5a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$422                                                                                                                                                                                                                                                     
RDB$ACL                         9:d5c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$423                                                                                                                                                                                                                                                     
RDB$ACL                         9:d5e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$424                                                                                                                                                                                                                                                     
RDB$ACL                         9:d60
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$425                                                                                                                                                                                                                                                     
RDB$ACL                         9:d62
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$426                                                                                                                                                                                                                                                     
RDB$ACL                         9:d64
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$427                                                                                                                                                                                                                                                     
RDB$ACL                         9:d66
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$428                                                                                                                                                                                                                                                     
RDB$ACL                         9:d68
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$429                                                                                                                                                                                                                                                     
RDB$ACL                         9:d6a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$43                                                                                                                                                                                                                                                      
RDB$ACL                         9:2a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$430                                                                                                                                                                                                                                                     
RDB$ACL                         9:d6c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$431                                                                                                                                                                                                                                                     
RDB$ACL                         9:d6e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$432                                                                                                                                                                                                                                                     
RDB$ACL                         9:d70
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$433                                                                                                                                                                                                                                                     
RDB$ACL                         9:d72
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$434                                                                                                                                                                                                                                                     
RDB$ACL                         9:d74
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$435                                                                                                                                                                                                                                                     
RDB$ACL                         9:d2f
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)22, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$436                                                                                                                                                                                                                                                     
RDB$ACL                         9:d78
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$437                                                                                                                                                                                                                                                     
RDB$ACL                         9:d7a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$438                                                                                                                                                                                                                                                     
RDB$ACL                         9:d7c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$439                                                                                                                                                                                                                                                     
RDB$ACL                         9:d7e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$44                                                                                                                                                                                                                                                      
RDB$ACL                         9:2b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$440                                                                                                                                                                                                                                                     
RDB$ACL                         9:d80
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$441                                                                                                                                                                                                                                                     
RDB$ACL                         9:d82
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$442                                                                                                                                                                                                                                                     
RDB$ACL                         9:d84
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$443                                                                                                                                                                                                                                                     
RDB$ACL                         9:d86
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$45                                                                                                                                                                                                                                                      
RDB$ACL                         9:2c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$46                                                                                                                                                                                                                                                      
RDB$ACL                         9:2d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$47                                                                                                                                                                                                                                                      
RDB$ACL                         9:2e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$48                                                                                                                                                                                                                                                      
RDB$ACL                         9:2f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$49                                                                                                                                                                                                                                                      
RDB$ACL                         9:30
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$5                                                                                                                                                                                                                                                       
RDB$ACL                         9:4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$50                                                                                                                                                                                                                                                      
RDB$ACL                         9:31
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$51                                                                                                                                                                                                                                                      
RDB$ACL                         9:32
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$52                                                                                                                                                                                                                                                      
RDB$ACL                         9:33
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$53                                                                                                                                                                                                                                                      
RDB$ACL                         9:34
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$54                                                                                                                                                                                                                                                      
RDB$ACL                         9:35
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$55                                                                                                                                                                                                                                                      
RDB$ACL                         9:36
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$56                                                                                                                                                                                                                                                      
RDB$ACL                         9:37
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$57                                                                                                                                                                                                                                                      
RDB$ACL                         9:38
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$58                                                                                                                                                                                                                                                      
RDB$ACL                         9:39
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$59                                                                                                                                                                                                                                                      
RDB$ACL                         9:3a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$6                                                                                                                                                                                                                                                       
RDB$ACL                         9:5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$60                                                                                                                                                                                                                                                      
RDB$ACL                         9:3b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$61                                                                                                                                                                                                                                                      
RDB$ACL                         9:3c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$62                                                                                                                                                                                                                                                      
RDB$ACL                         9:3d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$63                                                                                                                                                                                                                                                      
RDB$ACL                         9:3e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$64                                                                                                                                                                                                                                                      
RDB$ACL                         9:3f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$65                                                                                                                                                                                                                                                      
RDB$ACL                         9:40
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$66                                                                                                                                                                                                                                                      
RDB$ACL                         9:41
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$67                                                                                                                                                                                                                                                      
RDB$ACL                         9:42
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$68                                                                                                                                                                                                                                                      
RDB$ACL                         9:43
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$69                                                                                                                                                                                                                                                      
RDB$ACL                         9:44
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$7                                                                                                                                                                                                                                                       
RDB$ACL                         9:6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$70                                                                                                                                                                                                                                                      
RDB$ACL                         9:45
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$71                                                                                                                                                                                                                                                      
RDB$ACL                         9:46
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$72                                                                                                                                                                                                                                                      
RDB$ACL                         9:47
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$73                                                                                                                                                                                                                                                      
RDB$ACL                         9:48
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$74                                                                                                                                                                                                                                                      
RDB$ACL                         9:49
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$75                                                                                                                                                                                                                                                      
RDB$ACL                         9:4a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$76                                                                                                                                                                                                                                                      
RDB$ACL                         9:4b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$77                                                                                                                                                                                                                                                      
RDB$ACL                         9:4c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$78                                                                                                                                                                                                                                                      
RDB$ACL                         9:4d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$79                                                                                                                                                                                                                                                      
RDB$ACL                         9:4e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$8                                                                                                                                                                                                                                                       
RDB$ACL                         9:7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$80                                                                                                                                                                                                                                                      
RDB$ACL                         9:4f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$81                                                                                                                                                                                                                                                      
RDB$ACL                         9:50
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$82                                                                                                                                                                                                                                                      
RDB$ACL                         9:51
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$83                                                                                                                                                                                                                                                      
RDB$ACL                         9:52
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$84                                                                                                                                                                                                                                                      
RDB$ACL                         9:53
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$85                                                                                                                                                                                                                                                      
RDB$ACL                         9:54
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$86                                                                                                                                                                                                                                                      
RDB$ACL                         9:55
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$87                                                                                                                                                                                                                                                      
RDB$ACL                         9:56
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$88                                                                                                                                                                                                                                                      
RDB$ACL                         9:57
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$89                                                                                                                                                                                                                                                      
RDB$ACL                         9:58
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$9                                                                                                                                                                                                                                                       
RDB$ACL                         9:8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$90                                                                                                                                                                                                                                                      
RDB$ACL                         9:59
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$91                                                                                                                                                                                                                                                      
RDB$ACL                         9:5a
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$92                                                                                                                                                                                                                                                      
RDB$ACL                         9:5b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$93                                                                                                                                                                                                                                                      
RDB$ACL                         9:5c
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$94                                                                                                                                                                                                                                                      
RDB$ACL                         9:5d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$95                                                                                                                                                                                                                                                      
RDB$ACL                         9:5e
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$96                                                                                                                                                                                                                                                      
RDB$ACL                         9:5f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$97                                                                                                                                                                                                                                                      
RDB$ACL                         9:60
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$98                                                                                                                                                                                                                                                      
RDB$ACL                         9:61
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$99                                                                                                                                                                                                                                                      
RDB$ACL                         9:62
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, usage)
        		all users: (*.*), privileges: (usage)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$CHARSETS                                                                                                                                                                                                                                                
RDB$ACL                         9:9db
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$COLLATIONS                                                                                                                                                                                                                                              
RDB$ACL                         9:9dc
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT1                                                                                                                                                                                                                                                
RDB$ACL                         9:9e2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT10                                                                                                                                                                                                                                               
RDB$ACL                         9:d2b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT11                                                                                                                                                                                                                                               
RDB$ACL                         9:d2d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT12                                                                                                                                                                                                                                               
RDB$ACL                         9:d2e
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)3, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT13                                                                                                                                                                                                                                               
RDB$ACL                         9:d31
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT14                                                                                                                                                                                                                                               
RDB$ACL                         9:d33
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT15                                                                                                                                                                                                                                               
RDB$ACL                         9:d35
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT16                                                                                                                                                                                                                                               
RDB$ACL                         9:d37
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT17                                                                                                                                                                                                                                               
RDB$ACL                         9:d39
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT18                                                                                                                                                                                                                                               
RDB$ACL                         9:d3b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT19                                                                                                                                                                                                                                               
RDB$ACL                         9:d3d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT2                                                                                                                                                                                                                                                
RDB$ACL                         9:9e4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT20                                                                                                                                                                                                                                               
RDB$ACL                         9:d3f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT21                                                                                                                                                                                                                                               
RDB$ACL                         9:d41
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT22                                                                                                                                                                                                                                               
RDB$ACL                         9:d43
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT23                                                                                                                                                                                                                                               
RDB$ACL                         9:d45
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT24                                                                                                                                                                                                                                               
RDB$ACL                         9:d47
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT25                                                                                                                                                                                                                                               
RDB$ACL                         9:d49
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT26                                                                                                                                                                                                                                               
RDB$ACL                         9:d4b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT27                                                                                                                                                                                                                                               
RDB$ACL                         9:d4d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT28                                                                                                                                                                                                                                               
RDB$ACL                         9:d4f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT29                                                                                                                                                                                                                                               
RDB$ACL                         9:d51
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT3                                                                                                                                                                                                                                                
RDB$ACL                         9:9e6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT30                                                                                                                                                                                                                                               
RDB$ACL                         9:d53
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT31                                                                                                                                                                                                                                               
RDB$ACL                         9:d55
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT32                                                                                                                                                                                                                                               
RDB$ACL                         9:d57
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT33                                                                                                                                                                                                                                               
RDB$ACL                         9:d58
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)4, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT34                                                                                                                                                                                                                                               
RDB$ACL                         9:d5b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT35                                                                                                                                                                                                                                               
RDB$ACL                         9:d5d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT36                                                                                                                                                                                                                                               
RDB$ACL                         9:d5f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT37                                                                                                                                                                                                                                               
RDB$ACL                         9:d61
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT38                                                                                                                                                                                                                                               
RDB$ACL                         9:d63
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT39                                                                                                                                                                                                                                               
RDB$ACL                         9:d65
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT4                                                                                                                                                                                                                                                
RDB$ACL                         9:9e8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT40                                                                                                                                                                                                                                               
RDB$ACL                         9:d67
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT41                                                                                                                                                                                                                                               
RDB$ACL                         9:d69
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT42                                                                                                                                                                                                                                               
RDB$ACL                         9:d6b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT43                                                                                                                                                                                                                                               
RDB$ACL                         9:d6d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT44                                                                                                                                                                                                                                               
RDB$ACL                         9:d6f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT45                                                                                                                                                                                                                                               
RDB$ACL                         9:d71
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT46                                                                                                                                                                                                                                               
RDB$ACL                         9:d73
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT47                                                                                                                                                                                                                                               
RDB$ACL                         9:d75
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT48                                                                                                                                                                                                                                               
RDB$ACL                         9:d76
        	ACL version 1
        		person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
        		(null)22, privileges: (insert, update, delete, select, references)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT49                                                                                                                                                                                                                                               
RDB$ACL                         9:d79
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT5                                                                                                                                                                                                                                                
RDB$ACL                         9:d21
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT50                                                                                                                                                                                                                                               
RDB$ACL                         9:d7b
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT51                                                                                                                                                                                                                                               
RDB$ACL                         9:d7d
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT52                                                                                                                                                                                                                                               
RDB$ACL                         9:d7f
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT53                                                                                                                                                                                                                                               
RDB$ACL                         9:d81
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT54                                                                                                                                                                                                                                               
RDB$ACL                         9:d83
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT55                                                                                                                                                                                                                                               
RDB$ACL                         9:d85
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT56                                                                                                                                                                                                                                               
RDB$ACL                         9:d87
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT6                                                                                                                                                                                                                                                
RDB$ACL                         9:d23
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT7                                                                                                                                                                                                                                                
RDB$ACL                         9:d25
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT8                                                                                                                                                                                                                                                
RDB$ACL                         9:d27
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DEFAULT9                                                                                                                                                                                                                                                
RDB$ACL                         9:d29
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop, select, insert, update, delete)
        		all users: (*.*), privileges: (select)

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$DOMAINS                                                                                                                                                                                                                                                 
RDB$ACL                         9:9d8
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$EXCEPTIONS                                                                                                                                                                                                                                              
RDB$ACL                         9:9d9
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$FILTERS                                                                                                                                                                                                                                                 
RDB$ACL                         9:9dd
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$FUNCTIONS                                                                                                                                                                                                                                               
RDB$ACL                         9:9d5
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$GENERATORS                                                                                                                                                                                                                                              
RDB$ACL                         9:9d7
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$JOBS                                                                                                                                                                                                                                                    
RDB$ACL                         9:9de
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$PACKAGES                                                                                                                                                                                                                                                
RDB$ACL                         9:9d6
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$PROCEDURES                                                                                                                                                                                                                                              
RDB$ACL                         9:9d4
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$ROLES                                                                                                                                                                                                                                                   
RDB$ACL                         9:9da
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$TABLES                                                                                                                                                                                                                                                  
RDB$ACL                         9:9d2
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$TABLESPACES                                                                                                                                                                                                                                             
RDB$ACL                         9:9df
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>

RDB$SECURITY_CLASS              SQL$VIEWS                                                                                                                                                                                                                                                   
RDB$ACL                         9:9d3
        	ACL version 1
        		person: SYSDBA, privileges: (control, alter, drop)
        		all users: (*.*), privileges: ()

RDB$DESCRIPTION                 <null>


Records affected: 513

"""

@pytest.mark.version('>=5.0')
@pytest.mark.skip("DISABLED: see notes")
def test_3(act: Action):
    act.expected_stdout = expected_stdout_3
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
