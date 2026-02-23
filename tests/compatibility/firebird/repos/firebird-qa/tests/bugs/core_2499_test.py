#coding:utf-8

"""
ID:          issue-2911
ISSUE:       2911
TITLE:       Implementation limit of DISTINCT items are not enforced, causing generation of incorrect BLR
DESCRIPTION:
JIRA:        CORE-2499
FBTEST:      bugs.core_2499
"""

import pytest
from firebird.qa import *

init_script = """create table t (
  n1 int, n2 int, n3 int, n4 int, n5 int, n6 int, n7 int, n8 int, n9 int, n10 int, n11 int,
  n12 int, n13 int, n14 int, n15 int, n16 int, n17 int, n18 int, n19 int, n20 int, n21 int,
  n22 int, n23 int, n24 int, n25 int, n26 int, n27 int, n28 int, n29 int, n30 int, n31 int,
  n32 int, n33 int, n34 int, n35 int, n36 int, n37 int, n38 int, n39 int, n40 int, n41 int,
  n42 int, n43 int, n44 int, n45 int, n46 int, n47 int, n48 int, n49 int, n50 int, n51 int,
  n52 int, n53 int, n54 int, n55 int, n56 int, n57 int, n58 int, n59 int, n60 int, n61 int,
  n62 int, n63 int, n64 int, n65 int, n66 int, n67 int, n68 int, n69 int, n70 int, n71 int,
  n72 int, n73 int, n74 int, n75 int, n76 int, n77 int, n78 int, n79 int, n80 int, n81 int,
  n82 int, n83 int, n84 int, n85 int, n86 int, n87 int, n88 int, n89 int, n90 int, n91 int,
  n92 int, n93 int, n94 int, n95 int, n96 int, n97 int, n98 int, n99 int, n100 int, n101 int,
  n102 int, n103 int, n104 int, n105 int, n106 int, n107 int, n108 int, n109 int, n110 int,
  n111 int, n112 int, n113 int, n114 int, n115 int, n116 int, n117 int, n118 int, n119 int,
  n120 int, n121 int, n122 int, n123 int, n124 int, n125 int, n126 int, n127 int, n128 int,
  n129 int, n130 int, n131 int, n132 int, n133 int, n134 int, n135 int, n136 int, n137 int,
  n138 int, n139 int, n140 int, n141 int, n142 int, n143 int, n144 int, n145 int, n146 int,
  n147 int, n148 int, n149 int, n150 int, n151 int, n152 int, n153 int, n154 int, n155 int,
  n156 int, n157 int, n158 int, n159 int, n160 int, n161 int, n162 int, n163 int, n164 int,
  n165 int, n166 int, n167 int, n168 int, n169 int, n170 int, n171 int, n172 int, n173 int,
  n174 int, n175 int, n176 int, n177 int, n178 int, n179 int, n180 int, n181 int, n182 int,
  n183 int, n184 int, n185 int, n186 int, n187 int, n188 int, n189 int, n190 int, n191 int,
  n192 int, n193 int, n194 int, n195 int, n196 int, n197 int, n198 int, n199 int, n200 int,
  n201 int, n202 int, n203 int, n204 int, n205 int, n206 int, n207 int, n208 int, n209 int,
  n210 int, n211 int, n212 int, n213 int, n214 int, n215 int, n216 int, n217 int, n218 int,
  n219 int, n220 int, n221 int, n222 int, n223 int, n224 int, n225 int, n226 int, n227 int,
  n228 int, n229 int, n230 int, n231 int, n232 int, n233 int, n234 int, n235 int, n236 int,
  n237 int, n238 int, n239 int, n240 int, n241 int, n242 int, n243 int, n244 int, n245 int,
  n246 int, n247 int, n248 int, n249 int, n250 int, n251 int, n252 int, n253 int, n254 int,
  n255 int, n256 int);
COMMIT;
"""

db = db_factory(charset='UTF8', init=init_script)

test_script = """select distinct * from t;"""

act = isql_act('db', test_script)

# version: 3.0

expected_stderr_1 = """Statement failed, SQLSTATE = 54011
Dynamic SQL Error
-SQL error code = -104
-Invalid command
-Cannot have more than 255 items in DISTINCT list"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr_1
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

# version: 4.0

expected_stderr_2 = """Statement failed, SQLSTATE = 54011
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid command
    -Cannot have more than 255 items in DISTINCT / UNION DISTINCT list
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stderr = expected_stderr_2
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

