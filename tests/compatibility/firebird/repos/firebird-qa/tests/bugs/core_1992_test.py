#coding:utf-8

"""
ID:          issue-2429
ISSUE:       2429
TITLE:       bad BLR -- invalid stream for union select
DESCRIPTION:
JIRA:        CORE-1992
FBTEST:      bugs.core_1992
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create or alter procedure sp01(
        a_01 smallint,
        a_02 int,
        a_03 bigint,
        a_04 numeric(9,0),
        a_05 numeric(10,0),
        a_06 float,
        a_07 double precision,
        --a_08 date,
        --a_09 time,
        a_10 timestamp,
        a_11 varchar(1),
        a_12 char(2),
        a_13 blob
    ) returns (
        o_01 smallint,
        o_02 int,
        o_03 bigint,
        o_04 numeric(9,0),
        o_05 numeric(10,0),
        o_06 float,
        o_07 double precision,
        --o_08 date,
        --o_09 time,
        o_10 timestamp,
        o_11 varchar(1),
        o_12 char(2),
        o_13 blob
    ) as
    begin
        suspend;
    end
    ^
    set term ;^
    commit;

    ------------------------------------

    with
    extent1 as
    (
        select
            trim(f.rdb$function_name) as Id
            , null as CatalogName
            , null as SchemaName
            , trim(f.rdb$function_name) as Name
            , trim(
            case fa.rdb$field_type
                when 7 then
                    case fa.rdb$field_sub_type
                        when 0 then 'smallint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 8 then
                    case fa.rdb$field_sub_type
                        when 0 then 'int'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 16 then
                    case fa.rdb$field_sub_type
                        when 0 then 'bigint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 10 then 'float'
                when 27 then 'double'
                when 12 then 'date'
                when 13 then 'time'
                when 35 then 'timestamp'
                when 261 then 'blob'
                when 37 then 'varchar'
                when 14 then 'char'
                when 40 then 'cstring'
            end
            ) as ReturnTypeName
            , fa.rdb$character_length as ReturnMaxLength
            , fa.rdb$field_precision as ReturnPrecision
            , 0 as ReturnDateTimePrecision
            , fa.rdb$field_scale * (-1) as ReturnScale
            , null as ReturnCollationCatalog
            , null as ReturnCollationSchema
            , null as ReturnCollationName
            , null as ReturnCharacterSetCatalog
            , null as ReturnCharacterSetSchema
            , null as ReturnCharacterSetName
            , CAST(0 as smallint) as ReturnIsMultiSet
            , CAST(0 as smallint) as IsAggregate
            , CAST(0 as smallint) as IsBuiltIn
            , CAST((select CASE COUNT(*) WHEN 1 THEN 1 ELSE 0 END FROM rdb$function_arguments fa WHERE fa.rdb$function_name = f.rdb$function_name ) as smallint) as IsNiladic
        from rdb$functions f
        inner join rdb$function_arguments fa on (f.rdb$function_name = fa.rdb$function_name and f.rdb$return_argument = fa.rdb$argument_position)
        where f.rdb$system_flag = 0 -- ::: nb ::: need add alias f. in 3.0!
    )
    ,extent2 as (
        select
        null as id
        , null as parentid
        , null as name
        , null as ordinal
        , null as typename
        , null as maxlength
        , null as dblprecision
        , null as datetimeprecision
        , null as scale
        , null as collationcatalog
        , null as collationschema
        , null as collationname
        , null as charactersetcatalog
        , null as charactersetschema
        , null as charactersetname
        , null as ismultiset
        , null as mode
        , null as defaulttype
        from rdb$database
        where 0=1
    ),
    extent3 as (
        select
          trim(pp.rdb$procedure_name) || 'x' || trim(pp.rdb$parameter_name) as id
        , trim(pp.rdb$procedure_name) as parentid
        , trim(pp.rdb$parameter_name) as name
        , pp.rdb$parameter_number+1 as ordinal
        , trim(
                case f.rdb$field_type
                when 7 then
                    case f.rdb$field_sub_type
                        when 0 then 'smallint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 8 then
                    case f.rdb$field_sub_type
                        when 0 then 'int'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 16 then
                    case f.rdb$field_sub_type
                        when 0 then 'bigint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 10 then 'float'
                when 27 then 'double'
                when 12 then 'date'
                when 13 then 'time'
                when 35 then 'timestamp'
                when 261 then 'blob'
                when 37 then 'varchar'
                when 14 then 'char'
                when 40 then 'cstring'
                end
            ) as typename
        , f.rdb$character_length as MaxLength
        , f.rdb$field_precision as DblPrecision
        , 0 as DateTimePrecision
        , f.rdb$field_scale * (-1) as Scale
        , null as CollationCatalog
        , null as CollationSchema
        , null CollationName
        , null as CharacterSetCatalog
        , null as CharacterSetSchema
        , null as CharacterSetName
        , cast(0 as smallint) as IsMultiSet
        , trim(iif(pp.rdb$parameter_type = 1, 'OUT', 'IN')) as Mode
        , null as DefaultType
        from rdb$procedure_parameters pp
        join rdb$fields f on (pp.rdb$field_source = f.rdb$field_name)
    )
    ,extent4 as (
        select
            trim(rdb$procedure_name) as id
            , null as catalogname
            , '' as schemaname -- bug or not??? need to be not null
            , trim(rdb$procedure_name) as name
        from
        rdb$procedures
    )
    ,extent5 as (
        select
            null as Id
            , null as ParentId
            , null as Name
            , null as Ordinal
            , null as TypeName
            , null as MaxLength
            , null as DblPrecision
            , null as DateTimePrecision
            , null as Scale
            , null as CollationCatalog
            , null as CollationSchema
            , null as CollationName
            , null as CharacterSetCatalog
            , null as CharacterSetSchema
            , null as CharacterSetName
            , null as IsMultiSet
            , null as Mode
            , null as DefaultType
        from rdb$database where 0=1
    )
    ,extent6 as (
        select
          trim(pp.rdb$procedure_name) || 'x' || trim(pp.rdb$parameter_name) as id
        , trim(pp.rdb$procedure_name) as parentid
        , trim(pp.rdb$parameter_name) as name
        , pp.rdb$parameter_number+1 as ordinal
        , trim(
            case f.rdb$field_type
                when 7 then
                    case f.rdb$field_sub_type
                        when 0 then 'smallint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 8 then
                    case f.rdb$field_sub_type
                        when 0 then 'int'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 16 then
                    case f.rdb$field_sub_type
                        when 0 then 'bigint'
                        when 1 then 'numeric'
                        when 2 then 'decimal'
                    end
                when 10 then 'float'
                when 27 then 'double'
                when 12 then 'date'
                when 13 then 'time'
                when 35 then 'timestamp'
                when 261 then 'blob'
                when 37 then 'varchar'
                when 14 then 'char'
                when 40 then 'cstring'
            end
        ) as TypeName
        , f.rdb$character_length as MaxLength
        , f.rdb$field_precision as DblPrecision
        , 0 as DateTimePrecision
        , f.rdb$field_scale * (-1) as Scale
        , null as CollationCatalog
        , null as CollationSchema
        , null CollationName
        , null as CharacterSetCatalog
        , null as CharacterSetSchema
        , null as CharacterSetName
        , cast(0 as smallint) as IsMultiSet
        , trim(iif(pp.rdb$parameter_type = 1, 'OUT', 'IN')) as Mode
        , null as DefaultType
        from rdb$procedure_parameters pp
        join rdb$fields f on (pp.rdb$field_source = f.rdb$field_name)
    )

    ,UnionAll1 as ( -- e2 union all e3
        select
            e2.name as name,
            e2.ordinal as ordinal,
            e2.typename as typename,
            e2.mode as mode,
            0 as c1,
            e2.parentid as parentid
        from extent2 as e2

        UNION ALL

        select
            e3.name as name,
            e3.ordinal as ordinal,
            e3.typename as typename,
            e3.mode as mode,
            6 as c1,
            e3.parentid as parentId
        from extent3 as e3
    )

    ,unionAll2 as ( -- e5 union all e6

        select
            e5.name as name,
            e5.ordinal as ordinal,
            e5.typename as typename,
            e5.mode as mode,
            0 as c1,
            e5.parentid as parentid
        from extent5 as e5

        UNION ALL

        select
            e6.name as name,
            e6.ordinal as ordinal,
            e6.typename as typename,
            e6.mode as mode,
            6 as c1,
            e6.parentid as parentid
        from extent6 as e6
    )
    ,unionall3 as (  -- e1 left join u1 UNION ALL e4 left join u2
        select
            e1.schemaname as schemaname,
            e1.name as name,
            e1.returntypename as returntypename,
            e1.isaggregate as isaggregate,
            cast(1 as smallint) as c1,
            e1.isbuiltin as isbuiltin,
            e1.isniladic as isniladic,
            u1.name as c2,
            u1.typename as c3,
            u1.mode as c4,
            u1.ordinal as c5
        from extent1 e1
        left join unionAll1 u1 on (0 = u1.C1) AND (e1.Id = u1.ParentId)

        UNION ALL

        select
            e4.schemaname as schemaname,
            e4.name as name,
            cast(null as varchar(1000)) as c1,
            cast(0 as smallint) as c2,
            cast(0 as smallint) as c3,
            cast(0 as smallint) as c4,
            cast(0 as smallint) as c5,
            u2.name as c6,
            u2.typename as c7,
            u2.mode as c8,
            u2.ordinal as c9
        from extent4 as e4
        left outer join unionall2 as u2 on (6 = u2.c1) and (e4.id = u2.parentid)
    ) -- end of unionAll3

    ,project7 as (
        select
            u3.schemaname as c1,
            u3.name as c2,
            u3.returntypename as c3,
            u3.isaggregate as c4,
            u3.c1 as c5,
            u3.isbuiltin as c6,
            u3.isniladic as c7,
            u3.c2 as c8,
            u3.c3 as c9,
            u3.c4 as c10,
            u3.c5 as c11,
            1 as c12
        from unionAll3 as u3
    )
    ,c_final as (
        select
            p7.c8 as par_name,
            p7.c9 as par_type,
            p7.c10 as par_dir
        from project7 as p7
        order by par_dir,par_name
    )
    select sign(count(*)) as result from c_final
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RESULT                          1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

