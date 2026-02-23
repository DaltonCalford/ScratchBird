#coding:utf-8

"""
ID:          issue-5912
ISSUE:       5912
TITLE:       Parse error when compiling a statement causes memory leak until attachment is disconnected
DESCRIPTION:
    Test uses DDL and query for well known Einstein task.
    SQL query for solution of this task is quite complex and must consume some memory resources.
    This query intentionally is made non-compilable, so actually it will not ever run.
    We query mon$memory_usage before running this query and after it, with storing values mon$memory* fields in the dictionary.
    We do <N_MEASURES> runs and after this evaluate differences of comsumed memory for each run.
    Build 4.0.0.483 (05-jan-2017) shows that:
        * mon$memory_used increased for ~227 Kb each run;
        * mon$memory_allocated increased for ~960Kb.
    Build 4.0.0.840 (02-jan-2018) shows that mon$memory_used increased for 2Kb - but only at FIRST measure.
    Since 2nd measure memory consumption did not increase (neither mon$memory_used nor mon$memory_allocated).
    But mon$max_memory_used can slightly increase, only ONCE, at 3rd run, for about 1Kb.

    Test evaluates MEDIANS for differences of mon$memory* fields.
    All these medians must be ZERO otherwise test is considered as failed.

JIRA:        CORE-5646
FBTEST:      bugs.core_5646
NOTES:
    [27.11.2023] pzotov
        commit to FB 4.x: 26-nov-2017
            https://github.com/FirebirdSQL/firebird/commit/5e1b5e172e95388f8f2f236b89295bb915aef397

        commit to FB 3.x: 06-oct-2021
            https://github.com/FirebirdSQL/firebird/commit/ed585ab09fdad63551c48d1ce392c810b5cef4a8
            (since build 3.0.8.33519, date: 07-oct-2021)

        4.0.0.483, 05-jan-2017
        ======================
        absolute values:
        1  ::: (2329392, 2818048, 2569880, 2818048)
        2  ::: (2565264, 4456448, 3380832, 4653056)
        3  ::: (2565264, 4456448, 3380832, 4653056)
        4  ::: (2798240, 5373952, 3614280, 5636096)
        5  ::: (2798240, 5373952, 3614280, 5636096)
        6  ::: (3031216, 6356992, 3846784, 6619136)
        7  ::: (3031216, 6356992, 3846784, 6619136)
        8  ::: (3264192, 7340032, 4079760, 7602176)
        9  ::: (3264192, 7340032, 4079760, 7602176)
        10 ::: (3497400, 8323072, 4312968, 8585216)
        11 ::: (3497400, 8323072, 4312968, 8585216)
        12 ::: (3730376, 9240576, 4545944, 9502720)
        13 ::: (3730376, 9240576, 4545944, 9502720)
        14 ::: (3963352, 10223616, 4778920, 10485760)
        15 ::: (3963352, 10223616, 4778920, 10485760)
        16 ::: (4196328, 11141120, 5011896, 11403264)
        17 ::: (4196328, 11141120, 5011896, 11403264)
        18 ::: (4429304, 12124160, 5244872, 12386304)
        19 ::: (4429304, 12124160, 5244872, 12386304)
        20 ::: (4662280, 13041664, 5477848, 13303808)
        21 ::: (4662280, 13041664, 5477848, 13303808)
        22 ::: (4895256, 14024704, 5710824, 14286848)

        differences:
         0 ::: [235872, 1638400, 810952, 1835008]
         1 ::: [232976,  917504, 233448,  983040]
         2 ::: [232976,  983040, 232504,  983040]
         3 ::: [232976,  983040, 232976,  983040]
         4 ::: [233208,  983040, 233208,  983040]
         5 ::: [232976,  917504, 232976,  917504]
         6 ::: [232976,  983040, 232976,  983040]
         7 ::: [232976,  917504, 232976,  917504]
         8 ::: [232976,  983040, 232976,  983040]
         9 ::: [232976,  917504, 232976,  917504]
        10 ::: [232976,  983040, 232976,  983040]

    4.0.0.840, 02-jan-2018
    ======================
        differences:
         0 ::: [2896, 65536, 876800, 1966080]
         1 ::: [   0,     0,      0,       0]
         2 ::: [   0,     0,      0,       0]
         3 ::: [   0,     0,      0,       0]
         4 ::: [   0,     0,      0,       0]
         5 ::: [   0,     0,      0,       0]
         6 ::: [   0,     0,      0,       0]
         7 ::: [   0,     0,      0,       0]
         8 ::: [   0,     0,      0,       0]
         9 ::: [   0,     0,      0,       0]
        10 ::: [   0,     0,      0,       0]

    [18.01.2025] pzotov
        Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
        in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
        Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
        This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
        The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################

# How many times we call procedures:
N_MEASURES = 41

init_sql = """
    set bail on;
    recreate sequence g;

    recreate table animal (animal varchar(10) primary key using index pk_animal);
    recreate table color (color varchar(10) primary key using index pk_color);
    recreate table drink (drink varchar(10) primary key using index pk_drink);
    recreate table nation (nation varchar(10) primary key using index pk_nation);
    recreate table smoke (smoke varchar(10) primary key using index pk_smoke);

    insert into animal (animal) values ('cat');
    insert into animal (animal) values ('fish');
    insert into animal (animal) values ('dog');
    insert into animal (animal) values ('horse');
    insert into animal (animal) values ('bird');

    insert into color (color) values ('white');
    insert into color (color) values ('yellow');
    insert into color (color) values ('red');
    insert into color (color) values ('blue');
    insert into color (color) values ('green');

    insert into drink (drink) values ('cofee');
    insert into drink (drink) values ('milk');
    insert into drink (drink) values ('water');
    insert into drink (drink) values ('beer');
    insert into drink (drink) values ('tee');

    insert into nation (nation) values ('eng');
    insert into nation (nation) values ('swe');
    insert into nation (nation) values ('deu');
    insert into nation (nation) values ('den');
    insert into nation (nation) values ('nor');

    insert into smoke (smoke) values ('pall mall');
    insert into smoke (smoke) values ('marlboro');
    insert into smoke (smoke) values ('rothmans');
    insert into smoke (smoke) values ('winfield');
    insert into smoke (smoke) values ('dunhill');
    commit;
