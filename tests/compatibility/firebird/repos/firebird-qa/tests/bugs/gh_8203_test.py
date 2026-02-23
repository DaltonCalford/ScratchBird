#coding:utf-8

"""
ID:          issue-8203
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8203
TITLE:       MAKE_DBKEY can raise 'malformed string' for some table names
DESCRIPTION:
    Test verifies ability to create table with random name for each of Unicode ranges
    defined in https://jrgraphix.net/r/Unicode/, except following:
       (0xD800, 0xDB7F), # High Surrogates
       (0xDB80, 0xDBFF), # High Private Use Surrogates
       (0xDC00, 0xDFFF), # Low Surrogates
    Random name is generated for each range, with random length from scope NAME_MIN_LEN ... NAME_MAX_LEN scope.
    Then we create table with such name and stored procedure that attempts to use make_dbkey() with 1st argument
    equals to just created table.
    This action is repeated REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE times for each Unicode range.
    Some characters from 'Basic Latin' are NOT included in any table names - see CHARS_TO_SKIP.
    No error must raise for any of checked Unicode scopes.
    Example of output when problem does exist:
        iter=4 of REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE=5: SUCCESS
        range_name='Basic Latin', ..., table_random_unicode_name='}JIry@frnWdzb]5[:A=IomGozwyM*rmJ'
        Error while parsing procedure SP_CHK's BLR
        -Malformed string
        err.gds_codes=(335544876, 335544849)
        err.sqlcode=-901
        err.sqlstate='2F000'
NOTES:
    [11.08.2024] pzotov
        Confirmed bug on 6.0.0.421, 5.0.1.1469
        Checked on 6.0.0.423, 5.0.2.1477
    [06.07.2025] pzotov
        ::: NB ::: See doc/sql.extensions/README.schemas.md 
        When working with object names in ... `MAKE_DBKEY`, the names containing special characters or lowercase
        letters must be enclosed in quotes ... In earlier versions, `MAKE_DBKEY` required an exact table name as
        its first parameter and did not support the use of double quotes for special characters.
        ----------------------------------------------------------------------------------------
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.914; 5.0.3.1668.
"""
import pytest
from firebird.qa import *
from io import BytesIO
from firebird.driver import SrvRestoreFlag, DatabaseError
import locale
import random

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

#########################
###  s e t t i n g s  ###
#########################
CHARS_TO_SKIP = set('<>|"\'^')
NAME_MIN_LEN = 32
NAME_MAX_LEN = 63
REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE = 5 # duration: ~60"

#------------------------------------------------

def get_random_unicode(length, bound_points):
    # https://stackoverflow.com/questions/1477294/generate-random-utf-8-string-in-python
    try:
        get_char = unichr
    except NameError:
        get_char = chr

    alphabet = [
        get_char(code_point) for code_point in range(bound_points[0],bound_points[1])
    ]
    return ''.join(random.choice(alphabet) for i in range(length))

#------------------------------------------------
    
