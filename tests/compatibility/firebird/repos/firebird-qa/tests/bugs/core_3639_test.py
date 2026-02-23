#coding:utf-8

"""
ID:          issue-3990
ISSUE:       3990
TITLE:       Allow the use of multiple WHEN MATCHED / NOT MATCHED clauses in MERGE, as per the SQL 2008 specification
DESCRIPTION:
JIRA:        CORE-3639
FBTEST:      bugs.core_3639
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table ta(id int primary key, x int, y int);
    recreate table tb(id int primary key, x int, y int);
    commit;
    insert into ta(id, x, y) values(1, 100, 111);
    insert into ta(id, x, y) values(2, 200, 222);
    insert into ta(id, x, y) values(3, 300, 333);
    insert into ta(id, x, y) values(4, 400, 444);
    insert into ta(id, x, y) values(5, 500, 555);

    insert into tb(id, x, y) values(1, 10, 11);
    insert into tb(id, x, y) values(4, 40, 44);
    insert into tb(id, x, y) values(5, 50, 55);
    commit;

    recreate table s(id int, x int);
    commit;
    insert into s(id, x) select row_number()over(), rand()*1000000 from rdb$types;
    commit;
    recreate table t(id int primary key, x int);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    -- 1. Check ability to compile MERGE with 254 trivial `when` expressions:
    --  Batch for generating SQL with MERGE and arbitrary number of WHEN sections:
    --    @echo off
    --    set sql=%~n0.sql
    --    del %sql% 2>nul
    --    set n=254
    --    echo recreate table s(id int, x int); commit;>>%sql%
    --    echo insert into s(id, x) select row_number()over(), rand()*1000000 from rdb$types; commit;>>%sql%
    --    echo recreate table t(id int primary key, x int); commit;>>%sql%
    --
    --    echo merge into t using s on s.id = t.id>>%sql%
    --    for /l %%i in (1, 1, %n%) do (
    --       echo when NOT matched and s.id = %%i then insert values(s.id, s.x^)>>%sql%
    --    )
    --    echo ;>>%sql%
    --
    --
    --    echo merge into t using s on s.id = t.id>>%sql%
    --    for /l %%i in (1, 1, %n%) do (
    --       echo when matched and s.id = %%i then update set t.x = t.x + s.x>>%sql%
    --    )
    --    echo ;>>%sql%
    --
    --    echo rollback;>>%sql%
    --    echo set count on;>>%sql%
    --    echo select * from t;>>%sql%
    --
    --    isql localhost/3333:e30 -i %sql%

    merge into t using s on s.id = t.id
    when NOT matched and s.id = 1 then insert values(s.id, s.x)
    when NOT matched and s.id = 2 then insert values(s.id, s.x)
    when NOT matched and s.id = 3 then insert values(s.id, s.x)
    when NOT matched and s.id = 4 then insert values(s.id, s.x)
    when NOT matched and s.id = 5 then insert values(s.id, s.x)
    when NOT matched and s.id = 6 then insert values(s.id, s.x)
    when NOT matched and s.id = 7 then insert values(s.id, s.x)
    when NOT matched and s.id = 8 then insert values(s.id, s.x)
    when NOT matched and s.id = 9 then insert values(s.id, s.x)
    when NOT matched and s.id = 10 then insert values(s.id, s.x)
    when NOT matched and s.id = 11 then insert values(s.id, s.x)
    when NOT matched and s.id = 12 then insert values(s.id, s.x)
    when NOT matched and s.id = 13 then insert values(s.id, s.x)
    when NOT matched and s.id = 14 then insert values(s.id, s.x)
    when NOT matched and s.id = 15 then insert values(s.id, s.x)
    when NOT matched and s.id = 16 then insert values(s.id, s.x)
    when NOT matched and s.id = 17 then insert values(s.id, s.x)
    when NOT matched and s.id = 18 then insert values(s.id, s.x)
    when NOT matched and s.id = 19 then insert values(s.id, s.x)
    when NOT matched and s.id = 20 then insert values(s.id, s.x)
    when NOT matched and s.id = 21 then insert values(s.id, s.x)
    when NOT matched and s.id = 22 then insert values(s.id, s.x)
    when NOT matched and s.id = 23 then insert values(s.id, s.x)
    when NOT matched and s.id = 24 then insert values(s.id, s.x)
    when NOT matched and s.id = 25 then insert values(s.id, s.x)
    when NOT matched and s.id = 26 then insert values(s.id, s.x)
    when NOT matched and s.id = 27 then insert values(s.id, s.x)
    when NOT matched and s.id = 28 then insert values(s.id, s.x)
    when NOT matched and s.id = 29 then insert values(s.id, s.x)
    when NOT matched and s.id = 30 then insert values(s.id, s.x)
    when NOT matched and s.id = 31 then insert values(s.id, s.x)
    when NOT matched and s.id = 32 then insert values(s.id, s.x)
    when NOT matched and s.id = 33 then insert values(s.id, s.x)
    when NOT matched and s.id = 34 then insert values(s.id, s.x)
    when NOT matched and s.id = 35 then insert values(s.id, s.x)
    when NOT matched and s.id = 36 then insert values(s.id, s.x)
    when NOT matched and s.id = 37 then insert values(s.id, s.x)
    when NOT matched and s.id = 38 then insert values(s.id, s.x)
    when NOT matched and s.id = 39 then insert values(s.id, s.x)
    when NOT matched and s.id = 40 then insert values(s.id, s.x)
    when NOT matched and s.id = 41 then insert values(s.id, s.x)
    when NOT matched and s.id = 42 then insert values(s.id, s.x)
    when NOT matched and s.id = 43 then insert values(s.id, s.x)
    when NOT matched and s.id = 44 then insert values(s.id, s.x)
    when NOT matched and s.id = 45 then insert values(s.id, s.x)
    when NOT matched and s.id = 46 then insert values(s.id, s.x)
    when NOT matched and s.id = 47 then insert values(s.id, s.x)
    when NOT matched and s.id = 48 then insert values(s.id, s.x)
    when NOT matched and s.id = 49 then insert values(s.id, s.x)
    when NOT matched and s.id = 50 then insert values(s.id, s.x)
    when NOT matched and s.id = 51 then insert values(s.id, s.x)
    when NOT matched and s.id = 52 then insert values(s.id, s.x)
    when NOT matched and s.id = 53 then insert values(s.id, s.x)
    when NOT matched and s.id = 54 then insert values(s.id, s.x)
    when NOT matched and s.id = 55 then insert values(s.id, s.x)
    when NOT matched and s.id = 56 then insert values(s.id, s.x)
    when NOT matched and s.id = 57 then insert values(s.id, s.x)
    when NOT matched and s.id = 58 then insert values(s.id, s.x)
    when NOT matched and s.id = 59 then insert values(s.id, s.x)
    when NOT matched and s.id = 60 then insert values(s.id, s.x)
    when NOT matched and s.id = 61 then insert values(s.id, s.x)
    when NOT matched and s.id = 62 then insert values(s.id, s.x)
    when NOT matched and s.id = 63 then insert values(s.id, s.x)
    when NOT matched and s.id = 64 then insert values(s.id, s.x)
    when NOT matched and s.id = 65 then insert values(s.id, s.x)
    when NOT matched and s.id = 66 then insert values(s.id, s.x)
    when NOT matched and s.id = 67 then insert values(s.id, s.x)
    when NOT matched and s.id = 68 then insert values(s.id, s.x)
    when NOT matched and s.id = 69 then insert values(s.id, s.x)
    when NOT matched and s.id = 70 then insert values(s.id, s.x)
    when NOT matched and s.id = 71 then insert values(s.id, s.x)
    when NOT matched and s.id = 72 then insert values(s.id, s.x)
    when NOT matched and s.id = 73 then insert values(s.id, s.x)
    when NOT matched and s.id = 74 then insert values(s.id, s.x)
    when NOT matched and s.id = 75 then insert values(s.id, s.x)
    when NOT matched and s.id = 76 then insert values(s.id, s.x)
    when NOT matched and s.id = 77 then insert values(s.id, s.x)
    when NOT matched and s.id = 78 then insert values(s.id, s.x)
    when NOT matched and s.id = 79 then insert values(s.id, s.x)
    when NOT matched and s.id = 80 then insert values(s.id, s.x)
    when NOT matched and s.id = 81 then insert values(s.id, s.x)
    when NOT matched and s.id = 82 then insert values(s.id, s.x)
    when NOT matched and s.id = 83 then insert values(s.id, s.x)
    when NOT matched and s.id = 84 then insert values(s.id, s.x)
    when NOT matched and s.id = 85 then insert values(s.id, s.x)
    when NOT matched and s.id = 86 then insert values(s.id, s.x)
    when NOT matched and s.id = 87 then insert values(s.id, s.x)
    when NOT matched and s.id = 88 then insert values(s.id, s.x)
    when NOT matched and s.id = 89 then insert values(s.id, s.x)
    when NOT matched and s.id = 90 then insert values(s.id, s.x)
    when NOT matched and s.id = 91 then insert values(s.id, s.x)
    when NOT matched and s.id = 92 then insert values(s.id, s.x)
    when NOT matched and s.id = 93 then insert values(s.id, s.x)
    when NOT matched and s.id = 94 then insert values(s.id, s.x)
    when NOT matched and s.id = 95 then insert values(s.id, s.x)
    when NOT matched and s.id = 96 then insert values(s.id, s.x)
    when NOT matched and s.id = 97 then insert values(s.id, s.x)
    when NOT matched and s.id = 98 then insert values(s.id, s.x)
    when NOT matched and s.id = 99 then insert values(s.id, s.x)
    when NOT matched and s.id = 100 then insert values(s.id, s.x)
    when NOT matched and s.id = 101 then insert values(s.id, s.x)
    when NOT matched and s.id = 102 then insert values(s.id, s.x)
    when NOT matched and s.id = 103 then insert values(s.id, s.x)
    when NOT matched and s.id = 104 then insert values(s.id, s.x)
    when NOT matched and s.id = 105 then insert values(s.id, s.x)
    when NOT matched and s.id = 106 then insert values(s.id, s.x)
    when NOT matched and s.id = 107 then insert values(s.id, s.x)
    when NOT matched and s.id = 108 then insert values(s.id, s.x)
    when NOT matched and s.id = 109 then insert values(s.id, s.x)
    when NOT matched and s.id = 110 then insert values(s.id, s.x)
    when NOT matched and s.id = 111 then insert values(s.id, s.x)
    when NOT matched and s.id = 112 then insert values(s.id, s.x)
    when NOT matched and s.id = 113 then insert values(s.id, s.x)
    when NOT matched and s.id = 114 then insert values(s.id, s.x)
    when NOT matched and s.id = 115 then insert values(s.id, s.x)
    when NOT matched and s.id = 116 then insert values(s.id, s.x)
    when NOT matched and s.id = 117 then insert values(s.id, s.x)
    when NOT matched and s.id = 118 then insert values(s.id, s.x)
    when NOT matched and s.id = 119 then insert values(s.id, s.x)
    when NOT matched and s.id = 120 then insert values(s.id, s.x)
    when NOT matched and s.id = 121 then insert values(s.id, s.x)
    when NOT matched and s.id = 122 then insert values(s.id, s.x)
    when NOT matched and s.id = 123 then insert values(s.id, s.x)
    when NOT matched and s.id = 124 then insert values(s.id, s.x)
    when NOT matched and s.id = 125 then insert values(s.id, s.x)
    when NOT matched and s.id = 126 then insert values(s.id, s.x)
    when NOT matched and s.id = 127 then insert values(s.id, s.x)
    when NOT matched and s.id = 128 then insert values(s.id, s.x)
    when NOT matched and s.id = 129 then insert values(s.id, s.x)
    when NOT matched and s.id = 130 then insert values(s.id, s.x)
    when NOT matched and s.id = 131 then insert values(s.id, s.x)
    when NOT matched and s.id = 132 then insert values(s.id, s.x)
    when NOT matched and s.id = 133 then insert values(s.id, s.x)
    when NOT matched and s.id = 134 then insert values(s.id, s.x)
    when NOT matched and s.id = 135 then insert values(s.id, s.x)
    when NOT matched and s.id = 136 then insert values(s.id, s.x)
    when NOT matched and s.id = 137 then insert values(s.id, s.x)
    when NOT matched and s.id = 138 then insert values(s.id, s.x)
    when NOT matched and s.id = 139 then insert values(s.id, s.x)
    when NOT matched and s.id = 140 then insert values(s.id, s.x)
    when NOT matched and s.id = 141 then insert values(s.id, s.x)
    when NOT matched and s.id = 142 then insert values(s.id, s.x)
    when NOT matched and s.id = 143 then insert values(s.id, s.x)
    when NOT matched and s.id = 144 then insert values(s.id, s.x)
    when NOT matched and s.id = 145 then insert values(s.id, s.x)
    when NOT matched and s.id = 146 then insert values(s.id, s.x)
    when NOT matched and s.id = 147 then insert values(s.id, s.x)
    when NOT matched and s.id = 148 then insert values(s.id, s.x)
    when NOT matched and s.id = 149 then insert values(s.id, s.x)
    when NOT matched and s.id = 150 then insert values(s.id, s.x)
    when NOT matched and s.id = 151 then insert values(s.id, s.x)
    when NOT matched and s.id = 152 then insert values(s.id, s.x)
    when NOT matched and s.id = 153 then insert values(s.id, s.x)
    when NOT matched and s.id = 154 then insert values(s.id, s.x)
    when NOT matched and s.id = 155 then insert values(s.id, s.x)
    when NOT matched and s.id = 156 then insert values(s.id, s.x)
    when NOT matched and s.id = 157 then insert values(s.id, s.x)
    when NOT matched and s.id = 158 then insert values(s.id, s.x)
    when NOT matched and s.id = 159 then insert values(s.id, s.x)
    when NOT matched and s.id = 160 then insert values(s.id, s.x)
    when NOT matched and s.id = 161 then insert values(s.id, s.x)
    when NOT matched and s.id = 162 then insert values(s.id, s.x)
    when NOT matched and s.id = 163 then insert values(s.id, s.x)
    when NOT matched and s.id = 164 then insert values(s.id, s.x)
    when NOT matched and s.id = 165 then insert values(s.id, s.x)
    when NOT matched and s.id = 166 then insert values(s.id, s.x)
    when NOT matched and s.id = 167 then insert values(s.id, s.x)
    when NOT matched and s.id = 168 then insert values(s.id, s.x)
    when NOT matched and s.id = 169 then insert values(s.id, s.x)
    when NOT matched and s.id = 170 then insert values(s.id, s.x)
    when NOT matched and s.id = 171 then insert values(s.id, s.x)
    when NOT matched and s.id = 172 then insert values(s.id, s.x)
    when NOT matched and s.id = 173 then insert values(s.id, s.x)
    when NOT matched and s.id = 174 then insert values(s.id, s.x)
    when NOT matched and s.id = 175 then insert values(s.id, s.x)
    when NOT matched and s.id = 176 then insert values(s.id, s.x)
    when NOT matched and s.id = 177 then insert values(s.id, s.x)
    when NOT matched and s.id = 178 then insert values(s.id, s.x)
    when NOT matched and s.id = 179 then insert values(s.id, s.x)
    when NOT matched and s.id = 180 then insert values(s.id, s.x)
    when NOT matched and s.id = 181 then insert values(s.id, s.x)
    when NOT matched and s.id = 182 then insert values(s.id, s.x)
    when NOT matched and s.id = 183 then insert values(s.id, s.x)
    when NOT matched and s.id = 184 then insert values(s.id, s.x)
    when NOT matched and s.id = 185 then insert values(s.id, s.x)
    when NOT matched and s.id = 186 then insert values(s.id, s.x)
    when NOT matched and s.id = 187 then insert values(s.id, s.x)
    when NOT matched and s.id = 188 then insert values(s.id, s.x)
    when NOT matched and s.id = 189 then insert values(s.id, s.x)
    when NOT matched and s.id = 190 then insert values(s.id, s.x)
    when NOT matched and s.id = 191 then insert values(s.id, s.x)
    when NOT matched and s.id = 192 then insert values(s.id, s.x)
    when NOT matched and s.id = 193 then insert values(s.id, s.x)
    when NOT matched and s.id = 194 then insert values(s.id, s.x)
    when NOT matched and s.id = 195 then insert values(s.id, s.x)
    when NOT matched and s.id = 196 then insert values(s.id, s.x)
    when NOT matched and s.id = 197 then insert values(s.id, s.x)
    when NOT matched and s.id = 198 then insert values(s.id, s.x)
    when NOT matched and s.id = 199 then insert values(s.id, s.x)
    when NOT matched and s.id = 200 then insert values(s.id, s.x)
    when NOT matched and s.id = 201 then insert values(s.id, s.x)
    when NOT matched and s.id = 202 then insert values(s.id, s.x)
    when NOT matched and s.id = 203 then insert values(s.id, s.x)
    when NOT matched and s.id = 204 then insert values(s.id, s.x)
    when NOT matched and s.id = 205 then insert values(s.id, s.x)
    when NOT matched and s.id = 206 then insert values(s.id, s.x)
    when NOT matched and s.id = 207 then insert values(s.id, s.x)
    when NOT matched and s.id = 208 then insert values(s.id, s.x)
    when NOT matched and s.id = 209 then insert values(s.id, s.x)
    when NOT matched and s.id = 210 then insert values(s.id, s.x)
    when NOT matched and s.id = 211 then insert values(s.id, s.x)
    when NOT matched and s.id = 212 then insert values(s.id, s.x)
    when NOT matched and s.id = 213 then insert values(s.id, s.x)
    when NOT matched and s.id = 214 then insert values(s.id, s.x)
    when NOT matched and s.id = 215 then insert values(s.id, s.x)
    when NOT matched and s.id = 216 then insert values(s.id, s.x)
    when NOT matched and s.id = 217 then insert values(s.id, s.x)
    when NOT matched and s.id = 218 then insert values(s.id, s.x)
    when NOT matched and s.id = 219 then insert values(s.id, s.x)
    when NOT matched and s.id = 220 then insert values(s.id, s.x)
    when NOT matched and s.id = 221 then insert values(s.id, s.x)
    when NOT matched and s.id = 222 then insert values(s.id, s.x)
    when NOT matched and s.id = 223 then insert values(s.id, s.x)
    when NOT matched and s.id = 224 then insert values(s.id, s.x)
    when NOT matched and s.id = 225 then insert values(s.id, s.x)
    when NOT matched and s.id = 226 then insert values(s.id, s.x)
    when NOT matched and s.id = 227 then insert values(s.id, s.x)
    when NOT matched and s.id = 228 then insert values(s.id, s.x)
    when NOT matched and s.id = 229 then insert values(s.id, s.x)
    when NOT matched and s.id = 230 then insert values(s.id, s.x)
    when NOT matched and s.id = 231 then insert values(s.id, s.x)
    when NOT matched and s.id = 232 then insert values(s.id, s.x)
    when NOT matched and s.id = 233 then insert values(s.id, s.x)
    when NOT matched and s.id = 234 then insert values(s.id, s.x)
    when NOT matched and s.id = 235 then insert values(s.id, s.x)
    when NOT matched and s.id = 236 then insert values(s.id, s.x)
    when NOT matched and s.id = 237 then insert values(s.id, s.x)
    when NOT matched and s.id = 238 then insert values(s.id, s.x)
    when NOT matched and s.id = 239 then insert values(s.id, s.x)
    when NOT matched and s.id = 240 then insert values(s.id, s.x)
    when NOT matched and s.id = 241 then insert values(s.id, s.x)
    when NOT matched and s.id = 242 then insert values(s.id, s.x)
    when NOT matched and s.id = 243 then insert values(s.id, s.x)
    when NOT matched and s.id = 244 then insert values(s.id, s.x)
    when NOT matched and s.id = 245 then insert values(s.id, s.x)
    when NOT matched and s.id = 246 then insert values(s.id, s.x)
    when NOT matched and s.id = 247 then insert values(s.id, s.x)
    when NOT matched and s.id = 248 then insert values(s.id, s.x)
    when NOT matched and s.id = 249 then insert values(s.id, s.x)
    when NOT matched and s.id = 250 then insert values(s.id, s.x)
    when NOT matched and s.id = 251 then insert values(s.id, s.x)
    when NOT matched and s.id = 252 then insert values(s.id, s.x)
    when NOT matched and s.id = 253 then insert values(s.id, s.x)
    when NOT matched and s.id = 254 then insert values(s.id, s.x)
    ;
    merge into t using s on s.id = t.id
    when matched and s.id = 1 then update set t.x = t.x + s.x
    when matched and s.id = 2 then update set t.x = t.x + s.x
    when matched and s.id = 3 then update set t.x = t.x + s.x
    when matched and s.id = 4 then update set t.x = t.x + s.x
    when matched and s.id = 5 then update set t.x = t.x + s.x
    when matched and s.id = 6 then update set t.x = t.x + s.x
    when matched and s.id = 7 then update set t.x = t.x + s.x
    when matched and s.id = 8 then update set t.x = t.x + s.x
    when matched and s.id = 9 then update set t.x = t.x + s.x
    when matched and s.id = 10 then update set t.x = t.x + s.x
    when matched and s.id = 11 then update set t.x = t.x + s.x
    when matched and s.id = 12 then update set t.x = t.x + s.x
    when matched and s.id = 13 then update set t.x = t.x + s.x
    when matched and s.id = 14 then update set t.x = t.x + s.x
    when matched and s.id = 15 then update set t.x = t.x + s.x
    when matched and s.id = 16 then update set t.x = t.x + s.x
    when matched and s.id = 17 then update set t.x = t.x + s.x
    when matched and s.id = 18 then update set t.x = t.x + s.x
    when matched and s.id = 19 then update set t.x = t.x + s.x
    when matched and s.id = 20 then update set t.x = t.x + s.x
    when matched and s.id = 21 then update set t.x = t.x + s.x
    when matched and s.id = 22 then update set t.x = t.x + s.x
    when matched and s.id = 23 then update set t.x = t.x + s.x
    when matched and s.id = 24 then update set t.x = t.x + s.x
    when matched and s.id = 25 then update set t.x = t.x + s.x
    when matched and s.id = 26 then update set t.x = t.x + s.x
    when matched and s.id = 27 then update set t.x = t.x + s.x
    when matched and s.id = 28 then update set t.x = t.x + s.x
    when matched and s.id = 29 then update set t.x = t.x + s.x
    when matched and s.id = 30 then update set t.x = t.x + s.x
    when matched and s.id = 31 then update set t.x = t.x + s.x
    when matched and s.id = 32 then update set t.x = t.x + s.x
    when matched and s.id = 33 then update set t.x = t.x + s.x
    when matched and s.id = 34 then update set t.x = t.x + s.x
    when matched and s.id = 35 then update set t.x = t.x + s.x
    when matched and s.id = 36 then update set t.x = t.x + s.x
    when matched and s.id = 37 then update set t.x = t.x + s.x
    when matched and s.id = 38 then update set t.x = t.x + s.x
    when matched and s.id = 39 then update set t.x = t.x + s.x
    when matched and s.id = 40 then update set t.x = t.x + s.x
    when matched and s.id = 41 then update set t.x = t.x + s.x
    when matched and s.id = 42 then update set t.x = t.x + s.x
    when matched and s.id = 43 then update set t.x = t.x + s.x
    when matched and s.id = 44 then update set t.x = t.x + s.x
    when matched and s.id = 45 then update set t.x = t.x + s.x
    when matched and s.id = 46 then update set t.x = t.x + s.x
    when matched and s.id = 47 then update set t.x = t.x + s.x
    when matched and s.id = 48 then update set t.x = t.x + s.x
    when matched and s.id = 49 then update set t.x = t.x + s.x
    when matched and s.id = 50 then update set t.x = t.x + s.x
    when matched and s.id = 51 then update set t.x = t.x + s.x
    when matched and s.id = 52 then update set t.x = t.x + s.x
    when matched and s.id = 53 then update set t.x = t.x + s.x
    when matched and s.id = 54 then update set t.x = t.x + s.x
    when matched and s.id = 55 then update set t.x = t.x + s.x
    when matched and s.id = 56 then update set t.x = t.x + s.x
    when matched and s.id = 57 then update set t.x = t.x + s.x
    when matched and s.id = 58 then update set t.x = t.x + s.x
    when matched and s.id = 59 then update set t.x = t.x + s.x
    when matched and s.id = 60 then update set t.x = t.x + s.x
    when matched and s.id = 61 then update set t.x = t.x + s.x
    when matched and s.id = 62 then update set t.x = t.x + s.x
    when matched and s.id = 63 then update set t.x = t.x + s.x
    when matched and s.id = 64 then update set t.x = t.x + s.x
    when matched and s.id = 65 then update set t.x = t.x + s.x
    when matched and s.id = 66 then update set t.x = t.x + s.x
    when matched and s.id = 67 then update set t.x = t.x + s.x
    when matched and s.id = 68 then update set t.x = t.x + s.x
    when matched and s.id = 69 then update set t.x = t.x + s.x
    when matched and s.id = 70 then update set t.x = t.x + s.x
    when matched and s.id = 71 then update set t.x = t.x + s.x
    when matched and s.id = 72 then update set t.x = t.x + s.x
    when matched and s.id = 73 then update set t.x = t.x + s.x
    when matched and s.id = 74 then update set t.x = t.x + s.x
    when matched and s.id = 75 then update set t.x = t.x + s.x
    when matched and s.id = 76 then update set t.x = t.x + s.x
    when matched and s.id = 77 then update set t.x = t.x + s.x
    when matched and s.id = 78 then update set t.x = t.x + s.x
    when matched and s.id = 79 then update set t.x = t.x + s.x
    when matched and s.id = 80 then update set t.x = t.x + s.x
    when matched and s.id = 81 then update set t.x = t.x + s.x
    when matched and s.id = 82 then update set t.x = t.x + s.x
    when matched and s.id = 83 then update set t.x = t.x + s.x
    when matched and s.id = 84 then update set t.x = t.x + s.x
    when matched and s.id = 85 then update set t.x = t.x + s.x
    when matched and s.id = 86 then update set t.x = t.x + s.x
    when matched and s.id = 87 then update set t.x = t.x + s.x
    when matched and s.id = 88 then update set t.x = t.x + s.x
    when matched and s.id = 89 then update set t.x = t.x + s.x
    when matched and s.id = 90 then update set t.x = t.x + s.x
    when matched and s.id = 91 then update set t.x = t.x + s.x
    when matched and s.id = 92 then update set t.x = t.x + s.x
    when matched and s.id = 93 then update set t.x = t.x + s.x
    when matched and s.id = 94 then update set t.x = t.x + s.x
    when matched and s.id = 95 then update set t.x = t.x + s.x
    when matched and s.id = 96 then update set t.x = t.x + s.x
    when matched and s.id = 97 then update set t.x = t.x + s.x
    when matched and s.id = 98 then update set t.x = t.x + s.x
    when matched and s.id = 99 then update set t.x = t.x + s.x
    when matched and s.id = 100 then update set t.x = t.x + s.x
    when matched and s.id = 101 then update set t.x = t.x + s.x
    when matched and s.id = 102 then update set t.x = t.x + s.x
    when matched and s.id = 103 then update set t.x = t.x + s.x
    when matched and s.id = 104 then update set t.x = t.x + s.x
    when matched and s.id = 105 then update set t.x = t.x + s.x
    when matched and s.id = 106 then update set t.x = t.x + s.x
    when matched and s.id = 107 then update set t.x = t.x + s.x
    when matched and s.id = 108 then update set t.x = t.x + s.x
    when matched and s.id = 109 then update set t.x = t.x + s.x
    when matched and s.id = 110 then update set t.x = t.x + s.x
    when matched and s.id = 111 then update set t.x = t.x + s.x
    when matched and s.id = 112 then update set t.x = t.x + s.x
    when matched and s.id = 113 then update set t.x = t.x + s.x
    when matched and s.id = 114 then update set t.x = t.x + s.x
    when matched and s.id = 115 then update set t.x = t.x + s.x
    when matched and s.id = 116 then update set t.x = t.x + s.x
    when matched and s.id = 117 then update set t.x = t.x + s.x
    when matched and s.id = 118 then update set t.x = t.x + s.x
    when matched and s.id = 119 then update set t.x = t.x + s.x
    when matched and s.id = 120 then update set t.x = t.x + s.x
    when matched and s.id = 121 then update set t.x = t.x + s.x
    when matched and s.id = 122 then update set t.x = t.x + s.x
    when matched and s.id = 123 then update set t.x = t.x + s.x
    when matched and s.id = 124 then update set t.x = t.x + s.x
    when matched and s.id = 125 then update set t.x = t.x + s.x
    when matched and s.id = 126 then update set t.x = t.x + s.x
    when matched and s.id = 127 then update set t.x = t.x + s.x
    when matched and s.id = 128 then update set t.x = t.x + s.x
    when matched and s.id = 129 then update set t.x = t.x + s.x
    when matched and s.id = 130 then update set t.x = t.x + s.x
    when matched and s.id = 131 then update set t.x = t.x + s.x
    when matched and s.id = 132 then update set t.x = t.x + s.x
    when matched and s.id = 133 then update set t.x = t.x + s.x
    when matched and s.id = 134 then update set t.x = t.x + s.x
    when matched and s.id = 135 then update set t.x = t.x + s.x
    when matched and s.id = 136 then update set t.x = t.x + s.x
    when matched and s.id = 137 then update set t.x = t.x + s.x
    when matched and s.id = 138 then update set t.x = t.x + s.x
    when matched and s.id = 139 then update set t.x = t.x + s.x
    when matched and s.id = 140 then update set t.x = t.x + s.x
    when matched and s.id = 141 then update set t.x = t.x + s.x
    when matched and s.id = 142 then update set t.x = t.x + s.x
    when matched and s.id = 143 then update set t.x = t.x + s.x
    when matched and s.id = 144 then update set t.x = t.x + s.x
    when matched and s.id = 145 then update set t.x = t.x + s.x
    when matched and s.id = 146 then update set t.x = t.x + s.x
    when matched and s.id = 147 then update set t.x = t.x + s.x
    when matched and s.id = 148 then update set t.x = t.x + s.x
    when matched and s.id = 149 then update set t.x = t.x + s.x
    when matched and s.id = 150 then update set t.x = t.x + s.x
    when matched and s.id = 151 then update set t.x = t.x + s.x
    when matched and s.id = 152 then update set t.x = t.x + s.x
    when matched and s.id = 153 then update set t.x = t.x + s.x
    when matched and s.id = 154 then update set t.x = t.x + s.x
    when matched and s.id = 155 then update set t.x = t.x + s.x
    when matched and s.id = 156 then update set t.x = t.x + s.x
    when matched and s.id = 157 then update set t.x = t.x + s.x
    when matched and s.id = 158 then update set t.x = t.x + s.x
    when matched and s.id = 159 then update set t.x = t.x + s.x
    when matched and s.id = 160 then update set t.x = t.x + s.x
    when matched and s.id = 161 then update set t.x = t.x + s.x
    when matched and s.id = 162 then update set t.x = t.x + s.x
    when matched and s.id = 163 then update set t.x = t.x + s.x
    when matched and s.id = 164 then update set t.x = t.x + s.x
    when matched and s.id = 165 then update set t.x = t.x + s.x
    when matched and s.id = 166 then update set t.x = t.x + s.x
    when matched and s.id = 167 then update set t.x = t.x + s.x
    when matched and s.id = 168 then update set t.x = t.x + s.x
    when matched and s.id = 169 then update set t.x = t.x + s.x
    when matched and s.id = 170 then update set t.x = t.x + s.x
    when matched and s.id = 171 then update set t.x = t.x + s.x
    when matched and s.id = 172 then update set t.x = t.x + s.x
    when matched and s.id = 173 then update set t.x = t.x + s.x
    when matched and s.id = 174 then update set t.x = t.x + s.x
    when matched and s.id = 175 then update set t.x = t.x + s.x
    when matched and s.id = 176 then update set t.x = t.x + s.x
    when matched and s.id = 177 then update set t.x = t.x + s.x
    when matched and s.id = 178 then update set t.x = t.x + s.x
    when matched and s.id = 179 then update set t.x = t.x + s.x
    when matched and s.id = 180 then update set t.x = t.x + s.x
    when matched and s.id = 181 then update set t.x = t.x + s.x
    when matched and s.id = 182 then update set t.x = t.x + s.x
    when matched and s.id = 183 then update set t.x = t.x + s.x
    when matched and s.id = 184 then update set t.x = t.x + s.x
    when matched and s.id = 185 then update set t.x = t.x + s.x
    when matched and s.id = 186 then update set t.x = t.x + s.x
    when matched and s.id = 187 then update set t.x = t.x + s.x
    when matched and s.id = 188 then update set t.x = t.x + s.x
    when matched and s.id = 189 then update set t.x = t.x + s.x
    when matched and s.id = 190 then update set t.x = t.x + s.x
    when matched and s.id = 191 then update set t.x = t.x + s.x
    when matched and s.id = 192 then update set t.x = t.x + s.x
    when matched and s.id = 193 then update set t.x = t.x + s.x
    when matched and s.id = 194 then update set t.x = t.x + s.x
    when matched and s.id = 195 then update set t.x = t.x + s.x
    when matched and s.id = 196 then update set t.x = t.x + s.x
    when matched and s.id = 197 then update set t.x = t.x + s.x
    when matched and s.id = 198 then update set t.x = t.x + s.x
    when matched and s.id = 199 then update set t.x = t.x + s.x
    when matched and s.id = 200 then update set t.x = t.x + s.x
    when matched and s.id = 201 then update set t.x = t.x + s.x
    when matched and s.id = 202 then update set t.x = t.x + s.x
    when matched and s.id = 203 then update set t.x = t.x + s.x
    when matched and s.id = 204 then update set t.x = t.x + s.x
    when matched and s.id = 205 then update set t.x = t.x + s.x
    when matched and s.id = 206 then update set t.x = t.x + s.x
    when matched and s.id = 207 then update set t.x = t.x + s.x
    when matched and s.id = 208 then update set t.x = t.x + s.x
    when matched and s.id = 209 then update set t.x = t.x + s.x
    when matched and s.id = 210 then update set t.x = t.x + s.x
    when matched and s.id = 211 then update set t.x = t.x + s.x
    when matched and s.id = 212 then update set t.x = t.x + s.x
    when matched and s.id = 213 then update set t.x = t.x + s.x
    when matched and s.id = 214 then update set t.x = t.x + s.x
    when matched and s.id = 215 then update set t.x = t.x + s.x
    when matched and s.id = 216 then update set t.x = t.x + s.x
    when matched and s.id = 217 then update set t.x = t.x + s.x
    when matched and s.id = 218 then update set t.x = t.x + s.x
    when matched and s.id = 219 then update set t.x = t.x + s.x
    when matched and s.id = 220 then update set t.x = t.x + s.x
    when matched and s.id = 221 then update set t.x = t.x + s.x
    when matched and s.id = 222 then update set t.x = t.x + s.x
    when matched and s.id = 223 then update set t.x = t.x + s.x
    when matched and s.id = 224 then update set t.x = t.x + s.x
    when matched and s.id = 225 then update set t.x = t.x + s.x
    when matched and s.id = 226 then update set t.x = t.x + s.x
    when matched and s.id = 227 then update set t.x = t.x + s.x
    when matched and s.id = 228 then update set t.x = t.x + s.x
    when matched and s.id = 229 then update set t.x = t.x + s.x
    when matched and s.id = 230 then update set t.x = t.x + s.x
    when matched and s.id = 231 then update set t.x = t.x + s.x
    when matched and s.id = 232 then update set t.x = t.x + s.x
    when matched and s.id = 233 then update set t.x = t.x + s.x
    when matched and s.id = 234 then update set t.x = t.x + s.x
    when matched and s.id = 235 then update set t.x = t.x + s.x
    when matched and s.id = 236 then update set t.x = t.x + s.x
    when matched and s.id = 237 then update set t.x = t.x + s.x
    when matched and s.id = 238 then update set t.x = t.x + s.x
    when matched and s.id = 239 then update set t.x = t.x + s.x
    when matched and s.id = 240 then update set t.x = t.x + s.x
    when matched and s.id = 241 then update set t.x = t.x + s.x
    when matched and s.id = 242 then update set t.x = t.x + s.x
    when matched and s.id = 243 then update set t.x = t.x + s.x
    when matched and s.id = 244 then update set t.x = t.x + s.x
    when matched and s.id = 245 then update set t.x = t.x + s.x
    when matched and s.id = 246 then update set t.x = t.x + s.x
    when matched and s.id = 247 then update set t.x = t.x + s.x
    when matched and s.id = 248 then update set t.x = t.x + s.x
    when matched and s.id = 249 then update set t.x = t.x + s.x
    when matched and s.id = 250 then update set t.x = t.x + s.x
    when matched and s.id = 251 then update set t.x = t.x + s.x
    when matched and s.id = 252 then update set t.x = t.x + s.x
    when matched and s.id = 253 then update set t.x = t.x + s.x
    when matched and s.id = 254 then update set t.x = t.x + s.x
    ;
    rollback;
    set count on;
    select * from t;
    set count off;

    -- 2. Check correctness of results:
    select * from tb;
    merge into tb t
    using ta s
    on s.id = t.id
    when matched and t.id < 2 then delete
    when matched then update set t.x = t.x + s.x, t.y = t.y - s.y
    when not matched and s.x < 250 then insert values(-s.id, s.x, s.y)
    when not matched then insert values(s.id, s.x, s.y)
    ;
    select * from tb;
    rollback;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    Records affected: 0

              ID            X            Y
    ============ ============ ============
               1           10           11
               4           40           44
               5           50           55


              ID            X            Y
    ============ ============ ============
               4          440         -400
               5          550         -500
              -2          200          222
               3          300          333
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

