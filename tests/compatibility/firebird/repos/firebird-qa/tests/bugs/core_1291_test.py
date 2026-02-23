#coding:utf-8

"""
ID:          issue-1712
ISSUE:       1712
TITLE:       Can't transliterate character set when look at procedure text in database just created from script
DESCRIPTION:
JIRA:        CORE-1291
FBTEST:      bugs.core_1291
NOTES:
    [24.06.2025] pzotov
    Added substitutions in orde to suppress output of procedure header info. This data irrelevant to the test.
    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

test_script = """
SET SQL DIALECT 3;
SET NAMES WIN1251;
CREATE DATABASE '%s' DEFAULT CHARACTER SET WIN1251;

CREATE DOMAIN DMN_BOOL AS
SMALLINT
DEFAULT 0;

CREATE DOMAIN DMN_PRICE AS
DOUBLE PRECISION
DEFAULT 0
CHECK (Value >= 0);

CREATE DOMAIN DMN_DOCNAME AS
VARCHAR(20) CHARACTER SET WIN1251;

CREATE TABLE TBL_CONST_FLOAT (
    NAME     VARCHAR(20) CHARACTER SET WIN1251 NOT NULL,
    VAL      DOUBLE PRECISION,
    COMMENT  VARCHAR(128) CHARACTER SET WIN1251
);

CREATE TABLE TBL_EXPORTENTRY (
    ID_EXPORTGROUPENTRY  INTEGER NOT NULL,
    ID_EXPORTFIELDDOC    INTEGER NOT NULL,
    ACCOUNTCREDIT        INTEGER DEFAULT 0 NOT NULL,
    ACCOUNTDEBIT         INTEGER DEFAULT 0 NOT NULL
);

CREATE TABLE TBL_INDOC_TEMP (
    ID_INDOC_TEMP          INTEGER NOT NULL,
    ID_POINT               INTEGER NOT NULL,
    ID_ITEM                INTEGER NOT NULL,
    ID_INDOC               INTEGER NOT NULL,
    SERNO                  VARCHAR(20) CHARACTER SET WIN1251,
    POINTMUL               DOUBLE PRECISION NOT NULL,
    VOLUME                 DMN_PRICE NOT NULL /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICEDOC               DMN_PRICE /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICERUB               DMN_PRICE /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICECUR               DMN_PRICE /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICECALCDOC           DMN_PRICE NOT NULL /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICECALCRUB           DMN_PRICE NOT NULL /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    PRICECALCCUR           DMN_PRICE NOT NULL /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    NDS                    DMN_PRICE /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    F_CANSALE              DMN_BOOL NOT NULL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    PASPORT                VARCHAR(64) CHARACTER SET WIN1251,
    PASPORTCREATEDATE      TIMESTAMP,
    PASPORTEXPIREDATE      TIMESTAMP,
    REGISTRATION           VARCHAR(64) CHARACTER SET WIN1251,
    PROTOCOL               VARCHAR(64) CHARACTER SET WIN1251,
    SERTIFICAT             VARCHAR(128) CHARACTER SET WIN1251,
    ID_CERTCENTER          INTEGER DEFAULT 0,
    ITEMCREATEDATE         TIMESTAMP,
    ITEMEXPIREDATE         TIMESTAMP,
    ID_PARENT_OUTDOC_TEMP  INTEGER DEFAULT 0,
    SUMMDOC                COMPUTED BY (Volume*PriceDoc),
    SUMMRUB                COMPUTED BY (Volume*PriceRub),
    SUMMCUR                COMPUTED BY (Volume*PriceCur),
    SUMMCALCDOC            COMPUTED BY (Volume*PriceCalcDoc),
    SUMMCALCRUB            COMPUTED BY (Volume*PriceCalcRub),
    SUMMCALCCUR            COMPUTED BY (Volume*PriceCalcCur),
    SUMMNDSDOC             COMPUTED BY (NDS*SummCalcDoc/100),
    SUMMNDSRUB             COMPUTED BY (NDS*SummCalcRub/100),
    SUMMNDSCUR             COMPUTED BY (NDS*SummCalcCur/100),
    SUMMWITHNDSDOC         COMPUTED BY (SummCalcDoc+SummNDSDoc),
    SUMMWITHNDSRUB         COMPUTED BY (SummCalcRub+SummNDSRub),
    SUMMWITHNDSCUR         COMPUTED BY (SummCalcCur+SummNDSCur),
    PRICEDOC_IN            DOUBLE PRECISION DEFAULT 0,
    EXPIREPERCENT          COMPUTED BY (Cast(100 * (ItemExpireDate - CURRENT_TIMESTAMP) / (ItemExpireDate - ItemCreateDate) as Integer))
);

