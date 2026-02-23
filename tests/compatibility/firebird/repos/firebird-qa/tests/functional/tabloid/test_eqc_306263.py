#coding:utf-8

"""
ID:          tabloid.eqc-306263
TITLE:       Check ability to run complex query
DESCRIPTION: 
FBTEST:      functional.tabloid.eqc_306263
NOTES:
[26.05.2022] pzotov
  Re-implemented for work in firebird-qa suite. 
  Checked on: 3.0.8.33535, 4.0.1.2692, 5.0.0.497
"""

import pytest
import zipfile
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

act = python_act('db', substitutions=[('0.000000000000000.*', '0.0000000000000000')])

expected_stdout = """
    OBSCH_SCHL                      1.000000000000000
    SCHLD_BZG                       <null>
    OBJ_ID                          3759
    VERTRAG                         <null>
    DLKO_ANNUI                      0.0000000000000000
    ZUKO_WRT                        <null>
    DLKO_UNOM                       0.0000000000000000
    DARLG_BZG                       <null>
    OBSCH_GB                        2100-01-01 00:00:00.0000
    OBSCH_GABD                      2005-08-01 00:00:00.0000
    FLGK_KZ                         4
    FAELLIGKEIT                     <null>
    TOP_ID                          <null>
    DLKO_GVOND                      2011-01-01 00:00:00.0000
    DLKO_GBISD                      2100-01-01 00:00:00.0000
    ZUKO_ID                         <null>
    ZUKO_GVOND                      <null>
    ZUKO_GBID                       <null>
    OBJ_ID                          3759
"""

fbk_file = temp_file('tmp_eqc_306263.fbk')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path):

    zipped_fbk_file = zipfile.Path(act.files_dir / 'eqc306263.zip', at='eqc306263.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    with act.connect_server() as srv:
        srv.database.restore(database=act.db.db_path, backup=fbk_file, flags=SrvRestoreFlag.REPLACE)
        srv.wait()

    script = """
        set list on; 
        select
            objeschlue.obsch_schl,
            schluedef.schld_bzg,
            objeschlue.obj_id,
            darlehen.vertrag,
            darlkond.dlko_annui,
            zuschkond.zuko_wrt,
            darlkond.dlko_unom,
            darlgeber.darlg_bzg,
            objeschlue.obsch_gb,
            objeschlue.obsch_gabd,
            darlkond.flgk_kz,
            zuschkond.faelligkeit,
            darl_obper.top_id,
            darlkond.dlko_gvond,
            darlkond.dlko_gbisd,
            zuschkond.zuko_id,
            zuschkond.zuko_gvond,
            zuschkond.zuko_gbid,
            darl_obper.obj_id
        from
        (
            (
                (
                    (
                        (
                            (
                                darl_obper darl_obper
                                inner join darlehen darlehen on darl_obper.darl_id=darlehen.darl_id
                            )
                            inner join objeschlue objeschlue on darlehen.darl_id=objeschlue.darl_id
                        )
                        inner join darlgeber darlgeber on darlehen.darlg_id=darlgeber.darlg_id
                    )
                    inner join darlkond darlkond on darlehen.darl_id=darlkond.darl_id
                )
                left outer join schluedef schluedef on objeschlue.schld_id=schluedef.schld_id
            )
            left outer join zuschkond zuschkond on darlehen.darl_id=zuschkond.darl_id
        )
        where
            darl_obper.obj_id=3759
            and darlkond.dlko_gvond<'12/02/2011 00:00:00'
            and darlkond.dlko_gbisd>='12/01/2011 00:00:00'
            and objeschlue.obj_id=3759
            and objeschlue.obsch_gb>='12/02/2011 00:00:00'
            and objeschlue.obsch_gabd<'12/02/2011 00:00:00'
            and darl_obper.top_id is  null
            and (
                    zuschkond.zuko_id is  null
                    or zuschkond.zuko_gvond<'12/02/2011 00:00:00' and zuschkond.zuko_gbid>='12/01/2011 00:00:00'
                );
        commit;
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=[], input = script, combine_output=True)

    assert act.clean_stdout == act.clean_expected_stdout
