#coding:utf-8

"""
ID:          issue-8452
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8452
TITLE:       Disallow creation of object names with only spaces
DESCRIPTION:
NOTES:
    [03.04.2025] pzotov
    1. Test verifies ability to create DB with name = <non-double quote character> that belongs
       to following unicode ranges:
           * [SPACE CHARACTER] // ascii-code = 32 
           * https://www.compart.com/en/unicode/category/Cc : BEL, TAB, LF, CR
           * https://www.compart.com/en/unicode/category/Cf : 0xAD ("soft hyhen")
           * https://www.compart.com/en/unicode/category/Zl : 0x2028, Line Separator
           * https://www.compart.com/en/unicode/category/Zp : 0x2029, Paragraph Separator

    2. Currently FB *allows* to create databases with single characters from these range,
       except space (ascii 32) -- this is prohibited after fix.
       Characters from other ranges are NOT prohibited to be used as DB name!
       See comment by Adriano:
       https://github.com/FirebirdSQL/firebird/pull/8473#issuecomment-2738617159

       Following unicode category currently SKIPPED:
           * https://www.compart.com/en/unicode/category/Cs : (0xD800, 0xDB7F) High Surrogates
       (got: SyntaxError: (unicode error) 'utf-8' codec can't decode byte 0xed in position 2061: invalid continuation byte)

    3. Ability to use space-only names in PSQL currently not checked.

    Database which name ' ' (single space char) could be created up to 6.0.0.677.
    Checked on 6.0.0.710 - "spaces-only" DB name no more allowed.
"""
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions = [('-At line \\d+, column \\d+', '')])

@pytest.mark.version('>=6.0')
def test_1(act: Action):


    test_sql = """
    -- from https://github.com/FirebirdSQL/firebird/issues/8452:
    -- A <non-double quote character> is any character of the source language
    -- character set other than a <double quote> that is not included in the
    -- Unicode General Categories “Cc”, “Cf”, “Cn”, “Cs”, “Zl”, or “Zp”.

    -- https://www.compart.com/en/unicode/category/Cc : BEL, TAB, LF, CR
    -- https://www.compart.com/en/unicode/category/Cf : 0xAD ("soft hyhen")
    -- https://www.compart.com/en/unicode/category/Cs : (0xD800, 0xDB7F) High Surrogates
    -- https://www.compart.com/en/unicode/category/Zl : 0x2028, Line Separator
    -- https://www.compart.com/en/unicode/category/Zp : 0x2029, Paragraph Separator

    set bail off;

    -- SPACE character only:
    recreate table " " (id int);

    -- ############################################

    -- Cc category:

    -- "t" + CR + LF:
    recreate table "t
    " (id int);
    comment on table "t
    " is '"t" + CR + LF';

    -- CR + LF:
    recreate table "
    " (id int);
    comment on table "
    " is 'CR + LF';

    -- LF only:
    recreate table "
    " (id int);
    comment on table "
    " is 'Cc: LF only';

    -- CR only:
    recreate table "
    " (id int);
    comment on table "
    " is 'Cc: CR only';

    -- TAB character:
    recreate table "	" (id int);
    comment on table "	" is 'Cc: TAB character';

    -- BEL character:
    recreate table "" (id int);
    comment on table "" is 'Cc: BEL character';

    -- ############################################

    -- Cf category:
    -- `SHY;`  aka SoftHyphen
    recreate table "­" (id int);
    comment on table "­" is 'Cf: SoftHyphen';

    -- ############################################

    -- Zl category:
    recreate table " " (id int); -- 0x2028
    comment on table " " is 'Zl: Line separator';

    -- ############################################

    -- Zp category:
    recreate table " " (id int); -- 0x2029
    comment on table " " is 'Zp: Paragraph Separator';

    commit;

    set count on;
    set list on;
    select unicode_val(rdb$relation_name) as rel_name_uval, cast(rdb$description as varchar(1024)) as rel_comment from rdb$relations
    where rdb$system_flag is distinct from 1
    order by rdb$relation_id
    ;
    set count off;
    """
    
    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Zero length identifiers are not allowed

        REL_NAME_UVAL                   116
        REL_COMMENT                     "t" + CR + LF

        REL_NAME_UVAL                   10
        REL_COMMENT                     Cc: CR only

        REL_NAME_UVAL                   9
        REL_COMMENT                     Cc: TAB character

        REL_NAME_UVAL                   7
        REL_COMMENT                     Cc: BEL character

        REL_NAME_UVAL                   173
        REL_COMMENT                     Cf: SoftHyphen

        REL_NAME_UVAL                   8232
        REL_COMMENT                     Zl: Line separator

        REL_NAME_UVAL                   8233
        REL_COMMENT                     Zp: Paragraph Separator
        Records affected: 7
    """
    act.expected_stdout = expected_stdout
    act.isql(input = test_sql, combine_output=True, charset = 'utf8')
    assert act.clean_stdout == act.clean_expected_stdout
