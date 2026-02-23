#coding:utf-8

"""
ID:          intfunc.encryption.rsa-family-pkcs15
ISSUE:       6929
TITLE:       RSA-family function, attitional test: verify ability to use PKCS_1_5 keyword
DESCRIPTION:
FBTEST:      functional.intfunc.encryption.rsa_family_pkcs_1_5
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

        -- After fixed gh-6929 ("Add support of PKCS v.1.5 padding to RSA functions ... "):
        -- RSA_SIGN_HASH ( <string> KEY <private key> [HASH <hash>] [SALT_LENGTH <smallint>] [PKCS_1_5] )
        -- RSA_VERIFY_HASH ( <string> SIGNATURE <string> KEY <public key> [HASH <hash>] [SALT_LENGTH <smallint>] [PKCS_1_5] )
        -- RSA_ENCRYPT ( <string> KEY <public key> [LPARAM <string>] [HASH <hash>] [PKCS_1_5] )

        -- <hash>::= { MD5 | SHA1 | SHA256 | SHA512 } ; Default is SHA256.

        execute statement 'update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using md5) key k_prv hash md5 PKCS_1_5) returning octet_length(text_rsa_sign)' into rsa_sign_len_mg5;
        execute statement 'update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using md5) signature text_rsa_sign key k_pub hash md5 PKCS_1_5) returning text_rsa_vrfy' into rsa_vrfy_md5;

        execute statement 'update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha1) key k_prv hash sha1 PKCS_1_5) returning octet_length(text_rsa_sign)' into rsa_sign_len_sha1;
        execute statement 'update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha1) signature text_rsa_sign key k_pub hash sha1  PKCS_1_5) returning text_rsa_vrfy' into rsa_vrfy_sha1;

        execute statement 'update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha256) key k_prv hash sha256 PKCS_1_5) returning octet_length(text_rsa_sign)' into rsa_sign_len_sha256;
        execute statement 'update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha256) signature text_rsa_sign key k_pub hash sha256 PKCS_1_5) returning text_rsa_vrfy' into rsa_vrfy_sha256;

        execute statement 'update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha512) key k_prv hash sha512 PKCS_1_5) returning octet_length(text_rsa_sign)' into rsa_sign_len_sha512;
        execute statement 'update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha512) signature text_rsa_sign key k_pub hash sha512 PKCS_1_5) returning text_rsa_vrfy' into rsa_vrfy_sha512;

        --#################################################

        execute statement q'{update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash md5 PKCS_1_5) returning octet_length(text_encrypted)}' into encr_octet_len_md5;
        execute statement q'{update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash md5 PKCS_1_5) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD')}' into decryption_result_md5;
        -------------------------------------------------
        execute statement q'{update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha1 PKCS_1_5) returning octet_length(text_encrypted)}' into encr_octet_len_sha1;
        execute statement q'{update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha1 PKCS_1_5) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD')}' into decryption_result_sha1;
        -------------------------------------------------
        execute statement q'{update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha256 PKCS_1_5) returning octet_length(text_encrypted)}' into encr_octet_len_sha256;
        execute statement q'{update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha256 PKCS_1_5) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD')}' into decryption_result_sha256;
        -------------------------------------------------
        execute statement q'{update rsa set text_encrypted = rsa_encrypt(text_unencrypted key k_pub hash sha512 PKCS_1_5) returning octet_length(text_encrypted)}' into encr_octet_len_sha512;
        execute statement q'{update rsa set text_decrypted = rsa_decrypt(text_encrypted key k_prv hash sha512 PKCS_1_5) returning iif(text_unencrypted  = text_decrypted, 'OK.','BAD')}' into decryption_result_sha512;
        -------------------------------------------------

        rdb$set_context('USER_SESSION', 'SOURCE_TEXT', null);

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

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
