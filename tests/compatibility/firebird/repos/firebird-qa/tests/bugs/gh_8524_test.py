#coding:utf-8

"""
ID:          issue-8524
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8524
TITLE:       ISQL will truncate lines longer than 255 when pasting
DESCRIPTION:
    Test creates temporary batch file with requirement to perform following commands:
    ==========
    chcp 65001
    {act.vars['isql']} -q
    exit
    ==========
    This batch further is called as CHILD process via: 'cmd.exe /c start <this_batch>' which will launch ISQL. 
    ISQL process will have console window (which will appear on screen) and we will try to paste some text in it.

    Text may contain non-printable / non-readable characters, so we have to check not the result of pasting but
    the value of CHAR_LENGTH(<pasted_text>). This is done by creating a table and using INSERT command:
    =========
    insert into test(s_utf8) values(
        '<long_utf8_data>'
    ) returning char_length(s_utf8);
    =========
    Result of this command is redirected to log.
    Finally, we open this log and parse it in order to see there: 'CHAR_LENGTH {checked_char_len}'
    test can be considered as successful if we actually found this line.
NOTES:
    [27.05.2025] pzotov
    0. Package 'pywin32' must be installed for this test (pip install pywin32).
    1. Problem could be reproduced only on Windows. Emulation of PASTE required (PIPE will not show any problem).
       See also:
           https://github.com/FirebirdSQL/firebird/issues/7202
    2. Test was added just to be able to check result of pasting long text in existing ISQL console.
       This test will not able to create console window if python was launched by scheduler and
       appropriate task has not set to: "Run only when user is logged on".
       Because of that, this test has '@pytest.mark.skip' marker.
       ##################################################
       REMOVE OR COMMENT OUT THIS MARKER TO RUN THIS TEST
       ##################################################

    3. String that we want to be pasted into console must NOT have two adjacent characters, e.g. 'commit'.
       Otherwise only first of them will be actually pasted but others will be 'swallowed' for unknown reason.
    4. Every line of <send_sql> (i.e. text to be pasted in console of ISQL) must be prefixed with TWO characters: ascii_char(13) and ascii_char(10).
       It is NOT enough to use as prefix only CHR_10 at this case!
    5. Command processor ('cmd.exe') must change code page to 65001 before running ISQL, thus we launch BATCH rather than just ISQL (as child process).

    Checked on 6.0.0.789.
"""

import os
import random
import subprocess
import time
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory(charset = 'utf8')

#-----------------------------------------------------------

