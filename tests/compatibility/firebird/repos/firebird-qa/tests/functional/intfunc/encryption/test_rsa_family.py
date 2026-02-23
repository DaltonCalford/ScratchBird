#coding:utf-8

"""
ID:          intfunc.encryption.rsa-family
TITLE:       Basic test for RSA-family: rsa_private(), rsa_public(), rsa_encrypt(),
  rsa_decrypt(), rsa_sign_hash() and rsa_verify_hash()
DESCRIPTION:
  We create table with one record and write in it UTF8 text with enough length.
  Then we get random substring from this text and use it as "source message" that will be encrypted further.
  After this test generates private and public keys and uses them for two tasks:
  1) get RSA signature of crypted_hash of source message (using private key)
     and verify it (using public key); RSA_VERIFY_HASH must return <true>;
  2) encrypt source message, then decrypt it and compare result of decryption with original text. They must be equal.

  All these actions are applied against four algorithms: MD5, SHA1, SHA256 and SHA512.

  NB: code for FB 5.x is separated from FB 4.x because of renamed functions rsa_sign_hash() and rsa_verify_hash(),
  see: https://github.com/FirebirdSQL/firebird/issues/6806

  ::: NOTE :::
  It was encountered maximal number of octets in the source text must NOT exceed 126.
  Otherwise usage of SHA512 will fail with:
    Statement failed, SQLSTATE = 22023
    TomCrypt library error: Invalid sized parameter.
    -Encrypting using cipher RSA
  Because of this, we must limit length of generated UTF8 "source text" is limited by 63 - see variable 'lorem' in EB.
  (greek and cyrillic characters are used here in the source text; they both require 2 bytes per character).
NOTES:
[22.05.2021]
  correction because of renamed functions rsa_sign and rsa_verify: suffix "_hash" was added to their names after ~14.05.2021
[03.02.2022] pcisar
  Removed version for 5.0, because both tests_script and expected_output were identical.
  Renamed functions rsa_sign_hash() and rsa_verify_hash() are part of v4.0
FBTEST:      functional.intfunc.encryption.rsa_family
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table rsa(
        text_unencrypted varchar(256)
       ,k_prv varbinary(16384)
       ,k_pub varbinary(8192)
       ,text_rsa_sign varchar(8192)
       ,text_rsa_vrfy boolean
       ,text_encrypted varchar(256)
       ,text_decrypted varchar(256)
    );
    insert into rsa default values;

    set term ^;
    execute block returns(
        rsa_sign_len_mg5 smallint
       ,rsa_vrfy_md5 boolean
       ,rsa_sign_len_sha1 smallint
       ,rsa_vrfy_sha1 boolean
       ,rsa_sign_len_sha256 smallint
       ,rsa_vrfy_sha256 boolean
       ,rsa_sign_len_sha512 smallint
       ,rsa_vrfy_sha512 boolean
       ,encr_octet_len_md5 smallint
       ,decryption_result_md5 varchar(3)
       ,encr_octet_len_sha1 smallint
       ,decryption_result_sha1 varchar(3)
       ,encr_octet_len_sha256 smallint
       ,decryption_result_sha256 varchar(3)
       ,encr_octet_len_sha512 smallint
       ,decryption_result_sha512 varchar(3)
    )
    as
        declare lorem varchar(8190) character set utf8;
        declare p_beg smallint;
        declare n_for smallint;
    begin
        lorem =    'ΛορεμιπσθμδολορσιταμετvισιδνοvθμαλτερθμcομπλεcτιτθρτεναμηομεροβλανδιτμελανqθοδμοδοδολορεμΑτσεδγραεcιδελεcτθσcθειθσπροπριαεμολεστιαεθσθανηισqθοδσιποστθλαντπερτιναcιαΛαθδεμεqθιδεματηισΔθοπορροτολλιτπλατονεμαναλιιπαθλοcονστιτθαμqθοανΠρορεπθδιαρεcονσεqθθντθρεξνεcεθθνθμσολεαττεvελσθασcασελεγιμθσΝεεvερτιτθραππελλαντθρvισθτναμνοστερμολλισvολθπταριαΑγαμδολορεφφιcιαντθραννεcΑφφερτσιγνιφερθμqθεπριεισθασλθδθσδεσερθισσειδπερΠροβοπονδερθμετvιξετσιτστετφαcερρεφερρεντθρΕιαεqθετολλιτπροπριαεμεαvιξεθσθασδισπθτανδοΛαβορεφαcετεvολθπτατθμαδcθμηαβεοvιρισδολορετμελΑγαμλιβερβλανδιτηασαδvιμcθαεqθειντερεσσετVισεξvενιαμομνεσqθεινvενιρεCασεcονσετετθρvισατεvερτιφορενσιβθσσθσcιπιαντθρεξηισΕτπερτολλιτεριπθιτσαπιεντεμvελνεqθαεqθεποστθλανταεqθεηονεστατισεοσανΣονετεριπθιτεθvισλεγερεαδολεσcενσετναμCιβοαβηορρεαντινμεαναμηαβεοvιταειναδηισμαλισριδενσcορρθμπιτΝεcαδπριμισμενανδριμαγναqθαεστιοεξπλιcαριεθθσθνεqθοcομμοδοαετερνοαργθμεντθμΣθμοvιτθπερατοριβθσεστθτΜειτεvενιαμσεμπερΠαθλοαλβθcιθσvισατCθεαμφερριδοcτθσοφφενδιτεστατνατθμμθτατΕαμελτριτανιελαβοραρετλθδθσσcριπτορεμπερετΣεαειανιμαλcοντεντιονεσομιτταμλθcιλιθσπερνεΑπεριριπερσιθσαλτερθμθσθνονεcαπειριανοπορτερενεΑδμειμεισφαcερπθταντvιμcασεμοvεττραcτατοσεξΑππετερεμανδαμθσνοηιστιμεαμqθαεqθεδελενιτετεστvιρτθτετεμποριβθσδθοτεΜοδοινιμιcθσεισεαεοσιναφφερττεμπορcομμοδοΕνιμqθοτειρμοδcθπεραδολεσcενσπηιλοσοπηιαvιξεαΘτιναμνοστρθδvιστεπροειφαλλιλατινερεφορμιδανσΕθμαθδιαμεξπετενδισλιβεραvισσεεαεξοcθρρερετπροδεσσετσιτΝαμεξcοντεντιονεσδετερρθισσετvιστεδθισαθδιρεΕξμαλορθμcονσεqθατμνεσαρcηθμσεδΑδπροομνεσqθεcονστιτθαμλιβεραvισσεεξνθλλαμδοcτθσινδοcτθμεαμΕιηασμολλισομιτταμνεcδιcθντλθcιλιθσανταντασποσσιμιθvαρετεοσετΕιεραντμαιεστατισνεcεθμμοδθσαλιqθανδοεαVιμεθισμοδτορqθατοσδεσερθισσεειεαπροπονδερθμπερφεcτοQθοvιρισcαθσαετιβιqθετεσεδδιcοεσσελαορεετνοΑσσθμλθδθσβλανδιτεοσεξΦαcιλισοφφενδιτεαμεξατσιτπορροφαcιλισΗασπερφεcτοσεντεντιαεαccομμοδαρενοΗαβεοαλβθcιθσcονcλθσιονεμqθεατεοσμελαττατιονcονσετετθρjθστοαδιπισcιτεεοσΔιcαμρατιονιβθσσιγνιφερθμqθεεθμνομεισολεταδμοδθμνοφαcερταcιματεσεξπετενδισηισαδΗισcθπρομπτατορqθατοσvιξαδμελιορεπερcιπιτΕqθιδεμταcιματεσεθριπιδισιδπερμελατιλλθμεqθιδεμαccθμσανΕιαπεριαμινcορρθπτενεcΕιδθοδελιcαταρεφορμιδανσσολθμεθισμοδεθπερΔθορεβθμαλιqθιπδενιqθεθτcθqθιεσσεελιτδεσερθντΑμετσcριπταπαρτιενδονεvιμνοθσθιλλθμπορροπθτεντιδλαβορεσαλιενθμπροΑφφερτατομορθμcορρθμπιτναμαδαλιqθαμινερμισμεντιτθμεαμ'
                || 'ЛоремипсумдолорситаметехяуилибердоцендицоррумпитЕраталиенумносеаЕхтотатациматесирацундиаеумехерцицонсецтетуеридцумПробоатяуиделицатиссимиехяуосумосусципиантурхасехПродебитисмандамусеунеприиллудсонетрегионеТееамлегеремандамусПереталиатинцидунтНецибодицамлаудемусуанприталенуллаДолоресвертеремсплендидецумеуадмелопортересигниферумяуеАнвитаеорнатусопортеатеосадвисвениаминермисделенитиВихсуавитатенеглегентуртееумелталеиллуммалуиссетЕросмелиорехисеуПутентпосидониумтенецДицантделенитменандриутмеаадяуолатинеаргументумнемеиалтерумерудитицомплецтитурЦибототавидитеумцуменандрилаборамусеунамадагамграецохисЕхеумассумяуаестиоатеосцибомоллисТемелнолуиссеинтеллегатделицатиссимиНолаудемцонституамаппеллантурцумнецяуотграеценеЕпицуреипхаедруместехнулладебетеумутЕтхисенимаугуемуциуссцрипсеритвелидЕтвимунумволуптариасеабрутеинтеллегамцуМеатеелеифендмнесарчумяуисверотемпорибусусуанидперуллуминтеллегатХомероевертитурпосидониумиусетеимелвероаеяуеФацерпосситпробатусеинецСиттецонгуецонцептамседфацермоллислабитуринХабеосусципиантуридхасцасемаиоруминцорруптеехеосеосомнисвивендумаппеллантуранЕхмеаплацератплатонемцонцлусионемяуеПосситперицулаеамеаетдуофацерволуптариаяуаерендумДесеруиссесцриптореминяуиеивимпромптафеугиатхисеуетиамимпедитНовисопортеатволуптарианеглегентурдицампоссимеаяуиусуидаппетереинцидеринтцонцлудатуряуеДицоцорпорамеицуПервениамсаперетнеяуиеадолорессапиентемалияуандоансимуларгументумперВереарнонуместоряуатосмелетвисрецтеяуемолестиаецонсеяуунтуреиеоснеоффендитволуптуаратионибусСеаноессесаперетреформидансвимяуидамратионибусвитуператаеисединвидевидитДесерунтрепудиаределицатиссимицувиханперсиусерипуитцотидиеяуеприЯуимагнаиуваретфацилисцуинмоветириуреатоморумеамВисимпетусрецтеяуеевертитуреабрутериденсдолореснамнеНееффициантурделицатиссимияуоМеиеааеяуепосситнемореНовимяуаслудусаццусатаеиеумвертеремлуцилиусперсеяуерисНевелунумвероелояуентиамехпроеррорцонститутоантиопамеррорибусяуитеЕосепицуриоцурреретулламцорперноЕтомниуминструцтиорнамСитинутинамевертинихилдесеруиссееиперСедтеверосимулдигниссимпетентиумвулпутатенамеуНостроалияуамеуцумАлиипондерумхонестатисеапроутвихевертииндоцтумсапиентеметсеаодиоцаусаеПротеимпердиетсигниферумяуеЕамеиаццумсаномиттантурДуиснобиспертинахнецанцуцумалиенумпатриояуеадолесценсдебетнумяуамехприПаулоаперирилаореетяуиинмелнофастидиипетентиумЗриланциллаесадипсцингин';

        -- while (1=1) do
        -- begin
            p_beg = cast( 1 + rand() * ( char_length(lorem) - 63 ) as smallint);
            n_for = 63; -- cast( 8 + rand() * (63-8) as smallint);
            update rsa set text_unencrypted = substring(:lorem from :p_beg for :n_for);

            rdb$set_context('USER_SESSION', 'SOURCE_TEXT', (select text_unencrypted from rsa rows 1));

            update rsa set k_prv = rsa_private(256);
            update rsa set k_pub = rsa_public(k_prv);
            -- update rsa set text_decrypted = rsa_decrypt(cast(text_encrypted as varbinary(32760)) key k_prv );

            -------------------------------------------------
            -- RSA_SIGN_HASH ( <string> KEY <private key> [HASH <hash>] [SALT_LENGTH <smallint>] )
            -- RSA_VERIFY_HASH ( <string> SIGNATURE <string> KEY <public key> [HASH <hash>] [SALT_LENGTH <smallint>] )
            -- RSA_ENCRYPT (<data> KEY <public_key> [LPARAM <tag>] [HASH <hash>])
            -- <hash>::= { MD5 | SHA1 | SHA256 | SHA512 } ; Default is SHA256.

            update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using md5) key k_prv hash md5) returning octet_length(text_rsa_sign) into rsa_sign_len_mg5;
            update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using md5) signature text_rsa_sign key k_pub hash md5) returning text_rsa_vrfy into rsa_vrfy_md5;

            update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha1) key k_prv hash sha1) returning octet_length(text_rsa_sign) into rsa_sign_len_sha1;
            update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha1) signature text_rsa_sign key k_pub hash sha1) returning text_rsa_vrfy into rsa_vrfy_sha1;

            update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha256) key k_prv hash sha256) returning octet_length(text_rsa_sign) into rsa_sign_len_sha256;
            update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha256) signature text_rsa_sign key k_pub hash sha256) returning text_rsa_vrfy into rsa_vrfy_sha256;

            update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha512) key k_prv hash sha512) returning octet_length(text_rsa_sign) into rsa_sign_len_sha512;
            update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha512) signature text_rsa_sign key k_pub hash sha512) returning text_rsa_vrfy into rsa_vrfy_sha512;

            --#################################################

            update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash md5) returning octet_length(text_encrypted) into encr_octet_len_md5;
            update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash md5) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD') into decryption_result_md5;
            -------------------------------------------------
            update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha1) returning octet_length(text_encrypted) into encr_octet_len_sha1;
            update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha1) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD') into decryption_result_sha1;
            -------------------------------------------------
            update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha256) returning octet_length(text_encrypted) into encr_octet_len_sha256;
            update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha256) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD') into decryption_result_sha256;
            -------------------------------------------------
            update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha512) returning octet_length(text_encrypted) into encr_octet_len_sha512;
            update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha512) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD') into decryption_result_sha512;
            -------------------------------------------------

            rdb$set_context('USER_SESSION', 'SOURCE_TEXT', null);

        -- end
        suspend;

    end
    ^
    execute block returns(problem_text varchar(512) character set utf8, problem_octets_len smallint) as
    begin
        for
            select problem_text, octet_length(problem_text)
            from (
                select rdb$get_context('USER_SESSION', 'SOURCE_TEXT') as problem_text
                from rdb$database
            )
            where problem_text is not null
            into problem_text, problem_octets_len
        do
            suspend;
    end
    ^
    set term ;^

"""

act = isql_act('db', test_script)

expected_stdout = """
    RSA_SIGN_LEN_MG5                256
    RSA_VRFY_MD5                    <true>

    RSA_SIGN_LEN_SHA1               256
    RSA_VRFY_SHA1                   <true>

    RSA_SIGN_LEN_SHA256             256
    RSA_VRFY_SHA256                 <true>

    RSA_SIGN_LEN_SHA512             256
    RSA_VRFY_SHA512                 <true>

    ENCR_OCTET_LEN_MD5              256
    DECRYPTION_RESULT_MD5           OK.

    ENCR_OCTET_LEN_SHA1             256
    DECRYPTION_RESULT_SHA1          OK.

    ENCR_OCTET_LEN_SHA256           256
    DECRYPTION_RESULT_SHA256        OK.

    ENCR_OCTET_LEN_SHA512           256
    DECRYPTION_RESULT_SHA512        OK.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