@pytest.mark.version('>=5.0.2')
def test_1(act: Action, capsys):

    # https://jrgraphix.net/r/Unicode/
    UNICODE_RANGES_MAP = {
        (0x0020, 0x007F) : 'Basic Latin',
        (0x00A0, 0x00FF) : 'Latin-1 Supplement',
        (0x0100, 0x017F) : 'Latin Extended-A',
        (0x0180, 0x024F) : 'Latin Extended-B',
        (0x0400, 0x04FF) : 'Cyrillic',
        (0x0500, 0x052F) : 'Cyrillic Supplementary',
        (0x0300, 0x036F) : 'Combining Diacritical Marks',
        (0x0250, 0x02AF) : 'IPA Extensions',
        (0x0370, 0x03FF) : 'Greek and Coptic',
        (0x0530, 0x058F) : 'Armenian',
        (0x02B0, 0x02FF) : 'Spacing Modifier Letters',
        (0x0590, 0x05FF) : 'Hebrew',
        (0x0600, 0x06FF) : 'Arabic',
        (0x0700, 0x074F) : 'Syriac',
        (0x0780, 0x07BF) : 'Thaana',
        (0x0900, 0x097F) : 'Devanagari',
        (0x0980, 0x09FF) : 'Bengali',
        (0x0A00, 0x0A7F) : 'Gurmukhi',
        (0x0A80, 0x0AFF) : 'Gujarati',
        (0x0B00, 0x0B7F) : 'Oriya',
        (0x0B80, 0x0BFF) : 'Tamil',
        (0x0C00, 0x0C7F) : 'Telugu',
        (0x0C80, 0x0CFF) : 'Kannada',
        (0x0D00, 0x0D7F) : 'Malayalam',
        (0x0D80, 0x0DFF) : 'Sinhala',
        (0x0E00, 0x0E7F) : 'Thai',
        (0x0E80, 0x0EFF) : 'Lao',
        (0x0F00, 0x0FFF) : 'Tibetan',
        (0x1000, 0x109F) : 'Myanmar',
        (0x10A0, 0x10FF) : 'Georgian',
        (0x1100, 0x11FF) : 'Hangul Jamo',
        (0x1200, 0x137F) : 'Ethiopic',
        (0x13A0, 0x13FF) : 'Cherokee',
        (0x1400, 0x167F) : 'Unified Canadian Aboriginal Syllabics',
        (0x1680, 0x169F) : 'Ogham',
        (0x16A0, 0x16FF) : 'Runic',
        (0x1700, 0x171F) : 'Tagalog',
        (0x1720, 0x173F) : 'Hanunoo',
        (0x1740, 0x175F) : 'Buhid',
        (0x1760, 0x177F) : 'Tagbanwa',
        (0x1780, 0x17FF) : 'Khmer',
        (0x1800, 0x18AF) : 'Mongolian',
        (0x1900, 0x194F) : 'Limbu',
        (0x1950, 0x197F) : 'Tai Le',
        (0x19E0, 0x19FF) : 'Khmer Symbols',
        (0x1D00, 0x1D7F) : 'Phonetic Extensions',
        (0x1E00, 0x1EFF) : 'Latin Extended Additional',
        (0x1F00, 0x1FFF) : 'Greek Extended',
        (0x2000, 0x206F) : 'General Punctuation',
        (0x2070, 0x209F) : 'Superscripts and Subscripts',
        (0x20A0, 0x20CF) : 'Currency Symbols',
        (0x20D0, 0x20FF) : 'Combining Diacritical Marks for Symbols',
        (0x2100, 0x214F) : 'Letterlike Symbols',
        (0x2150, 0x218F) : 'Number Forms',
        (0x2190, 0x21FF) : 'Arrows',
        (0x2200, 0x22FF) : 'Mathematical Operators',
        (0x2300, 0x23FF) : 'Miscellaneous Technical',
        (0x2400, 0x243F) : 'Control Pictures',
        (0x2440, 0x245F) : 'Optical Character Recognition',
        (0x2460, 0x24FF) : 'Enclosed Alphanumerics',
        (0x2500, 0x257F) : 'Box Drawing',
        (0x2580, 0x259F) : 'Block Elements',
        (0x25A0, 0x25FF) : 'Geometric Shapes',
        (0x2600, 0x26FF) : 'Miscellaneous Symbols',
        (0x2700, 0x27BF) : 'Dingbats',
        (0x27C0, 0x27EF) : 'Miscellaneous Mathematical Symbols-A',
        (0x27F0, 0x27FF) : 'Supplemental Arrows-A',
        (0x2800, 0x28FF) : 'Braille Patterns',
        (0x2900, 0x297F) : 'Supplemental Arrows-B',
        (0x2980, 0x29FF) : 'Miscellaneous Mathematical Symbols-B',
        (0x2A00, 0x2AFF) : 'Supplemental Mathematical Operators',
        (0x2B00, 0x2BFF) : 'Miscellaneous Symbols and Arrows',
        (0x2E80, 0x2EFF) : 'CJK Radicals Supplement',
        (0x2F00, 0x2FDF) : 'Kangxi Radicals',
        (0x2FF0, 0x2FFF) : 'Ideographic Description Characters',
        (0x3000, 0x303F) : 'CJK Symbols and Punctuation',
        (0x3040, 0x309F) : 'Hiragana',
        (0x30A0, 0x30FF) : 'Katakana',
        (0x3100, 0x312F) : 'Bopomofo',
        (0x3130, 0x318F) : 'Hangul Compatibility Jamo',
        (0x3190, 0x319F) : 'Kanbun',
        (0x31A0, 0x31BF) : 'Bopomofo Extended',
        (0x31F0, 0x31FF) : 'Katakana Phonetic Extensions',
        (0x3200, 0x32FF) : 'Enclosed CJK Letters and Months',
        (0x3300, 0x33FF) : 'CJK Compatibility',
        (0x3400, 0x4DBF) : 'CJK Unified Ideographs Extension A',
        (0x4DC0, 0x4DFF) : 'Yijing Hexagram Symbols',
        (0x4E00, 0x9FFF) : 'CJK Unified Ideographs',
        (0xA000, 0xA48F) : 'Yi Syllables',
        (0xA490, 0xA4CF) : 'Yi Radicals',
        (0xAC00, 0xD7AF) : 'Hangul Syllables',
        (0xE000, 0xF8FF) : 'Private Use Area',
        (0xF900, 0xFAFF) : 'CJK Compatibility Ideographs',
        (0xFB00, 0xFB4F) : 'Alphabetic Presentation Forms',
        (0xFB50, 0xFDFF) : 'Arabic Presentation Forms-A',
        (0xFE00, 0xFE0F) : 'Variation Selectors',
        (0xFE20, 0xFE2F) : 'Combining Half Marks',
        (0xFE30, 0xFE4F) : 'CJK Compatibility Forms',
        (0xFE50, 0xFE6F) : 'Small Form Variants',
        (0xFE70, 0xFEFF) : 'Arabic Presentation Forms-B',
        (0xFF00, 0xFFEF) : 'Halfwidth and Fullwidth Forms',
        (0xFFF0, 0xFFFF) : 'Specials',
        (0x10000, 0x1007F) : 'Linear B Syllabary',
        (0x10080, 0x100FF) : 'Linear B Ideograms',
        (0x10100, 0x1013F) : 'Aegean Numbers',
        (0x10300, 0x1032F) : 'Old Italic',
        (0x10330, 0x1034F) : 'Gothic',
        (0x10380, 0x1039F) : 'Ugaritic',
        (0x10400, 0x1044F) : 'Deseret',
        (0x10450, 0x1047F) : 'Shavian',
        (0x10480, 0x104AF) : 'Osmanya',
        (0x10800, 0x1083F) : 'Cypriot Syllabary',
        (0x1D000, 0x1D0FF) : 'Byzantine Musical Symbols',
        (0x1D100, 0x1D1FF) : 'Musical Symbols',
        (0x1D300, 0x1D35F) : 'Tai Xuan Jing Symbols',
        (0x1D400, 0x1D7FF) : 'Mathematical Alphanumeric Symbols',
        (0x20000, 0x2A6DF) : 'CJK Unified Ideographs Extension B',
        (0x2F800, 0x2FA1F) : 'CJK Compatibility Ideographs Supplement',
        (0xE0000, 0xE007F) : 'Tags',
    }

    for bound_points, range_name in UNICODE_RANGES_MAP.items():

        for iter in range(1,REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE+1):
            
            table_random_unicode_name = get_random_unicode( random.randint(NAME_MIN_LEN, NAME_MAX_LEN), bound_points )
            table_random_unicode_name = ''.join(c for c in table_random_unicode_name if c not in CHARS_TO_SKIP)
            table_random_uname_quoted = table_random_unicode_name.replace('"','""')
            if act.is_version('<6'):
                test_sql = f"""
                    recreate table "{table_random_uname_quoted}"(id int)
                    ^
                    create or alter procedure sp_chk as
                        declare id1 int;
                    begin
                        select /* {range_name=} {iter=} */ id
                        from "{table_random_uname_quoted}"
                        where rdb$db_key = make_dbkey('{table_random_unicode_name}', 0)
                        into id1;
                    end
                    ^
                """
            else:
                # ::: NB ::: See doc/sql.extensions/README.schemas.md 
                # When working with object names in ... `MAKE_DBKEY`, the names containing special characters or lowercase
                # letters must be enclosed in quotes ... In earlier versions, `MAKE_DBKEY` required an exact table name as
                # its first parameter and did not support the use of double quotes for special characters.
                #
                test_sql = f"""
                    recreate table "{table_random_uname_quoted}"(id int)
                    ^
                    create or alter procedure sp_chk as
                        declare id1 int;
                    begin
                        select /* {range_name=} {iter=} */ id
                        from "{table_random_uname_quoted}"
                        where rdb$db_key = make_dbkey('"{table_random_uname_quoted}"', 0)
                        --                             |                           |
                        --                             |                           |
                        --                             +----- required in  6.x ----+
                        into id1;
                    end
                    ^
                """

            expected_txt = f'{iter=} of {REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE=}: SUCCESS'
            with act.db.connect(charset = 'utf-8') as con:
                try:
                    for line in test_sql.split('^'):
                        if (expr := line.strip()):
                            if expr != '^':
                                con.execute_immediate(expr)
                            else:
                                con.commit()
                    con.commit()
                    print(expected_txt)
                except DatabaseError as err:
                    print(f'{range_name=}, {iter=} of {REPEAT_CHECKS_FOR_SELECTED_UNICODE_RANGE=}, {table_random_unicode_name=}')
                    print(err)
                    print(f'{err.gds_codes=}')
                    print(f'{err.sqlcode=}')
                    print(f'{err.sqlstate=}')
            '''
            backup = BytesIO()
            with act.connect_server() as srv:
                srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
                backup.seek(0)
                srv.database.local_restore(backup_stream=backup, database=act.db.db_path, flags = SrvRestoreFlag.REPLACE)
            '''
            act.expected_stdout = expected_txt
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()
