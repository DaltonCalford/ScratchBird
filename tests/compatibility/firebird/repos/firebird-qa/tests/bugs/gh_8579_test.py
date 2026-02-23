#coding:utf-8

"""
ID:          issue-8579
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8579
TITLE:       Add sub routines info in the BLR debug blob filter
DESCRIPTION:
    Test creates procedure (sp_main) and function (fn_main) and each contains <SUBROUTINES_COUNT> declarations
    of inner procedures and <SUBROUTINES_COUNT> functions.
    Also, package with two units is created: procedure 'packaged_proc' and function 'packaged_func', and each
    of them have <SUBROUTINES_COUNT> declarations of inner sub-units (procedures and functions).
    Then we run query 'select rdb$debug_info from rdb$...' and check that line line <BLR_TO_SOURCE_TXT>
    occurs:
        * one time for each 'main-level' unit (i.e. sp_main; fn_pain; packaged_proc and packaged_func;
        * exactly <SUBROUTINES_COUNT> times after each kind sub-routine name.
    Expected output is build 'on-the-fly' during generation of appropriate DDL expressions, see 'expected_lst'.
    One need to keep in mind that rdb$debug_info shows inner functions *before* inner procedures.
NOTES:
    [31.05.2025] pzotov
    If we try to declare inner procedures 19 times then line 'BLR to Source mapping:' appears to be "broken"
    for last (19th) sub-routine. Because of this, test currently must use no more than 18 declarations.
    See: https://github.com/FirebirdSQL/firebird/issues/8590

    Problem did exist up to 6.0.0.799-c82c9cf and has been fixed 11-jun-2025:
    https://github.com/FirebirdSQL/firebird/commit/c867f34da84fbc0e1843757c386f955c014d8d41
    After fix, test execution time is ~6s.
    
    Checked on 6.0.0.800-96dd669e
"""
import pytest
from firebird.qa import *
import time

SUBROUTINES_COUNT = 2000 # 31.05.2025: 19 - broken line 'BLR to Source mapping:', see #8590.
BLR_TO_SOURCE_TXT = 'BLR to Source mapping:'

db = db_factory()
substitutions = [ ('^((?!' + BLR_TO_SOURCE_TXT + '|Sub function|Sub procedure).)*$', '') ]
act = python_act('db', substitutions = substitutions)

expected_lst = []

ddl_sp = []
ddl_fn = []
ddl_pkg = []
ddl_pbd = []

# ...................................................
# ...   f u n c t i o n,    s t a n d a l o n e   ...
# ...................................................
expected_lst.append(BLR_TO_SOURCE_TXT)
ddl_fn = [  'create function fn_main returns int as' ]
for i in range(SUBROUTINES_COUNT):
    # Inner functions:
    ddl_fn.extend( [f'  declare function fn_main_sub_func_{i:05} returns int as', '  begin', f'    return {i};', '  end'] )

for i in range(SUBROUTINES_COUNT):
    # Inner procedures:
    ddl_fn.extend( [f'  declare procedure fn_main_sub_proc_{i:05} as', '    declare n int;', '  begin', f'    n = {i};', '  end'] )
ddl_fn.extend( ['begin', '  return 1;', 'end'] )

# Add lines that must be in expected output. NOTE: inner functions are shown always BEFORE procedures!
for suffix in ('sub_func', 'sub_proc'):
    sub_routine_type = 'function' if suffix == 'sub_func' else 'procedure'
    for i in range(SUBROUTINES_COUNT):
        expected_lst.extend([f'Sub {sub_routine_type} FN_MAIN_{suffix.upper()}_{i:05}:', BLR_TO_SOURCE_TXT])


# .....................................................
# ...   p r o c e d u r e,    s t a n d a l o n e   ...
# .....................................................
expected_lst.append(BLR_TO_SOURCE_TXT)
ddl_sp = [  'create procedure sp_main as' ]
for i in range(SUBROUTINES_COUNT):
    # Inner functions:
    ddl_sp.extend( [f'  declare function sp_main_sub_func_{i:05} returns int as', '  begin', f'    return {i};', '  end'] )

for i in range(SUBROUTINES_COUNT):
    # Inner procedures:
    ddl_sp.extend( [f'  declare procedure sp_main_sub_proc_{i:05} as', '    declare n int;', '  begin', f'    n = {i};', '  end'] )
ddl_sp.extend( ['begin', 'end'] )