CREATE TABLE TBL_OUTDOC_TEMP (
    ID_OUTDOC_TEMP  INTEGER NOT NULL,
    ID_OUTDOC       INTEGER NOT NULL,
    ID_ITEMSALE     INTEGER,
    ID_ITEM         INTEGER,
    ID_POINT        INTEGER NOT NULL,
    VOLUME          DOUBLE PRECISION DEFAULT 0 NOT NULL,
    PRICEDOC        DOUBLE PRECISION DEFAULT 0 NOT NULL,
    NDS             DOUBLE PRECISION DEFAULT 0 NOT NULL,
    BUILDERPERCENT  DOUBLE PRECISION DEFAULT 0 NOT NULL,
    SUMMDOC         DOUBLE PRECISION DEFAULT 0,
    SUMMNDSDOC      DOUBLE PRECISION DEFAULT 0,
    SUMMWITHNDSDOC  DOUBLE PRECISION DEFAULT 0,
    BUILDERPRICE    COMPUTED BY (PriceDoc*100/(100+BuilderPercent))
);

CREATE TABLE TBL_ITEMSALE (
    ID_ITEMSALE         INTEGER NOT NULL,
    ID_STORE            INTEGER NOT NULL,
    ID_ITEM             INTEGER NOT NULL,
    ID_FIRSTINDOC_TEMP  INTEGER NOT NULL,
    F_ALOWSALE          DMN_BOOL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    VOLUME              DMN_PRICE NOT NULL /* DMN_PRICE = DOUBLE PRECISION DEFAULT 0 CHECK (Value >= 0) */,
    SALEPRICERUB        DOUBLE PRECISION DEFAULT 0 NOT NULL,
    SALEPRICECUR        DOUBLE PRECISION DEFAULT 0 NOT NULL,
    F_IMPORTED          DMN_BOOL NOT NULL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    V_VOLUME            DOUBLE PRECISION DEFAULT 0,
    V_MASS              DOUBLE PRECISION DEFAULT 0
);

CREATE TABLE TBL_INDOC (
    ID_INDOC              INTEGER NOT NULL,
    ID_STORE              INTEGER NOT NULL,
    ID_FIRM               INTEGER NOT NULL,
    ID_SELFFIRM           INTEGER NOT NULL,
    ID_CUR                INTEGER NOT NULL,
    DOCTYPE               INTEGER NOT NULL,
    DOCNAME               DMN_DOCNAME /* DMN_DOCNAME = VARCHAR(20) */,
    DATECREATE            TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    DATEDOC               TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    DATELASTEDIT          TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    DATESTORE             TIMESTAMP,
    ID_USERCREATE         INTEGER NOT NULL,
    ID_USERLOCK           INTEGER,
    F_FINISHED            DMN_BOOL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    F_STORE               DMN_BOOL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    F_SPECIAL             DMN_BOOL /* DMN_BOOL = SMALLINT DEFAULT 0 */,
    ADDENDUM              VARCHAR(128) CHARACTER SET WIN1251,
    COMMENT               VARCHAR(128) CHARACTER SET WIN1251,
    COURSE                DOUBLE PRECISION NOT NULL,
    ID_PARENT_OUTDOC      INTEGER DEFAULT 0 NOT NULL,
    PRICEDOC_IN_IMPORTED  SMALLINT DEFAULT 0 NOT NULL
);

