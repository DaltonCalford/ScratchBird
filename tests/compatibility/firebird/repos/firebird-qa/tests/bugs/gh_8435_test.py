#coding:utf-8

"""
ID:          issue-8435
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8435
TITLE:       Error "Can not transliterate character" with text concatention utf8 + win1250
DESCRIPTION:
NOTES:
    [31.05.2025] pzotov
    Confirmed bug on 6.0.0.702.
    Checked on intermediate snapshot 6.0.0.707-487f423
    Thanks to Adriano for provided example to reproduce bug.
"""
import pytest
from firebird.qa import *

db = db_factory()
CHECKED_TEXT = "Kwota faktury płatna wyłącznie na rachunek ING Commercial Finance Polska S.A.,  ul. Malczewskiego 45, 02-622 Warszawa, www.ingcomfin.pl  Bank: ING Bank Śląski  rachunek nr: PL33 1050 0099 5381 0000 1000 9471  któremu zbyliśmy nasze wierzytelności łącznie z niniejszą."

test_script = f"""
    set blob all;
    set list on;
    select _utf8 'u' || cast(_utf8 '{CHECKED_TEXT}' as blob character set win1250) blob_id from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[ ('BLOB_ID .*', ''), ('[ \\t]+', ' ') ])

@pytest.mark.intl
@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        u{CHECKED_TEXT}
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