def get_random_unicode(length):

    # https://jrgraphix.net/r/Unicode/
    UNICODE_RANGES_MAP = {
        (0x0028, 0x007E) : 'Basic Latin WITHOUT apostrophe and control character <DELETE>, 0x7F',
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

    alphabet = [
        chr(code_point) for current_range in UNICODE_RANGES_MAP.keys()
            for code_point in range(current_range[0], current_range[1] + 1)
    ]
    return ''.join(random.choice(alphabet) for i in range(length))


#-----------------------------------------------------------
def send_keys_to_window(window_title, keys_to_send):

    import win32gui
    import win32con

    window_id = win32gui.FindWindow(None, window_title)
    assert window_id > 0, f"Window with title '{window_title}' not found. Script must run only when user is logged on."

    win32gui.SetForegroundWindow(window_id)
    time.sleep(0.1)  # Give the window time to activate

    for key in keys_to_send:
        if key.isupper():
            win32gui.SendMessage(window_id, win32con.WM_KEYDOWN, win32con.VK_SHIFT, 0)
            win32gui.SendMessage(window_id, win32con.WM_CHAR, ord(key), 0)
            win32gui.SendMessage(window_id, win32con.WM_KEYUP, win32con.VK_SHIFT, 0)
        elif key == '\n':
             win32gui.SendMessage(window_id, win32con.WM_KEYDOWN, win32con.VK_RETURN, 0)
             win32gui.SendMessage(window_id, win32con.WM_KEYUP, win32con.VK_RETURN, 0)
        else:
            win32gui.SendMessage(window_id, win32con.WM_CHAR, ord(key), 0)
#-----------------------------------------------------------

MAX_WAIT_FOR_ISQL_FINISH_S = 20

test_script = """
"""

act = python_act('db', substitutions=[ ('[ \\t]+', ' ') ])

isql_out = temp_file('tmp_8524.log')
isql_bat = temp_file('tmp_8524.bat')
utf8_dat = temp_file('tmp_8524.dat') # for debug only

expected_stdout = """
"""

@pytest.mark.version('>=4.0.6')
@pytest.mark.platform('Windows')
@pytest.mark.skip("Can not run when user is logged out. Child process must run in console window.")

def test_1(act: Action, isql_out: Path, utf8_dat: Path, isql_bat: Path, capsys):

    # ::: ACHTUNG :::
    # The whole data that will be pasted in ISQL console must not have two same adjacent characters.
    # For example, 'commit' will be 'comit' --> token unknown etc.
    # The same for unicode string: second character (of two adjacent ones) will be 'swallowed'!
    # The reason of this weird behaviour remained unknown.
    #
    #long_utf8_data = ''.join(set(get_random_unicode(8190)))

    long_utf8_data = '∑∏Ω' * (8191//3)
    checked_char_len = len(long_utf8_data)
    send_sql = f"""
        set names utf8;
        set list on;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        show version;
        recreate table test( s_utf8 varchar(8191) );
        out {str(isql_out)};
        insert into test(s_utf8) values(
            '{long_utf8_data}'
        ) returning char_length(s_utf8);
        out;
        exit;

    """

    with open(utf8_dat, 'w', encoding = 'utf8') as f:
        f.write(send_sql)

    # Child processes cmd.exe or isql.exe will use code page = 866.
    # We have to set BEFOREHAND codepage = 65001 otherwise 'malformed string' will raise in ISQL console, even when use charset = utf8.
    # To to this, we have to launch as child process BATCH file rather than just cmd.exe because there is no way to start sopmewhat like
    # 'cmd.exe /cp 65001' with set needed codepage at one command.
    #
    launch_commands = f"""
        cls
        chcp 65001
        {act.vars['isql']} -q
        exit
    """
    with open(isql_bat, 'w') as f:
        f.write(launch_commands)


    # WRONG (can lead to 'malformed string' because we did not set codepage to 65001):
    #p_child = subprocess.run( [ 'cmd.exe', '/c', 'start', act.vars['isql'], '-q' ] ) # , stdout = f, stderr = subprocess.STDOUT)

    p_child = subprocess.run( [ 'cmd.exe', '/c', 'start', str(isql_bat) ] )
    time.sleep(1)

    # -----------------------------------------------------------
    # ::: NB :::
    # Every line of <send_sql> (i.e. text to be pasted in console of ISQL) must be prefixed with TWO characters: ascii_char(13) and ascii_char(10).
    # It is NOT enough to use as prefix only CHR_10 at this case!
    # -----------------------------------------------------------

    #send_keys_to_window( str(act.vars['isql']), '\r\n'.join( [x.lstrip() for x in send_sql.splitlines()] ) )
    send_keys_to_window( os.getenv('SystemRoot') + '\\system32\\cmd.exe - ' + str(isql_bat), '\r\n'.join( [x.lstrip() for x in send_sql.splitlines()] ) )

    time.sleep(1)
    try:
        p_child.wait(MAX_WAIT_FOR_ISQL_FINISH_S) 
        p_child.terminate()
    except AttributeError: # 'CompletedProcess' object has no attribute 'wait'   
        pass

    with open(isql_out, mode='r') as f:
        for line in f:
            print(line)

    act.expected_stdout = f"""
        CHAR_LENGTH {checked_char_len}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
