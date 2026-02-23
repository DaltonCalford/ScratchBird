#coding:utf-8

"""
ID:          issue-2119
ISSUE:       2119
TITLE:       Bug in create/alter Database trigger (with Russian comment)
DESCRIPTION:
JIRA:        CORE-1694
FBTEST:      bugs.core_1694
NOTES:
    [25.06.2025] pzotov
    Re-implemented: use variables for storing non-ascii text and f-notation to sustitute them.
    Use alternate string quoting to avoid duplicating of apostrophes inside statement passed to ES.

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

HEAD_COMMENT_1 = """
    /*
    — Eh bien, mon prince. Gênes et Lucques ne sont plus que des apanages, des поместья,
    de la famille Buonaparte. Non, je vous préviens que si vous ne me dites pas que nous
    avons la guerre, si vous vous permettez encore de pallier toutes les infamies, toutes
    les atrocités de cet Antichrist (ma parole, j'y crois) — je ne vous connais plus, vous
    n'êtes plus mon ami, vous n'êtes plus мой верный раб, comme vous dites.
    Ну, здравствуйте, здравствуйте. Je vois que je vous fais peur, садитесь и рассказывайте.
    Так говорила в июле 1805 года известная Анна Павловна Шерер, фрейлина и приближенная
    императрицы Марии Феодоровны, встречая важного и чиновного князя Василия, первого
    приехавшего на ее вечер. Анна Павловна кашляла несколько дней, у нее был грипп, как она
    говорила (грипп был тогда новое слово, употреблявшееся только редкими).
    */
"""

HEAD_COMMENT_2 = """
    /*
    Dieu, quelle virulente sortie! 4 — отвечал, нисколько не смутясь такою встречей, вошедший
    князь, в придворном, шитом мундире, в чулках, башмаках и звездах, с светлым выражением
    плоского лица. Он говорил на том изысканном французском языке, на котором не только говорили,
    но и думали наши деды, и с теми, тихими, покровительственными интонациями, которые свойственны
    состаревшемуся в свете и при дворе значительному человеку. Он подошел к Анне Павловне,
    поцеловал ее руку, подставив ей свою надушенную и сияющую лысину, и покойно уселся на диване.
    */
"""

HEAD_COMMENT_3 = """
    /*
    — Ne me tourmentez pas. Eh bien, qu'a-t-on décidé par rapport à la dépêche de Novosilzoff?
    Vous savez tout.
    — Как вам сказать? — сказал князь холодным, скучающим тоном. — Qu'a-t-on décidé? On a décidé
    que Buonaparte a brûlé ses vaisseaux, et je crois que nous sommes en train de brûler les nôtres 8.
    Князь Василий говорил всегда лениво, как актер говорит роль старой пиесы. Анна Павловна Шерер,
    напротив, несмотря на свои сорок лет, была преисполнена оживления и порывов.
    */
"""

BODY_COMMENT_1 = """
    /*
    Ах, не говорите мне про Австрию! Я ничего не понимаю, может быть, но Австрия никогда не хотела
    и не хочет войны. Она предает нас. Россия одна должна быть спасительницей Европы. Наш благодетель
    знает свое высокое призвание и будет верен ему. Вот одно, во что я верю. Нашему доброму и чудному
    */
"""

BODY_COMMENT_2 = """
    /*государю предстоит величайшая роль в мире, и он так добродетелен и хорош, что Бог не оставит его,
    и он исполнит свое призвание задавить гидру революции, которая теперь еще ужаснее в лице этого
    убийцы и злодея. Мы одни должны искупить кровь праведника. На кого нам надеяться, я вас спрашиваю?..
    */
"""

BODY_COMMENT_3 = """
    /*
    Англия с своим коммерческим духом не поймет и не может понять всю высоту души императора Александра
    */
"""

test_script = f"""
create domain varchar_domain as varchar(50) character set utf8 collate utf8;
create domain varchar_domain2 as varchar(50) character set utf8 collate utf8;
commit;

set term ^;
execute block as
begin
-- ###############################################################################
-- ### u s e  `q'<literal> <complex text with single/double quotes> <literal>` ###
-- ### see: $FB_HOME/doc/sql.extensions/README.alternate_string_quoting.txt    ###
-- ###############################################################################
execute statement q'#
create or alter trigger trg_conn active on connect position 0
as
    {HEAD_COMMENT_1}
    declare u int;
    declare variable new_var1 varchar(50) character set utf8 collate utf8 default 'Que voulez-vous ?';
    declare variable new_var3 type of varchar_domain default 'Что делать ?';
    declare variable new_var4 type of varchar_domain2 default 'Кто виноват ?';
    {HEAD_COMMENT_2}
    declare v int = 2;
    {HEAD_COMMENT_3}
    declare w int = 3;
begin
    u =
    {BODY_COMMENT_1}
    v +
    {BODY_COMMENT_2}
    w;
    {BODY_COMMENT_3}
end#'
;
end
^
set term ;^
commit;

show trigger trg_conn;
"""

act = isql_act('db', test_script,
                 substitutions=[('[+++].*', ''), ('[===].*', ''), ('Trigger text.*', '')])

@pytest.mark.version('>=3')
def test_1(act: Action):

    # 25.06.2025: name of DB objects now have schema prefix (since 6.0.0.834):
    #
    TRG_NAME = 'TRG_CONN' if act.is_version('<6') else 'PUBLIC.TRG_CONN'

    expected_stdout = f"""
    {TRG_NAME}, Sequence: 0, Type: ON CONNECT, Active
    as
        {HEAD_COMMENT_1}
        declare u int;
        declare variable new_var1 varchar(50) character set utf8 collate utf8 default 'Que voulez-vous ?';
        declare variable new_var3 type of varchar_domain default 'Что делать ?';
        declare variable new_var4 type of varchar_domain2 default 'Кто виноват ?';
        {HEAD_COMMENT_2}
        declare v int = 2;
        {HEAD_COMMENT_3}
        declare w int = 3;
    begin
        u =
        {BODY_COMMENT_1}
        v +
        {BODY_COMMENT_2}
        w;
        {BODY_COMMENT_3}
    end
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