# Add lines that must be in expected output. NOTE: inner functions are shown always BEFORE procedures!
for suffix in ('sub_func', 'sub_proc'):
    sub_routine_type = 'function' if suffix == 'sub_func' else 'procedure'
    for i in range(SUBROUTINES_COUNT):
        expected_lst.extend([f'Sub {sub_routine_type} SP_MAIN_{suffix.upper()}_{i:05}:', BLR_TO_SOURCE_TXT])


# .............................................................
# ...   p a c k a g e    w i t h    f u n c    a n d    p r o c
# .............................................................
ddl_pkg = [  'create package pg_test as', 'begin' ]
ddl_pkg.extend( ['  function packaged_func returns int;'] )
ddl_pkg.extend( ['  procedure packaged_proc;'] )
ddl_pkg.extend( ['end'] )  # end of package header

ddl_pbd = [  'create package body pg_test as', 'begin' ]

# packaged function with <SUBROUTINES_COUNT> inner units:
# -------------------------------------------------------
ddl_pbd.extend( ['  function packaged_func returns int as' ] )
for i in range(SUBROUTINES_COUNT):
    ddl_pbd.extend( [f'    declare function pg_func_sub_func_{i:05} returns int as', '    begin', f'      return 1;', '    end'] )
    ddl_pbd.extend( [f'    declare procedure pg_func_sub_proc_{i:05} as', '      declare n int;', '    begin', f'      n = 1;', '    end'] )
ddl_pbd.extend( ['  begin', '  end'] )


# packaged procedure with <SUBROUTINES_COUNT> inner units:
# --------------------------------------------------------
ddl_pbd.extend( ['  procedure packaged_proc as' ] )
for i in range(SUBROUTINES_COUNT):
    ddl_pbd.extend( [f'    declare function pg_proc_sub_func_{i:05} returns int as', '    begin', f'      return 1;', '    end'] )
    ddl_pbd.extend( [f'    declare procedure pg_proc_sub_proc_{i:05} as', '      declare n int;', '    begin', f'      n = 1;', '    end'] )

ddl_pbd.extend( ['  begin', '  end'] )

ddl_pbd.extend( ['end'] ) # end of package body

# Add lines that must be in expected output. NOTE: inner functions are shown always BEFORE procedures!
for prefix in ('pg_func', 'pg_proc'):
    expected_lst.append(BLR_TO_SOURCE_TXT)
    for i in range(SUBROUTINES_COUNT):
        expected_lst.extend([f'Sub function {prefix.upper()}_SUB_FUNC_{i:05}:', BLR_TO_SOURCE_TXT])

    for i in range(SUBROUTINES_COUNT):
        expected_lst.extend([f'Sub procedure {prefix.upper()}_SUB_PROC_{i:05}:', BLR_TO_SOURCE_TXT])

#####################################################################################################

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    with act.db.connect(charset = 'utf8') as con:

        for ddl_cmd_lst in (ddl_sp, ddl_fn, ddl_pkg, ddl_pbd):
            if ddl_cmd_lst:
                ddl_cmd_txt = '\n'.join([x.strip() for x in ddl_cmd_lst])
                #print(len(ddl_cmd_txt))
                con.execute_immediate( ddl_cmd_txt )
                con.commit()
    
    rdb_query_data = (
        ('rdb$functions',  'rdb$function_name',  'fn_main', None)
       ,('rdb$procedures', 'rdb$procedure_name', 'sp_main', None)
       ,('rdb$functions',  'rdb$function_name',  'packaged_func', 'pg_test')
       ,('rdb$procedures', 'rdb$procedure_name', 'packaged_proc', 'pg_test')
    )

    for p in rdb_query_data:
        
        rdb_table_name, rdb_field_name, rdb_field_value, rdb_package_name = p

        check_sql = f"""
            set heading off;
            set blob all;
            select p.rdb$debug_info as blob_debug_info
            from {rdb_table_name} p
            where p.{rdb_field_name} = '{rdb_field_value.upper()}'
        """
        if rdb_package_name:
            check_sql += f" and p.rdb$package_name = '{rdb_package_name.upper()}';"
        else:
            check_sql += f" and p.rdb$package_name is null;"

        act.isql(switches = ['-q'], input = check_sql, combine_output = True)
        print(act.stdout)
        act.reset()
    
    act.expected_stdout = '\n'.join(expected_lst)
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
