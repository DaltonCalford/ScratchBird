#coding:utf-8

"""
ID:          n/a
TITLE:       Test of packages names resolution - usage of scope specifier '%package'
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_NAME_RESOLUTION_01.script
NOTES:
    [21.09.2025] pzotov
    See doc/sql.extensions/README.name_resolution.md
    Checked on 6.0.0.1277.
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user: User = user_factory('db', name='tmp_gtcs_nested_proc_user', password='123')

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('=', ''), ('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', substitutions = substitutions)

#-----------------------------------------------------------

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_user: User):

    test_script = f"""
        set list on;
        set blob all;
        set term ^;

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        create package pkg1
        as
        begin
            procedure p1 returns (o varchar(50));
            procedure p1a returns (o varchar(50));
            procedure p1b returns (o varchar(50));
            procedure p1c returns (o varchar(50));
            procedure p2 returns (o varchar(50));
            procedure p2a returns (o varchar(50));
            procedure p2b returns (o varchar(50));
            procedure p2c returns (o varchar(50));
            function f1 returns varchar(50);
            function f1a returns varchar(50);
            function f1b returns varchar(50);
            function f1c returns varchar(50);
            function f2 returns varchar(50);
        end^

        create package body pkg1
        as
        begin
            procedure p1 returns (o varchar(50))
            as
            begin
                o = 'pkg1.p1';
                suspend;
            end

            procedure p1a returns (o varchar(50))
            as
            begin
                execute procedure p1 returning_values o;
                suspend;
            end

            procedure p1b returns (o varchar(50))
            as
            begin
                execute procedure pkg1.p1 returning_values o;
                suspend;
            end

            procedure p1c returns (o varchar(50))
            as
            begin
                execute procedure pkg1%package.p1 returning_values o;
                suspend;
            end

            procedure p2 returns (o varchar(50))
            as
            begin
                o = 'pkg1.p2';
                suspend;
            end

            procedure p2a returns (o varchar(50))
            as
            begin
                select * from p2 into o;
                suspend;
            end

            procedure p2b returns (o varchar(50))
            as
            begin
                select * from pkg1.p2 into o;
                suspend;
            end

            procedure p2c returns (o varchar(50))
            as
            begin
                select * from pkg1%package.p2 into o;
                suspend;
            end

            function f1 returns varchar(50)
            as
            begin
                return 'pkg1.f1';
            end

            function f1a returns varchar(50)
            as
            begin
                return f1();
            end

            function f1b returns varchar(50)
            as
            begin
                return pkg1.f1();
            end

            function f1c returns varchar(50)
            as
            begin
                return pkg1%package.f1();
            end

            function f2 returns varchar(50)
            as
            begin
                return 'pkg1.f2';
            end
        end^

        set term ;^
        commit;

        --+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

        execute procedure pkg1.p1;
        execute procedure pkg1%package.p1;
        select 'point-000' as msg from rdb$database;

        select * from pkg1.p1;
        select * from pkg1%package.p1;
        select 'point-050' as msg from rdb$database;

        execute procedure pkg1.p1a;
        execute procedure pkg1.p1b;
        execute procedure pkg1.p1c;
        select 'point-100' as msg from rdb$database;

        execute procedure pkg1.p2;
        execute procedure pkg1%package.p2;
        select 'point-150' as msg from rdb$database;

        select * from pkg1.p2a;
        select 'point-200' as msg from rdb$database;
        
        select * from pkg1.p2b;
        select 'point-210' as msg from rdb$database;
        
        select * from pkg1.p2c;
        select 'point-220' as msg from rdb$database;

        select pkg1.f1() from rdb$database;
        select 'point-230' as msg from rdb$database;

        select pkg1.f1a() from rdb$database;
        select 'point-240' as msg from rdb$database;
        
        select pkg1.f1b() from rdb$database;
        select 'point-250' as msg from rdb$database;
        
        select pkg1.f1c() from rdb$database;
        select 'point-260' as msg from rdb$database;

        select pkg1%package.f1() from rdb$database;
        select 'point-270' as msg from rdb$database;

        select pkg1.f2() from rdb$database;
        select 'point-280' as msg from rdb$database;

        select pkg1%package.f2() from rdb$database;
        select 'point-999' as msg from rdb$database;
    """
    
    expected_stdout = """
        O pkg1.p1
        O pkg1.p1
        MSG point-000
        O pkg1.p1
        O pkg1.p1
        MSG point-050
        O pkg1.p1
        O pkg1.p1
        O pkg1.p1
        MSG point-100
        O pkg1.p2
        O pkg1.p2
        MSG point-150
        O pkg1.p2
        MSG point-200
        O pkg1.p2
        MSG point-210
        O pkg1.p2
        MSG point-220
        F1 pkg1.f1
        MSG point-230
        F1A pkg1.f1
        MSG point-240
        F1B pkg1.f1
        MSG point-250
        F1C pkg1.f1
        MSG point-260
        F1 pkg1.f1
        MSG point-270
        F2 pkg1.f2
        MSG point-280
        F2 pkg1.f2
        MSG point-999
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
