#coding:utf-8

"""
ID:          intfunc.encryption.sha3
TITLE:       SHA3 support for the built-in function CRYPT_HASH()
DESCRIPTION:
    Idea of test see in test_rsa_family.py
    Commit in 'master' branch:
    https://github.com/FirebirdSQL/firebird/commit/167d28f188b85aa28fc51a2c08179f5c06e8c5d1
NOTES:
    [12.08.2023] pzotov
    Checked on 5.0.0.1163
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
        rsa_sign_len_sha3_224 smallint
       ,rsa_vrfy_sha3_224 boolean
       ,rsa_sign_len_sha3_256 smallint
       ,rsa_vrfy_sha3_256 boolean
       ,rsa_sign_len_sha3_384 smallint
       ,rsa_vrfy_sha3_384 boolean
       ,rsa_sign_len_sha3_512 smallint
       ,rsa_vrfy_sha3_512 boolean
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

        -------------------------------------------------

        update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha3_224) key k_prv hash sha256) returning octet_length(text_rsa_sign) into rsa_sign_len_sha3_224;
        update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha3_224) signature text_rsa_sign key k_pub hash sha256) returning text_rsa_vrfy into rsa_vrfy_sha3_224;

        update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha3_256) key k_prv hash sha256) returning octet_length(text_rsa_sign) into rsa_sign_len_sha3_256;
        update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha3_256) signature text_rsa_sign key k_pub hash sha256) returning text_rsa_vrfy into rsa_vrfy_sha3_256;

        update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha3_384) key k_prv hash sha256) returning octet_length(text_rsa_sign) into rsa_sign_len_sha3_384;
        update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha3_384) signature text_rsa_sign key k_pub hash sha256) returning text_rsa_vrfy into rsa_vrfy_sha3_384;

        update rsa set text_rsa_sign = rsa_sign_hash( crypt_hash(text_unencrypted using sha3_512) key k_prv hash sha512) returning octet_length(text_rsa_sign) into rsa_sign_len_sha3_512;
        update rsa set text_rsa_vrfy = rsa_verify_hash( crypt_hash(text_unencrypted using sha3_512) signature text_rsa_sign key k_pub hash sha512) returning text_rsa_vrfy into rsa_vrfy_sha3_512;

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
    RSA_SIGN_LEN_SHA3_224           256
    RSA_VRFY_SHA3_224               <true>
    RSA_SIGN_LEN_SHA3_256           256
    RSA_VRFY_SHA3_256               <true>
    RSA_SIGN_LEN_SHA3_384           256
    RSA_VRFY_SHA3_384               <true>
    RSA_SIGN_LEN_SHA3_512           256
    RSA_VRFY_SHA3_512               <true>
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
