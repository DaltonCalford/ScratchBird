#coding:utf-8

"""
ID:          issue-708
ISSUE:       708
TITLE:       Complex view crashes server
DESCRIPTION:
  Name of original test has no any relation with actual task of this test: ###
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_26.script

  Issue in original script: bug #583690 Complex view crashes server
  Found in FB tracker as: http://tracker.firebirdsql.org/browse/CORE-366
  Fixed on 2.0 Beta 1

  We expect that compilation of this test script finished OK, without any errors/warnings.
  2.5 issues "too many contexts / max allowed 255'; because of this, min_version=3.0
JIRA:        CORE-366
FBTEST:      bugs.core_0366
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    CREATE TABLE DRZAVA(
    	POZIVNIBROJDRZAVE VARCHAR(4) NOT NULL,
    	NAZIVDRZAVE VARCHAR(20),
    	GRUPA INTEGER NOT NULL,
    	PRIMARY KEY(POZIVNIBROJDRZAVE)
    );

    CREATE TABLE LOG (
    	BROJ VARCHAR(25) NOT NULL,
    	POCETAK TIMESTAMP NOT NULL,
    	TRAJANJE INTEGER NOT NULL,
    	LOKAL INTEGER,
    	LINIJA INTEGER,
    	CENA NUMERIC(8,2) NOT NULL
    );

    CREATE TABLE LOKAL(
    	BROJLOKALA INTEGER NOT NULL,
    	NAZIVLOKALA VARCHAR(25) NOT NULL,
    	PRIMARY KEY(BROJLOKALA)
    );

    CREATE TABLE MESNI(
    	PTT CHAR(5) NOT NULL,
    	LOKALNIPREFIX VARCHAR(5) NOT NULL,
    	PRIMARY KEY (PTT,LOKALNIPREFIX)
    );

    CREATE TABLE MREZA(
    	BROJMREZE VARCHAR(4) NOT NULL,
    	POZIVNIBROJ VARCHAR(4) NOT NULL,
    	ZONA INTEGER NOT NULL,
    	PRIMARY KEY (BROJMREZE,POZIVNIBROJ)
    );

    CREATE TABLE VrstaRT(
    	SifraVRT char(7) NOT NULL,
    	NazivVRT varchar(30) NOT NULL,
    	JM varchar(6),
    	PRIMARY KEY (SifraVRT)
    );

    CREATE TABLE Poslovnica(
    	SifraPoslovnice char(2) NOT NULL,
    	NazivPoslovnice varchar(18) NOT NULL,
    	PRIMARY KEY(SifraPoslovnice)
    );

    CREATE TABLE RezijskiTrosak(
    	RedniBroj integer NOT NULL,
    	DatumTroska timestamp NOT NULL,
    	SifraPoslovnice char(2) NOT NULL
    		REFERENCES Poslovnica (SifraPoslovnice) ON UPDATE CASCADE,
    	SifraVRT char(7) NOT NULL
    		REFERENCES VrstaRT(SifraVRT) ON UPDATE CASCADE,
    	Kolicina decimal(8,2),
    	Iznos decimal(8,2) NOT NULL,
    	PRIMARY KEY (RedniBroj)
    );

    CREATE GENERATOR GEN_RT_ID;
    SET GENERATOR GEN_RT_ID TO 0;

    CREATE TABLE VrstaMT(
    	SifraVMT char(7) NOT NULL,
    	NazivVMT varchar(30) NOT NULL,
    	DefaultJM varchar(6),
    	PRIMARY KEY(SifraVMT)
    );

    CREATE TABLE Roba(
    	SifraRobe char(6) NOT NULL,
    	VrstaRobe char(7) NOT NULL
    		REFERENCES VrstaMT (SifraVMT) ON UPDATE CASCADE,
    	NazivRobe varchar(30) NOT NULL,
    	JM varchar(6) NOT NULL,
    	BarCode varchar(50),
    	Pakovanje integer,
    	Napomena varchar(100),
    	PRIMARY KEY(SifraRobe)
    );

    CREATE TABLE Mesto(
    	PTT char(5) NOT NULL,
    	NazivMesta varchar(40) NOT NULL,
    	PozivniBroj char(4),
    	PRIMARY KEY(PTT)
    );

    CREATE TABLE Komitent(
    	SifraKomitenta integer NOT NULL,
    	Naziv varchar(25) NOT NULL ,
    	PTT char(5) NOT NULL
    		REFERENCES Mesto(PTT) ON UPDATE CASCADE,
    	Napomena varchar(100),
    	Owner char(8),
    	PRIMARY KEY(SifraKomitenta)
    );

    CREATE GENERATOR GEN_Komitent_ID;
    SET GENERATOR GEN_Komitent_ID TO 0;

    CREATE TABLE VrstaDetalja(
    	SifraVD integer NOT NULL,
    	OpisVD varchar(15),
    	Telefon char(1),
    	CHECK (telefon is null or telefon = 'D' or telefon ='Z'),
    	PRIMARY KEY(SifraVD)
    );

    CREATE GENERATOR GEN_VrstaDetalja_ID;
    SET GENERATOR GEN_VrstaDetalja_ID TO 0;

    CREATE TABLE KomitentDetaljno (
    	SifraKD integer NOT NULL,
    	SifraKomitenta integer NOT NULL
    		REFERENCES Komitent (SifraKomitenta) ON UPDATE CASCADE ON DELETE CASCADE,
    	SifraVD integer NOT NULL
    		REFERENCES VrstaDetalja (SifraVD) ON UPDATE CASCADE,
    	Podatak varchar(40) NOT NULL,
    	CistBroj varchar(25),
    	PRIMARY KEY(SifraKD)
    );

    CREATE GENERATOR GEN_KOMITENTDETALJNO_ID;
    SET GENERATOR GEN_KOMITENTDETALJNO_ID TO 0;

    CREATE TABLE Prijem(
    	BRDOK integer NOT NULL,
    	DatumUlaza timestamp NOT NULL,
    	SifraKomitenta integer
    		REFERENCES Komitent(SifraKomitenta) ON UPDATE CASCADE,
    	PRIMARY KEY(BRDOK)
    );

    CREATE GENERATOR GEN_PRIJ_ID;
    SET GENERATOR GEN_PRIJ_ID TO 0;

    CREATE TABLE Prijemst(
    	BRDOK integer NOT NULL
    		REFERENCES Prijem(BRDOK) ON UPDATE CASCADE ON DELETE CASCADE,
    	SifraRobe char(6) NOT NULL
    		REFERENCES ROBA(SifraRobe) ON UPDATE CASCADE,
    	Kolicina decimal(8,2) NOT NULL,
    	Cena decimal(8,2) NOT NULL,
    	PRIMARY KEY (BRDOK,SifraRobe)
    );

    CREATE TABLE Alokacija(
    	Brdok integer NOT NULL,
    	Datum timestamp NOT NULL,
    	SifraPoslovnice char(2) NOT NULL
    		REFERENCES Poslovnica (SifraPoslovnice) ON UPDATE CASCADE,
    	PRIMARY KEY (Brdok)
    );

    CREATE GENERATOR GEN_ALOK_ID;
    SET GENERATOR GEN_ALOK_ID TO 1;

    CREATE TABLE Alokacijast(
    	Brdok integer NOT NULL
    		REFERENCES Alokacija(BRDOK) ON UPDATE CASCADE ON DELETE CASCADE,
    	SifraRobe char(6) NOT NULL
    		REFERENCES ROBA(SifraRobe) ON UPDATE CASCADE,
    	Kolicina decimal(8,2) NOT NULL,
    	Cena decimal(8,2) NOT NULL,
    	PRIMARY KEY (Brdok,SifraRobe)
    );

    CREATE TABLE VrstaGoriva(
    	SifraVrsteGoriva Integer NOT NULL,
    	NazivVrsteGoriva varchar(10) NOT NULL,
    	PRIMARY KEY(SifraVrsteGoriva)
    );


    CREATE TABLE VrstaVozila(
    	SifraVrste char(2) NOT NULL,
    	NazivVrste varchar(18) NOT NULL,
    	PRIMARY KEY(SifraVrste)
    );

    CREATE TABLE Vozilo(
    	SifraVozila char(12) NOT NULL,
    	SifraVrste char(2) NOT NULL
    		REFERENCES VrstaVozila (SifraVrste) ON UPDATE CASCADE,
    	RegBroj char(10),
    	Marka char(10),
    	Tip char(20),
    	BrojSasije char(25),
    	BrojMotora char(25),
    	PrvaRegistracija timestamp,
    	SnagaMotora decimal(10,2),
    	Zapremina integer,
    	Nosivost integer,
    	MestaZaSedenje char(4),
    	Karoserija char(25),
    	Boja char(20),
    	BrojOsovina char(1),
    	RokPPAparata timestamp,
    	PRIMARY KEY(SifraVozila)
    );

    CREATE TABLE Vozac(
    	SifraVozaca integer NOT NULL,
    	Ime char(25) NOT NULL,
    	Kategorije char(5) NOT NULL,
    	DatumVazenjaDozvole Timestamp,
    	PRIMARY KEY(SifraVozaca)
    );

    CREATE TABLE SipanjeGoriva(
    	SifraSG integer NOT NULL,
    	Datum Timestamp NOT NULL,
    	SifraVozila char(12) NOT NULL
    		REFERENCES Vozilo(SifraVozila) ON UPDATE CASCADE,
    	SifraVozaca integer NOT NULL
    		REFERENCES Vozac(SifraVozaca) ON UPDATE CASCADE,
    	SifraVrsteGoriva integer NOT NULL
    		REFERENCES VrstaGoriva (SifraVrsteGoriva) ON UPDATE CASCADE,
    	SifraPoslovnice char(2) NOT NULL
    		REFERENCES Poslovnica (SifraPoslovnice) ON UPDATE CASCADE,
    	KMsat decimal(9,1),
    	Kolicina decimal(10, 2) NOT NULL,
    	Cena decimal(8,2) NOT NULL,
    	PunDoCepa char(1),
    	CHECK (PunDoCepa = 'N' or PunDoCepa = 'D'),
    	PRIMARY KEY (SifraSG)
    );

    CREATE GENERATOR GEN_GORIVO_ID;
    SET GENERATOR GEN_GORIVO_ID TO 1;

    CREATE TABLE Popravka(
    	Datum Timestamp NOT NULL,
    	SifraVozila char(12) NOT NULL
    		REFERENCES Vozilo(SifraVozila) ON UPDATE CASCADE,
    	SifraVozaca integer NOT NULL
    		REFERENCES Vozac(SifraVozaca) ON UPDATE CASCADE,
    	SifraPoslovnice char(2) NOT NULL
    		REFERENCES Poslovnica (SifraPoslovnice) ON UPDATE CASCADE,
    	Iznos decimal(12,2) NOT NULL,
    	Opis varchar(200),
    	PRIMARY KEY(Datum,SifraVozila)
    );

    CREATE TABLE Registracija(
    	Datum Timestamp NOT NULL,
    	SifraVozila char(12) NOT NULL
    		REFERENCES Vozilo(SifraVozila) ON UPDATE CASCADE,
    	CenaTehnickog decimal(12,2),
    	CenaOsiguranja decimal(12,2),
    	OstaliTroskovi decimal(12,2),
    	SifraPoslovnice char(2) NOT NULL
    		REFERENCES Poslovnica (SifraPoslovnice) ON UPDATE CASCADE,
    	PRIMARY KEY(Datum,SifraVozila)
    );

    CREATE TABLE DUMMY(
    	foobar integer NOT NULL primary key,
    	check (foobar = 1)
    );

    INSERT INTO dummy VALUES(1);



    CREATE VIEW APROMET(DATUM, SO, VRSTA,IZNOS) AS
    	select
    	rt.datumtroska,
    	SIFRAPOSLOVNICE,
    	cast(vrt.nazivvrt as varchar(30)),
    	cast(rt.iznos as numeric(18, 2))
    	from rezijskitrosak rt
    	left join VRSTART vrt on rt.sifravrt = vrt.sifravrt

    	union all

    	SELECT
    	AL.DATUM,
    	SIFRAPOSLOVNICE,
    	cast('KancMat'as varchar(30)),
    	cast(sum(alst.kolicina * alst.cena) as numeric(18, 2))
    	FROM ALOKACIJAST ALST
    	LEFT JOIN ALOKACIJA AL ON ALST.brdok=AL.brdok
    	LEFT JOIN ROBA R ON ALST.sifrarobe = R.sifrarobe
    	WHERE R.vrstarobe = 'KM'
    	GROUP BY AL.DATUM, SIFRAPOSLOVNICE

    	union all

    	SELECT
    	AL.DATUM,
    	SIFRAPOSLOVNICE,
    	cast ('Hemikalije' as varchar(30)),
    	cast(sum(alst.kolicina * alst.cena) as numeric(18, 2))
    	FROM ALOKACIJAST ALST
    	LEFT JOIN ALOKACIJA AL ON ALST.brdok=AL.brdok
    	LEFT JOIN ROBA R ON ALST.sifrarobe = R.sifrarobe
    	WHERE R.vrstarobe = 'HE'
    	GROUP BY AL.DATUM, SIFRAPOSLOVNICE

    	union all

    	SELECT
    	AL.DATUM,
    	SIFRAPOSLOVNICE,
    	cast('Prehrana' as varchar(30)),
    	cast(sum(alst.kolicina * alst.cena) as numeric(18, 2))
    	FROM ALOKACIJAST ALST
    	LEFT JOIN ALOKACIJA AL ON ALST.brdok=AL.brdok
    	LEFT JOIN ROBA R ON ALST.sifrarobe = R.sifrarobe
    	WHERE R.vrstarobe = 'HR'
    	GROUP BY AL.DATUM, SIFRAPOSLOVNICE

    	union all

    	SELECT
    	pp.datum,
    	SIFRAPOSLOVNICE,
    	cast('Popravke' as varchar(30)),
    	cast(sum(iznos) as numeric(18,2))
    	FROM popravka pp
    	GROUP BY pp.DATUM, SIFRAPOSLOVNICE

    	union all

    	SELECT
    	rg.datum,
    	SIFRAPOSLOVNICE,
    	cast('Registracije' as varchar(30)),
    	cast(sum(cenatehnickog + cenaosiguranja+ostalitroskovi) as numeric(18,2))
    	FROM registracija rg
    	GROUP BY rg.DATUM, SIFRAPOSLOVNICE

    	union all

    	SELECT
    	sg.datum,
    	SIFRAPOSLOVNICE,
    	cast('Gorivo' as varchar(30)),
    	cast(sum(kolicina * cena) as numeric(18,2))
    	FROM sipanjegoriva sg
    	GROUP BY
    	sg.DATUM, SIFRAPOSLOVNICE
    	;


    CREATE VIEW VV(VRSTA) AS
    	select distinct vrsta from apromet a;
    commit;


    SELECT vv.VRSTA,
    (select sum(ap.iznos)
    from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=1),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=2),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=3),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=4),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=5),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=6),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=7),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=8),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=9),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=10),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=11),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta =
    vv.vrsta
    and
    extract(month from ap.datum)=12),
    (select
    sum(ap.iznos) from apromet ap where ap.vrsta
    =
    vv.vrsta)
    FROM vv;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
