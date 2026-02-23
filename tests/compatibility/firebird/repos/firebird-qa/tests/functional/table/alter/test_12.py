#coding:utf-8

"""
ID:          table.alter-12
TITLE:       Verify ability to create exactly 254 changes of format (increasing it by 1) after initial creating table
DESCRIPTION: 
FBTEST:      functional.table.alter.12
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    It is enough to run 'alter table test1' 254 and then 'alter table test2' 255  times, and then run query to RDB$FORMATS table.
    Max value of rdb$format must be 255 in both cases.
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;

    recreate table test1(f0 int); -- this also create "format #1"
    -- following should run OK because of 254 changes:
    alter table test1 add f1 int;
    alter table test1 add f2 int;
    alter table test1 add f3 int;
    alter table test1 add f4 int;
    alter table test1 add f5 int;
    alter table test1 add f6 int;
    alter table test1 add f7 int;
    alter table test1 add f8 int;
    alter table test1 add f9 int;
    alter table test1 add f10 int;
    alter table test1 add f11 int;
    alter table test1 add f12 int;
    alter table test1 add f13 int;
    alter table test1 add f14 int;
    alter table test1 add f15 int;
    alter table test1 add f16 int;
    alter table test1 add f17 int;
    alter table test1 add f18 int;
    alter table test1 add f19 int;
    alter table test1 add f20 int;
    alter table test1 add f21 int;
    alter table test1 add f22 int;
    alter table test1 add f23 int;
    alter table test1 add f24 int;
    alter table test1 add f25 int;
    alter table test1 add f26 int;
    alter table test1 add f27 int;
    alter table test1 add f28 int;
    alter table test1 add f29 int;
    alter table test1 add f30 int;
    alter table test1 add f31 int;
    alter table test1 add f32 int;
    alter table test1 add f33 int;
    alter table test1 add f34 int;
    alter table test1 add f35 int;
    alter table test1 add f36 int;
    alter table test1 add f37 int;
    alter table test1 add f38 int;
    alter table test1 add f39 int;
    alter table test1 add f40 int;
    alter table test1 add f41 int;
    alter table test1 add f42 int;
    alter table test1 add f43 int;
    alter table test1 add f44 int;
    alter table test1 add f45 int;
    alter table test1 add f46 int;
    alter table test1 add f47 int;
    alter table test1 add f48 int;
    alter table test1 add f49 int;
    alter table test1 add f50 int;
    alter table test1 add f51 int;
    alter table test1 add f52 int;
    alter table test1 add f53 int;
    alter table test1 add f54 int;
    alter table test1 add f55 int;
    alter table test1 add f56 int;
    alter table test1 add f57 int;
    alter table test1 add f58 int;
    alter table test1 add f59 int;
    alter table test1 add f60 int;
    alter table test1 add f61 int;
    alter table test1 add f62 int;
    alter table test1 add f63 int;
    alter table test1 add f64 int;
    alter table test1 add f65 int;
    alter table test1 add f66 int;
    alter table test1 add f67 int;
    alter table test1 add f68 int;
    alter table test1 add f69 int;
    alter table test1 add f70 int;
    alter table test1 add f71 int;
    alter table test1 add f72 int;
    alter table test1 add f73 int;
    alter table test1 add f74 int;
    alter table test1 add f75 int;
    alter table test1 add f76 int;
    alter table test1 add f77 int;
    alter table test1 add f78 int;
    alter table test1 add f79 int;
    alter table test1 add f80 int;
    alter table test1 add f81 int;
    alter table test1 add f82 int;
    alter table test1 add f83 int;
    alter table test1 add f84 int;
    alter table test1 add f85 int;
    alter table test1 add f86 int;
    alter table test1 add f87 int;
    alter table test1 add f88 int;
    alter table test1 add f89 int;
    alter table test1 add f90 int;
    alter table test1 add f91 int;
    alter table test1 add f92 int;
    alter table test1 add f93 int;
    alter table test1 add f94 int;
    alter table test1 add f95 int;
    alter table test1 add f96 int;
    alter table test1 add f97 int;
    alter table test1 add f98 int;
    alter table test1 add f99 int;
    alter table test1 add f100 int;
    alter table test1 add f101 int;
    alter table test1 add f102 int;
    alter table test1 add f103 int;
    alter table test1 add f104 int;
    alter table test1 add f105 int;
    alter table test1 add f106 int;
    alter table test1 add f107 int;
    alter table test1 add f108 int;
    alter table test1 add f109 int;
    alter table test1 add f110 int;
    alter table test1 add f111 int;
    alter table test1 add f112 int;
    alter table test1 add f113 int;
    alter table test1 add f114 int;
    alter table test1 add f115 int;
    alter table test1 add f116 int;
    alter table test1 add f117 int;
    alter table test1 add f118 int;
    alter table test1 add f119 int;
    alter table test1 add f120 int;
    alter table test1 add f121 int;
    alter table test1 add f122 int;
    alter table test1 add f123 int;
    alter table test1 add f124 int;
    alter table test1 add f125 int;
    alter table test1 add f126 int;
    alter table test1 add f127 int;
    alter table test1 add f128 int;
    alter table test1 add f129 int;
    alter table test1 add f130 int;
    alter table test1 add f131 int;
    alter table test1 add f132 int;
    alter table test1 add f133 int;
    alter table test1 add f134 int;
    alter table test1 add f135 int;
    alter table test1 add f136 int;
    alter table test1 add f137 int;
    alter table test1 add f138 int;
    alter table test1 add f139 int;
    alter table test1 add f140 int;
    alter table test1 add f141 int;
    alter table test1 add f142 int;
    alter table test1 add f143 int;
    alter table test1 add f144 int;
    alter table test1 add f145 int;
    alter table test1 add f146 int;
    alter table test1 add f147 int;
    alter table test1 add f148 int;
    alter table test1 add f149 int;
    alter table test1 add f150 int;
    alter table test1 add f151 int;
    alter table test1 add f152 int;
    alter table test1 add f153 int;
    alter table test1 add f154 int;
    alter table test1 add f155 int;
    alter table test1 add f156 int;
    alter table test1 add f157 int;
    alter table test1 add f158 int;
    alter table test1 add f159 int;
    alter table test1 add f160 int;
    alter table test1 add f161 int;
    alter table test1 add f162 int;
    alter table test1 add f163 int;
    alter table test1 add f164 int;
    alter table test1 add f165 int;
    alter table test1 add f166 int;
    alter table test1 add f167 int;
    alter table test1 add f168 int;
    alter table test1 add f169 int;
    alter table test1 add f170 int;
    alter table test1 add f171 int;
    alter table test1 add f172 int;
    alter table test1 add f173 int;
    alter table test1 add f174 int;
    alter table test1 add f175 int;
    alter table test1 add f176 int;
    alter table test1 add f177 int;
    alter table test1 add f178 int;
    alter table test1 add f179 int;
    alter table test1 add f180 int;
    alter table test1 add f181 int;
    alter table test1 add f182 int;
    alter table test1 add f183 int;
    alter table test1 add f184 int;
    alter table test1 add f185 int;
    alter table test1 add f186 int;
    alter table test1 add f187 int;
    alter table test1 add f188 int;
    alter table test1 add f189 int;
    alter table test1 add f190 int;
    alter table test1 add f191 int;
    alter table test1 add f192 int;
    alter table test1 add f193 int;
    alter table test1 add f194 int;
    alter table test1 add f195 int;
    alter table test1 add f196 int;
    alter table test1 add f197 int;
    alter table test1 add f198 int;
    alter table test1 add f199 int;
    alter table test1 add f200 int;
    alter table test1 add f201 int;
    alter table test1 add f202 int;
    alter table test1 add f203 int;
    alter table test1 add f204 int;
    alter table test1 add f205 int;
    alter table test1 add f206 int;
    alter table test1 add f207 int;
    alter table test1 add f208 int;
    alter table test1 add f209 int;
    alter table test1 add f210 int;
    alter table test1 add f211 int;
    alter table test1 add f212 int;
    alter table test1 add f213 int;
    alter table test1 add f214 int;
    alter table test1 add f215 int;
    alter table test1 add f216 int;
    alter table test1 add f217 int;
    alter table test1 add f218 int;
    alter table test1 add f219 int;
    alter table test1 add f220 int;
    alter table test1 add f221 int;
    alter table test1 add f222 int;
    alter table test1 add f223 int;
    alter table test1 add f224 int;
    alter table test1 add f225 int;
    alter table test1 add f226 int;
    alter table test1 add f227 int;
    alter table test1 add f228 int;
    alter table test1 add f229 int;
    alter table test1 add f230 int;
    alter table test1 add f231 int;
    alter table test1 add f232 int;
    alter table test1 add f233 int;
    alter table test1 add f234 int;
    alter table test1 add f235 int;
    alter table test1 add f236 int;
    alter table test1 add f237 int;
    alter table test1 add f238 int;
    alter table test1 add f239 int;
    alter table test1 add f240 int;
    alter table test1 add f241 int;
    alter table test1 add f242 int;
    alter table test1 add f243 int;
    alter table test1 add f244 int;
    alter table test1 add f245 int;
    alter table test1 add f246 int;
    alter table test1 add f247 int;
    alter table test1 add f248 int;
    alter table test1 add f249 int;
    alter table test1 add f250 int;
    alter table test1 add f251 int;
    alter table test1 add f252 int;
    alter table test1 add f253 int;
    alter table test1 add f254 int;
    commit;

    select max(rf.rdb$format) as max_test1_format
    from rdb$formats rf
    join rdb$relations rr on rf.rdb$relation_id = rr.rdb$relation_id
    where rr.rdb$relation_name = upper('test1');
    commit;

    -- following should FAIL because of 255 changes:
    recreate table test2(f0 int);
    alter table test2 add f1 int;
    alter table test2 add f2 int;
    alter table test2 add f3 int;
    alter table test2 add f4 int;
    alter table test2 add f5 int;
    alter table test2 add f6 int;
    alter table test2 add f7 int;
    alter table test2 add f8 int;
    alter table test2 add f9 int;
    alter table test2 add f10 int;
    alter table test2 add f11 int;
    alter table test2 add f12 int;
    alter table test2 add f13 int;
    alter table test2 add f14 int;
    alter table test2 add f15 int;
    alter table test2 add f16 int;
    alter table test2 add f17 int;
    alter table test2 add f18 int;
    alter table test2 add f19 int;
    alter table test2 add f20 int;
    alter table test2 add f21 int;
    alter table test2 add f22 int;
    alter table test2 add f23 int;
    alter table test2 add f24 int;
    alter table test2 add f25 int;
    alter table test2 add f26 int;
    alter table test2 add f27 int;
    alter table test2 add f28 int;
    alter table test2 add f29 int;
    alter table test2 add f30 int;
    alter table test2 add f31 int;
    alter table test2 add f32 int;
    alter table test2 add f33 int;
    alter table test2 add f34 int;
    alter table test2 add f35 int;
    alter table test2 add f36 int;
    alter table test2 add f37 int;
    alter table test2 add f38 int;
    alter table test2 add f39 int;
    alter table test2 add f40 int;
    alter table test2 add f41 int;
    alter table test2 add f42 int;
    alter table test2 add f43 int;
    alter table test2 add f44 int;
    alter table test2 add f45 int;
    alter table test2 add f46 int;
    alter table test2 add f47 int;
    alter table test2 add f48 int;
    alter table test2 add f49 int;
    alter table test2 add f50 int;
    alter table test2 add f51 int;
    alter table test2 add f52 int;
    alter table test2 add f53 int;
    alter table test2 add f54 int;
    alter table test2 add f55 int;
    alter table test2 add f56 int;
    alter table test2 add f57 int;
    alter table test2 add f58 int;
    alter table test2 add f59 int;
    alter table test2 add f60 int;
    alter table test2 add f61 int;
    alter table test2 add f62 int;
    alter table test2 add f63 int;
    alter table test2 add f64 int;
    alter table test2 add f65 int;
    alter table test2 add f66 int;
    alter table test2 add f67 int;
    alter table test2 add f68 int;
    alter table test2 add f69 int;
    alter table test2 add f70 int;
    alter table test2 add f71 int;
    alter table test2 add f72 int;
    alter table test2 add f73 int;
    alter table test2 add f74 int;
    alter table test2 add f75 int;
    alter table test2 add f76 int;
    alter table test2 add f77 int;
    alter table test2 add f78 int;
    alter table test2 add f79 int;
    alter table test2 add f80 int;
    alter table test2 add f81 int;
    alter table test2 add f82 int;
    alter table test2 add f83 int;
    alter table test2 add f84 int;
    alter table test2 add f85 int;
    alter table test2 add f86 int;
    alter table test2 add f87 int;
    alter table test2 add f88 int;
    alter table test2 add f89 int;
    alter table test2 add f90 int;
    alter table test2 add f91 int;
    alter table test2 add f92 int;
    alter table test2 add f93 int;
    alter table test2 add f94 int;
    alter table test2 add f95 int;
    alter table test2 add f96 int;
    alter table test2 add f97 int;
    alter table test2 add f98 int;
    alter table test2 add f99 int;
    alter table test2 add f100 int;
    alter table test2 add f101 int;
    alter table test2 add f102 int;
    alter table test2 add f103 int;
    alter table test2 add f104 int;
    alter table test2 add f105 int;
    alter table test2 add f106 int;
    alter table test2 add f107 int;
    alter table test2 add f108 int;
    alter table test2 add f109 int;
    alter table test2 add f110 int;
    alter table test2 add f111 int;
    alter table test2 add f112 int;
    alter table test2 add f113 int;
    alter table test2 add f114 int;
    alter table test2 add f115 int;
    alter table test2 add f116 int;
    alter table test2 add f117 int;
    alter table test2 add f118 int;
    alter table test2 add f119 int;
    alter table test2 add f120 int;
    alter table test2 add f121 int;
    alter table test2 add f122 int;
    alter table test2 add f123 int;
    alter table test2 add f124 int;
    alter table test2 add f125 int;
    alter table test2 add f126 int;
    alter table test2 add f127 int;
    alter table test2 add f128 int;
    alter table test2 add f129 int;
    alter table test2 add f130 int;
    alter table test2 add f131 int;
    alter table test2 add f132 int;
    alter table test2 add f133 int;
    alter table test2 add f134 int;
    alter table test2 add f135 int;
    alter table test2 add f136 int;
    alter table test2 add f137 int;
    alter table test2 add f138 int;
    alter table test2 add f139 int;
    alter table test2 add f140 int;
    alter table test2 add f141 int;
    alter table test2 add f142 int;
    alter table test2 add f143 int;
    alter table test2 add f144 int;
    alter table test2 add f145 int;
    alter table test2 add f146 int;
    alter table test2 add f147 int;
    alter table test2 add f148 int;
    alter table test2 add f149 int;
    alter table test2 add f150 int;
    alter table test2 add f151 int;
    alter table test2 add f152 int;
    alter table test2 add f153 int;
    alter table test2 add f154 int;
    alter table test2 add f155 int;
    alter table test2 add f156 int;
    alter table test2 add f157 int;
    alter table test2 add f158 int;
    alter table test2 add f159 int;
    alter table test2 add f160 int;
    alter table test2 add f161 int;
    alter table test2 add f162 int;
    alter table test2 add f163 int;
    alter table test2 add f164 int;
    alter table test2 add f165 int;
    alter table test2 add f166 int;
    alter table test2 add f167 int;
    alter table test2 add f168 int;
    alter table test2 add f169 int;
    alter table test2 add f170 int;
    alter table test2 add f171 int;
    alter table test2 add f172 int;
    alter table test2 add f173 int;
    alter table test2 add f174 int;
    alter table test2 add f175 int;
    alter table test2 add f176 int;
    alter table test2 add f177 int;
    alter table test2 add f178 int;
    alter table test2 add f179 int;
    alter table test2 add f180 int;
    alter table test2 add f181 int;
    alter table test2 add f182 int;
    alter table test2 add f183 int;
    alter table test2 add f184 int;
    alter table test2 add f185 int;
    alter table test2 add f186 int;
    alter table test2 add f187 int;
    alter table test2 add f188 int;
    alter table test2 add f189 int;
    alter table test2 add f190 int;
    alter table test2 add f191 int;
    alter table test2 add f192 int;
    alter table test2 add f193 int;
    alter table test2 add f194 int;
    alter table test2 add f195 int;
    alter table test2 add f196 int;
    alter table test2 add f197 int;
    alter table test2 add f198 int;
    alter table test2 add f199 int;
    alter table test2 add f200 int;
    alter table test2 add f201 int;
    alter table test2 add f202 int;
    alter table test2 add f203 int;
    alter table test2 add f204 int;
    alter table test2 add f205 int;
    alter table test2 add f206 int;
    alter table test2 add f207 int;
    alter table test2 add f208 int;
    alter table test2 add f209 int;
    alter table test2 add f210 int;
    alter table test2 add f211 int;
    alter table test2 add f212 int;
    alter table test2 add f213 int;
    alter table test2 add f214 int;
    alter table test2 add f215 int;
    alter table test2 add f216 int;
    alter table test2 add f217 int;
    alter table test2 add f218 int;
    alter table test2 add f219 int;
    alter table test2 add f220 int;
    alter table test2 add f221 int;
    alter table test2 add f222 int;
    alter table test2 add f223 int;
    alter table test2 add f224 int;
    alter table test2 add f225 int;
    alter table test2 add f226 int;
    alter table test2 add f227 int;
    alter table test2 add f228 int;
    alter table test2 add f229 int;
    alter table test2 add f230 int;
    alter table test2 add f231 int;
    alter table test2 add f232 int;
    alter table test2 add f233 int;
    alter table test2 add f234 int;
    alter table test2 add f235 int;
    alter table test2 add f236 int;
    alter table test2 add f237 int;
    alter table test2 add f238 int;
    alter table test2 add f239 int;
    alter table test2 add f240 int;
    alter table test2 add f241 int;
    alter table test2 add f242 int;
    alter table test2 add f243 int;
    alter table test2 add f244 int;
    alter table test2 add f245 int;
    alter table test2 add f246 int;
    alter table test2 add f247 int;
    alter table test2 add f248 int;
    alter table test2 add f249 int;
    alter table test2 add f250 int;
    alter table test2 add f251 int;
    alter table test2 add f252 int;
    alter table test2 add f253 int;
    alter table test2 add f254 int;
    alter table test2 add f255 int;
    alter table test2 add f256 int;
    commit;

    select max(rf.rdb$format) as max_test2_format
    from rdb$formats rf
    join rdb$relations rr on rf.rdb$relation_id = rr.rdb$relation_id
    where rr.rdb$relation_name = upper('test2');
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST2_TABLE_NAME = 'TEST2' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST2"'
    expected_stdout = f"""
        MAX_TEST1_FORMAT                255
        Records affected: 1
        
        Statement failed, SQLSTATE = 54000
        unsuccessful metadata update
        -TABLE {TEST2_TABLE_NAME}
        -too many versions

        Statement failed, SQLSTATE = 54000
        unsuccessful metadata update
        -TABLE {TEST2_TABLE_NAME}
        -too many versions
        
        MAX_TEST2_FORMAT                255
        Records affected: 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
