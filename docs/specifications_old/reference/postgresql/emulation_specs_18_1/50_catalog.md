PostgreSQL 18.1 internal catalog (system catalog tables)

Source of truth
- CATALOG definitions: src/include/catalog/pg_*.h

Catalog tables

Catalog: pg_aggregate

Catalog: pg_am
- oid: Oid
- amname: NameData
- amtype: char

Catalog: pg_amop
- oid: Oid
- amopstrategy: int16

Catalog: pg_amproc
- oid: Oid
- amprocnum: int16

Catalog: pg_attrdef
- oid: Oid
- adnum: int16

Catalog: pg_cast
- oid: Oid
- castcontext: char
- castmethod: char

Catalog: pg_collation
- oid: Oid
- collname: NameData
- collprovider: char
- collencoding: int32

Catalog: pg_constraint
- oid: Oid
- conname: NameData
- contype: char
- condeferrable: bool
- condeferred: bool
- conenforced: bool
- convalidated: bool
- confupdtype: char
- confdeltype: char
- confmatchtype: char
- conislocal: bool
- coninhcount: int16
- connoinherit: bool
- conperiod: bool
- conbin: pg_node_tree

Catalog: pg_conversion
- oid: Oid
- conname: NameData

Catalog: pg_default_acl
- oid: Oid
- defaclobjtype: char

Catalog: pg_depend
- objid: Oid
- objsubid: int32
- refobjid: Oid
- refobjsubid: int32
- deptype: char

Catalog: pg_description
- objoid: Oid
- classoid: Oid
- objsubid: int32

Catalog: pg_enum
- oid: Oid
- enumsortorder: float4
- enumlabel: NameData

Catalog: pg_event_trigger
- oid: Oid
- evtname: NameData
- evtevent: NameData
- evtenabled: char

Catalog: pg_extension
- oid: Oid
- extname: NameData
- extrelocatable: bool

Catalog: pg_foreign_data_wrapper
- oid: Oid
- fdwname: NameData

Catalog: pg_foreign_server
- oid: Oid
- srvname: NameData
- srvtype: text
- srvversion: text

Catalog: pg_foreign_table

Catalog: pg_inherits
- inhseqno: int32
- inhdetachpending: bool

Catalog: pg_init_privs
- objoid: Oid
- objsubid: int32
- privtype: char

Catalog: pg_language
- oid: Oid
- lanname: NameData

Catalog: pg_largeobject
- pageno: int32

Catalog: pg_largeobject_metadata
- oid: Oid

Catalog: pg_namespace
- oid: Oid
- nspname: NameData

Catalog: pg_opclass
- oid: Oid
- opcname: NameData

Catalog: pg_operator
- oid: Oid
- oprname: NameData

Catalog: pg_opfamily
- oid: Oid
- opfname: NameData

Catalog: pg_partitioned_table
- partstrat: char
- partnatts: int16
- partexprs: pg_node_tree

Catalog: pg_policy
- oid: Oid
- polname: NameData
- polcmd: char
- polpermissive: bool
- polqual: pg_node_tree
- polwithcheck: pg_node_tree

Catalog: pg_publication
- oid: Oid
- pubname: NameData
- puballtables: bool
- pubinsert: bool
- pubupdate: bool
- pubdelete: bool
- pubtruncate: bool
- pubviaroot: bool
- pubgencols: char

Catalog: pg_publication_namespace
- oid: Oid

Catalog: pg_publication_rel
- oid: Oid
- prqual: pg_node_tree
- prattrs: int2vector

Catalog: pg_range

Catalog: pg_rewrite
- oid: Oid
- rulename: NameData
- ev_type: char
- ev_enabled: char
- is_instead: bool

Catalog: pg_seclabel
- objoid: Oid
- objsubid: int32

Catalog: pg_sequence
- seqstart: int64
- seqincrement: int64
- seqmax: int64
- seqmin: int64
- seqcache: int64
- seqcycle: bool

Catalog: pg_statistic
- staattnum: int16
- stainherit: bool
- stanullfrac: float4
- stawidth: int32
- stadistinct: float4
- stakind1: int16
- stakind2: int16
- stakind3: int16
- stakind4: int16
- stakind5: int16
- stavalues1: anyarray
- stavalues2: anyarray
- stavalues3: anyarray
- stavalues4: anyarray
- stavalues5: anyarray

Catalog: pg_statistic_ext
- oid: Oid
- stxname: NameData
- stxexprs: pg_node_tree

Catalog: pg_statistic_ext_data
- stxdinherit: bool
- stxdndistinct: pg_ndistinct
- stxddependencies: pg_dependencies
- stxdmcv: pg_mcv_list

Catalog: pg_subscription_rel
- srsubstate: char

Catalog: pg_transform
- oid: Oid

Catalog: pg_trigger
- oid: Oid
- tgname: NameData
- tgtype: int16
- tgenabled: char
- tgisinternal: bool
- tgdeferrable: bool
- tginitdeferred: bool
- tgnargs: int16
- tgqual: pg_node_tree
- tgoldtable: NameData
- tgnewtable: NameData

Catalog: pg_ts_config
- oid: Oid
- cfgname: NameData

Catalog: pg_ts_config_map
- maptokentype: int32
- mapseqno: int32

Catalog: pg_ts_dict
- oid: Oid
- dictname: NameData
- dictinitoption: text

Catalog: pg_ts_parser
- oid: Oid
- prsname: NameData

Catalog: pg_ts_template
- oid: Oid
- tmplname: NameData

Catalog: pg_user_mapping
- oid: Oid