SET TERM ^ ;

CREATE PROCEDURE PRC_EXPSUMM_8_10 (
    ID_EXPORTFIELDDOC INTEGER,
    ID_OUTDOC INTEGER,
    NDSDIV DOUBLE PRECISION)
RETURNS (
    SUMMRUS DOUBLE PRECISION,
    SUMMDOC DOUBLE PRECISION)
AS
begin
           IF (id_exportfielddoc=130) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=131) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=132) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=133) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=134) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceCalcRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceCalcDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
            IF (id_exportfielddoc=135) THEN BEGIN
             SELECT SUM(A.SummNDSDoc), SUM(A.SummNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=136) THEN BEGIN
             SELECT SUM((A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=137) THEN BEGIN
             SELECT SUM((A.PriceCalcRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceCalcDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=138) THEN BEGIN
             SELECT SUM(A.SummWithNDSDoc), SUM(A.SummWithNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=139) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=140) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=141) THEN BEGIN
             SELECT SUM(A.NDS*(A.PriceCalcRub-A.PriceRub)*B.Volume/(A.PointMul*100)), SUM(A.NDS*(A.PriceCalcDoc-A.PriceDoc)*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=142) THEN BEGIN
             SELECT SUM(B.SummNDSDoc-A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(B.SummNDSDoc-A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=143) THEN BEGIN
             SELECT SUM(((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=144) THEN BEGIN
             SELECT SUM(B.SummWithNDSDoc-(A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(B.SummWithNDSDoc-(A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds<:NDSDiv
             INTO :summrus, :summdoc;
           END
     SUSPEND;
END
^

CREATE PROCEDURE PRC_EXPSUMM_8_20 (
    ID_EXPORTFIELDDOC INTEGER,
    ID_OUTDOC INTEGER,
    NDSDIV DOUBLE PRECISION)
RETURNS (
    SUMMRUS DOUBLE PRECISION,
    SUMMDOC DOUBLE PRECISION)
AS
begin
           IF (id_exportfielddoc=145) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=146) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=147) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=148) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=149) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceCalcRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceCalcDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
            IF (id_exportfielddoc=150) THEN BEGIN
             SELECT SUM(A.SummNDSDoc), SUM(A.SummNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=151) THEN BEGIN
             SELECT SUM((A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=152) THEN BEGIN
             SELECT SUM((A.PriceCalcRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceCalcDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=153) THEN BEGIN
             SELECT SUM(A.SummWithNDSDoc), SUM(A.SummWithNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds>0 and a.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=154) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=155) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=156) THEN BEGIN
             SELECT SUM(A.NDS*(A.PriceCalcRub-A.PriceRub)*B.Volume/(A.PointMul*100)), SUM(A.NDS*(A.PriceCalcDoc-A.PriceDoc)*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=157) THEN BEGIN
             SELECT SUM(B.SummNDSDoc-A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(B.SummNDSDoc-A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=158) THEN BEGIN
             SELECT SUM(((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=159) THEN BEGIN
             SELECT SUM(B.SummWithNDSDoc-(A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(B.SummWithNDSDoc-(A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds>0 and b.nds>=:NDSDiv
             INTO :summrus, :summdoc;
           END
     SUSPEND;
END
^

CREATE PROCEDURE PRC_EXPSUMM_8(
    ID_INDOC INTEGER,
    ID_OUTDOC INTEGER,
    ID_GROUP INTEGER)
RETURNS (
    ACCOUNTCREDIT INTEGER,
    ACCOUNTDEBIT INTEGER,
    SUMMRUS DOUBLE PRECISION,
    SUMMDOC DOUBLE PRECISION)
AS
DECLARE VARIABLE NDSDiv DOUBLE PRECISION;
DECLARE VARIABLE ID_ExportFieldDoc Integer;
begin
  SELECT Val FROM TBL_Const_Float
  WHERE Name='NDSDiv'
  INTO :NDSDiv;
  FOR SELECT id_exportfielddoc, accountcredit, accountdebit FROM tbl_exportentry
      WHERE id_exportgroupentry=:id_group
      INTO :id_exportfielddoc, :accountcredit, :accountdebit
      DO BEGIN
         IF ((AccountCredit<>0) or (AccountDebit<>0)) THEN BEGIN
           IF (id_exportfielddoc=110) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=111) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=112) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=113) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=114) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceCalcRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceCalcDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
            IF (id_exportfielddoc=115) THEN BEGIN
             SELECT SUM(A.SummNDSDoc), SUM(A.SummNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=116) THEN BEGIN
             SELECT SUM((A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=117) THEN BEGIN
             SELECT SUM((A.PriceCalcRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceCalcDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=118) THEN BEGIN
             SELECT SUM(A.SummWithNDSDoc), SUM(A.SummWithNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=119) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=120) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=121) THEN BEGIN
             SELECT SUM(A.NDS*(A.PriceCalcRub-A.PriceRub)*B.Volume/(A.PointMul*100)), SUM(A.NDS*(A.PriceCalcDoc-A.PriceDoc)*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=122) THEN BEGIN
             SELECT SUM(B.SummNDSDoc-A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(B.SummNDSDoc-A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=123) THEN BEGIN
             SELECT SUM(((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=124) THEN BEGIN
             SELECT SUM(B.SummWithNDSDoc-(A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(B.SummWithNDSDoc-(A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END

           IF (id_exportfielddoc=125) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=126) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=127) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=128) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=129) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF ((id_exportfielddoc>129)and(id_exportfielddoc<145)) THEN BEGIN
                execute procedure  prc_expsumm_8_10 ( id_exportfielddoc, id_outdoc, NDSDiv ) returning_values( summrus, summdoc );
           END
           IF ((id_exportfielddoc>144)and(id_exportfielddoc<160)) THEN BEGIN
                execute procedure  prc_expsumm_8_20 ( id_exportfielddoc, id_outdoc, NDSDiv ) returning_values( summrus, summdoc );
           END
           IF ((SummRus>0) or (SummDoc>0)) THEN SUSPEND;
         END
      END
END
^

SET TERM ; ^

COMMIT WORK;

SHOW PROCEDURE PRC_EXPSUMM_8;

EXIT;
"""

db = db_factory(do_not_create=True)

substitutions = [ ('Procedure: \\S+.*', ''), ('Procedure text:', '') ]
act = python_act('db', substitutions = substitutions)

expected_stdout = """
=============================================================================
DECLARE VARIABLE NDSDiv DOUBLE PRECISION;
DECLARE VARIABLE ID_ExportFieldDoc Integer;
begin
  SELECT Val FROM TBL_Const_Float
  WHERE Name='NDSDiv'
  INTO :NDSDiv;
  FOR SELECT id_exportfielddoc, accountcredit, accountdebit FROM tbl_exportentry
      WHERE id_exportgroupentry=:id_group
      INTO :id_exportfielddoc, :accountcredit, :accountdebit
      DO BEGIN
         IF ((AccountCredit<>0) or (AccountDebit<>0)) THEN BEGIN
           IF (id_exportfielddoc=110) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=111) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=112) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=113) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=114) THEN BEGIN
             SELECT SUM(A.NDS*A.PriceCalcRub*B.Volume/(A.PointMul*100)), SUM(A.NDS*A.PriceCalcDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
            IF (id_exportfielddoc=115) THEN BEGIN
             SELECT SUM(A.SummNDSDoc), SUM(A.SummNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=116) THEN BEGIN
             SELECT SUM((A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=117) THEN BEGIN
             SELECT SUM((A.PriceCalcRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM((A.PriceCalcDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=118) THEN BEGIN
             SELECT SUM(A.SummWithNDSDoc), SUM(A.SummWithNDSDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=119) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=120) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=121) THEN BEGIN
             SELECT SUM(A.NDS*(A.PriceCalcRub-A.PriceRub)*B.Volume/(A.PointMul*100)), SUM(A.NDS*(A.PriceCalcDoc-A.PriceDoc)*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=122) THEN BEGIN
             SELECT SUM(B.SummNDSDoc-A.NDS*A.PriceRub*B.Volume/(A.PointMul*100)), SUM(B.SummNDSDoc-A.NDS*A.PriceDoc*B.Volume/(A.PointMul*100))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=123) THEN BEGIN
             SELECT SUM(((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=124) THEN BEGIN
             SELECT SUM(B.SummWithNDSDoc-(A.PriceRub*B.Volume/A.PointMul)*(1+A.NDS/100)), SUM(B.SummWithNDSDoc-(A.PriceDoc*B.Volume/A.PointMul)*(1+A.NDS/100)) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6
             INTO :summrus, :summdoc;
           END

           IF (id_exportfielddoc=125) THEN BEGIN
             SELECT SUM(A.PriceRub*B.Volume/A.PointMul), SUM(A.PriceDoc*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=126) THEN BEGIN
             SELECT SUM(A.PriceCalcRub*B.Volume/A.PointMul), SUM(A.PriceCalcDoc*B.Volume/A.PointMul) FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=127) THEN BEGIN
             SELECT SUM(A.SummDoc), SUM(A.SummDoc) FROM TBL_OUTDOC_TEMP A,
                                                                                                                            TBL_ITEMSALE B,
                                                                                                                            TBL_INDOC_TEMP C,
                                                                                                                            TBL_INDOC D
             WHERE A.ID_OUTDOC=:id_outdoc and b.id_itemsale=a.id_itemsale and c.id_indoc_temp=b.id_firstindoc_temp and
                               d.id_indoc=c.id_indoc and d.doctype = 6 and a.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=128) THEN BEGIN
             SELECT SUM((A.PriceCalcRub-A.PriceRub)*B.Volume/A.PointMul), SUM((A.PriceCalcDoc-A.PriceDoc)*B.Volume/A.PointMul)  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF (id_exportfielddoc=129) THEN BEGIN
             SELECT SUM(B.SummDoc-(A.PriceRub*B.Volume/A.PointMul)), SUM(B.SummDoc-(A.PriceDoc*B.Volume/A.PointMul))  FROM TBL_INDOC_TEMP A,
                                                                                                                                              TBL_OUTDOC_TEMP B,
                                                                                                                                              TBL_ITEMSALE C,
                                                                                                                                              TBL_INDOC D
             WHERE b.id_outdoc=:id_outdoc and b.id_itemsale=c.id_itemsale and c.id_firstindoc_temp=a.id_indoc_temp and
                               d.id_indoc=a.id_indoc and d.doctype = 6 and b.nds=0
             INTO :summrus, :summdoc;
           END
           IF ((id_exportfielddoc>129)and(id_exportfielddoc<145)) THEN BEGIN
                execute procedure  prc_expsumm_8_10 ( id_exportfielddoc, id_outdoc, NDSDiv ) returning_values( summrus, summdoc );
           END
           IF ((id_exportfielddoc>144)and(id_exportfielddoc<160)) THEN BEGIN
                execute procedure  prc_expsumm_8_20 ( id_exportfielddoc, id_outdoc, NDSDiv ) returning_values( summrus, summdoc );
           END
           IF ((SummRus>0) or (SummDoc>0)) THEN SUSPEND;
         END
      END
END
=============================================================================
Parameters:
ID_INDOC                          INPUT INTEGER
ID_OUTDOC                         INPUT INTEGER
ID_GROUP                          INPUT INTEGER
ACCOUNTCREDIT                     OUTPUT INTEGER
ACCOUNTDEBIT                      OUTPUT INTEGER
SUMMRUS                           OUTPUT DOUBLE PRECISION
SUMMDOC                           OUTPUT DOUBLE PRECISION
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script % act.db.dsn, connect_db=False, charset='WIN1251', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout


