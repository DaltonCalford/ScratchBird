#coding:utf-8

"""
ID:          exception.handling-name-and-message
FBTEST:      functional.exception.handling_name_and_message
TITLE:       Context variables EXCEPTION and ERROR_MESSAGE for ability to log exception info (including call stack!) on server side
DESCRIPTION:
  Testing new built-in context variables for exception handling (appearance: 06-sep-2016 21:12, 4.0 only):
  * 'exception' -- returns name of the active user-defined exception;
  * 'error_message' -- returns interpreted text for the active exception.
  See: https://github.com/FirebirdSQL/firebird/commit/ebd0d3c8133c62b5359100de5f1eec541e43da3b
  Explanation: doc\\sql.extensions\\README.context_variables

  GOOD NEWS:
  call stack now can be logged on database without calling of mon$ tables,
  simple by parsing 'error_message' content (part that follows by last newline character).

  WARNING-1.
  This test intentionally creates exception with non-ascii name and parametrized non-ascii message text.
  Length of exception *NAME* can be up to 63 non-ascii characters, but upper bound for length of exception
  *MESSAGE* is limited to 1023 *bytes* (NOT chars!) ==> it's max length for two-byte encoding (win1251 etc)
  will be usually much less, about 500...600 characters. This limit can not be overpassed nowadays.
  For database with default cset = utf8 table rdb$exception will have following DDL:
    RDB$EXCEPTION_NAME              (RDB$EXCEPTION_NAME) CHAR(63) Nullable
    RDB$MESSAGE                     (RDB$MESSAGE) VARCHAR(1023) CHARACTER SET NONE Nullable
    Checked on 4.0.0.366

  WARNING-2.
  It seems that handling of message with length = 1023 bytes (i.e. exactly upper limit) works wrong.
  Waiting for reply from dimitr, letter 09-sep-2016 18:27.
NOTES:
[07.12.2016]
  'exception' and 'error_message' context variables were replaced with calls RDB$ERROR(EXCEPTION) and RDB$ERROR(MESSAGE)
  (letter from dimitr, 06.12.2016 21:44; seems that this was done in 4.0.0.461, between 02-dec-2016 and 04-dec-2016)
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    recreate exception  "Что-то неправильно со складом" 'Остаток стал отрицательным: @1';

    /*

    This will be added after trouble with length = 1023 characters will be solved (TODO after):

    recreate exception "ЙцуКенгШщзХъЭждЛорПавЫфЯчсмиТьбЮЪхЗщШШГнЕкУцЙФывААпрОолДжЭЭюБьТ"
    '*Лев Николаевич Толстой * *Анна Каренина * /Мне отмщение, и аз воздам/ *ЧАСТЬ ПЕРВАЯ* *I *
    Все счастливые семьи похожи друг на друга, каждая несчастливая
    семья несчастлива по-своему.
    Все смешалось в доме Облонских. Жена узнала, что муж был в связи
    с бывшею в их доме француженкою-гувернанткой, и объявила мужу, что
    не может жить с ним в одном доме. Положение это продолжалось уже
    третий день и мучительно чувствовалось и самими супругами, и всеми
    членами семьи, и домочадцами. Все члены семьи и домочадцы
    чувствовали, что нет смысла в их сожительстве и что на каждом
    п1
    ';
    */

    recreate table log_user_trouble(
        e_declared_name varchar(63) character set utf8,
        e_detailed_text varchar(2000) character set utf8
    );

    set term ^;
    create or alter procedure sp_log_user_trouble(
        e_declared_name varchar(63) character set utf8,
        e_detailed_text varchar(2000) character set utf8
    ) as
    begin
        in autonomous transaction do
            insert into log_user_trouble(e_declared_name, e_detailed_text) values(:e_declared_name, :e_detailed_text);
    end
    ^
    set term ;^
    commit;

    set list on;

    /*
    show table rdb$exceptions;
    select
        rdb$exception_name,
        char_length(trim(rdb$exception_name)) exc_name_char_len,
        octet_length(trim(rdb$exception_name)) exc_name_octt_len,
        char_length(rdb$message) detailed_text_char_len,
        octet_length(rdb$message)  detailed_text_octt_len
    from rdb$exceptions;
    --*/

    -- RDB$ERROR(GDSCODE|SQLCODE|SQLSTATE|EXCEPTION|MESSAGE)
    set term  ^;
    create or alter procedure sp_check_amount(a_new_qty int) as
    begin
        begin
            if (a_new_qty < 0) then
                exception "Что-то неправильно со складом" using( a_new_qty );
                --exception "ЙцуКенгШщзХъЭждЛорПавЫфЯчсмиТьбЮЪхЗщШШГнЕкУцЙФывААпрОолДжЭЭюБьТ"; -- malformed string

            when any do
                if ( RDB$ERROR(EXCEPTION) is not null) then
                    -- before 4.0.0.462 (04.12.2016): execute procedure sp_log_user_trouble(exception, error_message);
                    execute procedure sp_log_user_trouble( RDB$ERROR(EXCEPTION), RDB$ERROR(MESSAGE) );
                else
                    exception;

        end

    end
    ^

    create or alter procedure sp_run_write_off(a_new_qty int) as
        declare old_qty int = 1;
        declare new_qty int;
    begin
        new_qty = old_qty - a_new_qty;
        execute procedure sp_check_amount( new_qty );
    end
    ^

    create or alter procedure "главная точка входа" as
    begin
        execute procedure sp_run_write_off(9);
    end
    ^
    set term ;^
    commit;

    set term ^;
    execute block as
    begin
        execute procedure "главная точка входа";
    end
    ^
    set term ;^
    commit;

    set count on;
    select * from log_user_trouble;
"""

act = isql_act('db', test_script, substitutions=[('line:\\s[0-9]+,', 'line: x'),
                                                 ('col:\\s[0-9]+', 'col: y')])

@pytest.mark.intl
@pytest.mark.version('>=4.0')
def test_1(act: Action):

    expected_stdout_5x = """
       E_DECLARED_NAME                 Что-то неправильно со складом
       E_DETAILED_TEXT                 exception 1
       Что-то неправильно со складом
       Остаток стал отрицательным: -8
       At procedure 'SP_CHECK_AMOUNT' line: x col: y
       At procedure 'SP_RUN_WRITE_OFF' line: x col: y
       At procedure 'главная точка входа' line: x col: y
       At block line: x col: y
       Records affected: 1
    """

    expected_stdout_6x = """
       E_DECLARED_NAME                 "PUBLIC"."Что-то неправильно со складом"
       E_DETAILED_TEXT                 exception 1
       "PUBLIC"."Что-то неправильно со складом"
       Остаток стал отрицательным: -8
       At procedure "PUBLIC"."SP_CHECK_AMOUNT" line: x col: y
       At procedure "PUBLIC"."SP_RUN_WRITE_OFF" line: x col: y
       At procedure "PUBLIC"."главная точка входа" line: x col: y
       At block line: x col: y
       Records affected: 1
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