"""
db = db_factory(init = init_sql)

act = python_act('db')


@pytest.mark.version('>=3.0.8')
def test_1(act: Action, capsys):

    bad_sql = """
        select count(*)
        from (
           select
               a1.animal as h1animal,
               d1.drink as h1drink,
               n1.nation as h1nation,
               s1.smoke as h1smoke,
               c1.color as h1color,

               a2.animal as h2animal,
               d2.drink as h2drink,
               n2.nation as h2nation,
               s2.smoke as h2smoke,
               c2.color as h2color,

               a3.animal as h3animal,
               d3.drink as h3drink,
               n3.nation as h3nation,
               s3.smoke as h3smoke,
               c3.color as h3color,

               a4.animal as h4animal,
               d4.drink as h4drink,
               n4.nation as h4nation,
               s4.smoke as h4smoke,
               c4.color as h4color,

               a5.animal as h5animal,
               d5.drink as h5drink,
               n5.nation as h5nation,
               s5.smoke as h5smoke,
               c5.color as h5color
           from
               animal a1,
               drink d1,
               nation n1,
               smoke s1,
               color c1,

               animal a2,
               drink d2,
               nation n2,
               smoke s2,
               color c2,

               animal a3,
               drink d3,
               nation n3,
               smoke s3,
               color c3,

               animal a4,
               drink d4,
               nation n4,
               smoke s4,
               color c4,

               animal a5,
               drink d5,
               nation n5,
               smoke s5,
               color c5
           where
                   a1.animal <> a2.animal and
                   a1.animal <> a3.animal and
                   a1.animal <> a4.animal and
                   a1.animal <> a5.animal and
                   a2.animal <> a3.animal and
                   a2.animal <> a4.animal and
                   a2.animal <> a5.animal and
                   a3.animal <> a4.animal and
                   a3.animal <> a5.animal and
                   a4.animal <> a5.animal --and
               and
                   c1.color <> c2.color and
                   c1.color <> c3.color and
                   c1.color <> c4.color and
                   c1.color <> c5.color and
                   c2.color <> c3.color and
                   c2.color <> c4.color and
                   c2.color <> c5.color and
                   c3.color <> c4.color and
                   c3.color <> c5.color and
                   c4.color <> c5.color
               and
                   d1.drink <> d2.drink and
                   d1.drink <> d3.drink and
                   d1.drink <> d4.drink and
                   d1.drink <> d5.drink and
                   d2.drink <> d3.drink and
                   d2.drink <> d4.drink and
                   d2.drink <> d5.drink and
                   d3.drink <> d4.drink and
                   d3.drink <> d5.drink and
                   d4.drink <> d5.drink
               and
                   n1.nation <> n2.nation and
                   n1.nation <> n3.nation and
                   n1.nation <> n4.nation and
                   n1.nation <> n5.nation and
                   n2.nation <> n3.nation and
                   n2.nation <> n4.nation and
                   n2.nation <> n5.nation and
                   n3.nation <> n4.nation and
                   n3.nation <> n5.nation and
                   n4.nation <> n5.nation
               and
                   s1.smoke <> s2.smoke and
                   s1.smoke <> s3.smoke and
                   s1.smoke <> s4.smoke and
                   s1.smoke <> s5.smoke and
                   s2.smoke <> s3.smoke and
                   s2.smoke <> s4.smoke and
                   s2.smoke <> s5.smoke and
                   s3.smoke <> s4.smoke and
                   s3.smoke <> s5.smoke and
                   s4.smoke <> s5.smoke
               and
               -- 1
               (
                   (n1.nation = 'eng' and c1.color = 'red') or
                   (n2.nation = 'eng' and c2.color = 'red') or
                   (n3.nation = 'eng' and c3.color = 'red') or
                   (n4.nation = 'eng' and c4.color = 'red') or
                   (n5.nation = 'eng' and c5.color = 'red')
               )
               and
               -- 2
               (
                   (n1.nation = 'swe' and a1.animal = 'dog') or
                   (n2.nation = 'swe' and a2.animal = 'dog') or
                   (n3.nation = 'swe' and a3.animal = 'dog') or
                   (n4.nation = 'swe' and a4.animal = 'dog') or
                   (n5.nation = 'swe' and a5.animal = 'dog')
               )
               and
               -- 3
               (
                   (n1.nation = 'den' and d1.drink = 'tee') or
                   (n2.nation = 'den' and d2.drink = 'tee') or
                   (n3.nation = 'den' and d3.drink = 'tee') or
                   (n4.nation = 'den' and d4.drink = 'tee') or
                   (n5.nation = 'den' and d5.drink = 'tee')
               )
               and
               -- 4
               (
                   (c1.color = 'green' and c2.color = 'white') or
                   (c2.color = 'green' and c3.color = 'white') or
                   (c3.color = 'green' and c4.color = 'white') or
                   (c4.color = 'green' and c5.color = 'white')
               )
               and
               -- 5
               (
                   (c1.color = 'green' and d1.drink = 'coffee') or
                   (c2.color = 'green' and d2.drink = 'coffee') or
                   (c3.color = 'green' and d3.drink = 'coffee') or
                   (c4.color = 'green' and d4.drink = 'coffee') or
                   (c5.color = 'green' and d5.drink = 'coffee')
               )
               and
               -- 6
               (
                   (s1.smoke = 'pall mall' and a1.animal = 'bird') or
                   (s2.smoke = 'pall mall' and a2.animal = 'bird') or
                   (s3.smoke = 'pall mall' and a3.animal = 'bird') or
                   (s4.smoke = 'pall mall' and a4.animal = 'bird') or
                   (s5.smoke = 'pall mall' and a5.animal = 'bird')
               )
               and
               -- 7
               (d3.drink = 'milk')
               and
               -- 8
               (
                   (s1.smoke = 'dunhill' and c1.color = 'yellow') or
                   (s2.smoke = 'dunhill' and c2.color = 'yellow') or
                   (s3.smoke = 'dunhill' and c3.color = 'yellow') or
                   (s4.smoke = 'dunhill' and c4.color = 'yellow') or
                   (s5.smoke = 'dunhill' and c5.color = 'yellow')
               )
               and
               -- 9
               (n1.nation = 'nor')
               and
               -- 10
               (
                   (s1.smoke = 'marlboro' and a2.animal = 'cat') or
                   (s2.smoke = 'marlboro' and 'cat' in (a1.animal, a3.animal)) or
                   (s3.smoke = 'marlboro' and 'cat' in (a2.animal, a4.animal)) or
                   (s4.smoke = 'marlboro' and 'cat' in (a3.animal, a5.animal)) or
                   (s5.smoke = 'marlboro' and a4.animal = 'cat')
               )
               and
               -- 11
               (
                   (s1.smoke = 'dunhill' and a2.animal = 'horse') or
                   (s2.smoke = 'dunhill' and 'cat' in (a1.animal, a3.animal)) or
                   (s3.smoke = 'dunhill' and 'cat' in (a2.animal, a4.animal)) or
                   (s4.smoke = 'dunhill' and 'cat' in (a3.animal, a5.animal)) or
                   (s5.smoke = 'dunhill' and a4.animal = 'horse')
               )
               and
               -- 12
               (
                   (s1.smoke = 'winfield' and d1.drink = 'beer') or
                   (s2.smoke = 'winfield' and d2.drink = 'beer') or
                   (s3.smoke = 'winfield' and d3.drink = 'beer') or
                   (s4.smoke = 'winfield' and d4.drink = 'beer') or
                   (s5.smoke = 'winfield' and d5.drink = 'beer')
               )
               and
               -- 13
               (
                   (n1.nation = 'nor' and c2.color = 'blue') or
                   (n2.nation = 'nor' and 'blue' in (c1.color, c3.color)) or
                   (n3.nation = 'nor' and 'blue' in (c2.color, c4.color)) or
                   (n4.nation = 'nor' and 'blue' in (c3.color, c5.color)) or
                   (n5.nation = 'nor' and c4.color = 'blue')
               )
               and
               -- 14
               (
                   (s1.smoke = 'rothmans' and n1.nation = 'deu') or
                   (s2.smoke = 'rothmans' and n2.nation = 'deu') or
                   (s3.smoke = 'rothmans' and n3.nation = 'deu') or
                   (s4.smoke = 'rothmans' and n4.nation = 'deu') or
                   (s5.smoke = 'rothmans' and n5.nation = 'deu')
               )
               and
               -- 15
               (
                   (s1.smoke = 'marlboro' and d2.drink = 'water') or
                   (s2.smoke = 'marlboro' and 'water' in (d1.drink, d3.drink)) or
                   (s3.smoke = 'marlboro' and 'water' in (d2.drink, d4.drink)) or
                   (s4.smoke = 'marlboro' and 'water' in (d3.drink, d5.drink)) or
                   (s5.smoke = 'marlboro' and d4.drink = 'water')
               )
               and %(i)s and
        )
    """

    memory_diff = {}
    with act.db.connect() as con_worker, act.db.connect() as con_dba:

        ################################################################
        ###   Q U E R Y    T O    M O N $ M E M O R Y _ U S A G E    ###
        ################################################################
        sql_memory_usage = f"""
            select
                 gen_id(g,1) - 1 as seq_no
                ,m.mon$memory_used as cur_used
                ,m.mon$memory_allocated as cur_alloc
                ,m.mon$max_memory_used as max_used
                ,m.mon$max_memory_allocated as max_alloc
            from mon$attachments a
            join mon$memory_usage m on a.mon$stat_id = m.mon$stat_id
            where a.mon$attachment_id = {con_worker.info.id}
        """

        with con_worker.cursor() as cur_worker, con_dba.cursor() as cur_dba:
            ps, rs = None, None
            try:
                ps = cur_dba.prepare(sql_memory_usage)
                for i in range(N_MEASURES):

                    # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                    # We have to store result of cur.execute(<psInstance>) in order to
                    # close it explicitly.
                    # Otherwise AV can occur during Python garbage collection and this
                    # causes pytest to hang on its final point.
                    # Explained by hvlad, email 26.10.24 17:42
                    rs = cur_dba.execute(ps)
                    for r in rs:
                        memory_diff[ i ] = r[1:]
                     
                    try:
                        # nb: EVERY run we force engine to compile NEW query because of changing 'i':
                        px = cur_worker.prepare(bad_sql % locals())
                    except DatabaseError as x:
                        pass

                    con_dba.commit()
                    rs = cur_dba.execute(ps)
                    for r in rs:
                        # Make subtraction for elements with same indices.
                        # This is DIFFERENCE between values in mon$memory_usage columns
                        # gathered after and before each measure:
                        #
                        memory_diff[ i ] = [a - b for a, b in zip(r[1:], memory_diff[ i ])]

            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)
            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

    memo_used_diff_list = [ v[0] for v in memory_diff.values() ]
    memo_allo_diff_list = [ v[1] for v in memory_diff.values() ]
    mmax_used_diff_list = [ v[2] for v in memory_diff.values() ]
    mmax_allo_diff_list = [ v[3] for v in memory_diff.values() ]

    memo_used_diff_median = median(memo_used_diff_list)
    memo_allo_diff_median = median(memo_allo_diff_list)
    mmax_used_diff_median = median(mmax_used_diff_list)
    mmax_allo_diff_median = median(mmax_allo_diff_list)

    expected_stdout = 'All differences in mon$memory_usage have zero medians.'
    if memo_used_diff_median + memo_allo_diff_median + mmax_used_diff_median + mmax_allo_diff_median > 0:
        print('Found at least one NON ZERO median of memory increment values:')
        print('memo_used_diff_median=', '{:9d}'.format(memo_used_diff_median))
        print('memo_allo_diff_median=', '{:9d}'.format(memo_allo_diff_median))
        print('mmax_used_diff_median=', '{:9d}'.format(mmax_used_diff_median))
        print('mmax_allo_diff_median=', '{:9d}'.format(mmax_allo_diff_median))
        print('memory_diff.items():')
        for k,v in memory_diff.items():
            print('measure # %4d ' % k,' diff:', '{:9d}'.format(v[0]), '{:9d}'.format(v[0]), '{:9d}'.format(v[0]), '{:9d}'.format(v[0])  )
    else:
        print(expected_stdout)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
