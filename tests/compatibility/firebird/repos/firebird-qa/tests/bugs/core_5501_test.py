#coding:utf-8

"""
ID:          issue-5770
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5770
TITLE:       Unclear gstat's diagnostic when damaged page in DB file appears encrypted
DESCRIPTION:
    Test creates table 'TEST' with varchar and blob fields, + index on varchar, and add some data to it.
    Long data is added into BLOB column in order to prevent acomodation of its content within data page.
    As result, this table should have pages of three different types: DataPage, BTreePage and BlobPage.

    First, we obtain number of generators page (from rdb$pages).
    Then we find number of first PP of 'TEST' table by scrolling RDB$PAGES join RDB$RELATIONS result set.
    After this we:
    * define type of every page starting from first PP for 'TEST' table and up to total pages of DB,
      and doing this for each subsequent page. Dictionary 'broken_pages_map' is used to store LIST of pages
      for each encountered page type;
    * close connection;
    * open test DB file in binary mode for writing and:
        ** store previous content of .fdb in variable 'raw_db_content' (for further restore);
        ** for every page types that are stored in broken_pages_map.keys():
            *** get list of pages of that type which must be broken;
            *** if page_type is POINTER_PAGE or IDX_ROOT_PAGE - do nothing (we can get problems if these pages are broken);
            *** otherwise put 'garbage bytes' in each of these pages (multiple pages for each type will be damaged);
    * close DB file
    * ENCRYPT database, see call of func 'run_encr_decr';
    * run 'gstat -e' and check its output for presense of several expected patterns:
        ** "Data pages: total encrypted, non-crypted"
        ** "Index pages: total encrypted, non-crypted"
        ** "Blob pages: total encrypted, non-crypted"
        ** "Generator pages: total encrypted, non-crypted"
        ** "Other pages: total ENCRYPTED (DB problem!), non-crypted"
    * run 'gfix -v -full' and check its log for presense of several expected patterns:
        ** "(expected data"
        ** "(expected index B-tree"
        ** "(expected blob"
        ** "(expected generators"
    * open DB file again as binary and restore its content from var. 'raw_db_content'
JIRA:        CORE-5501
FBTEST:      bugs.core_5501
NOTES:
    [17.02.2024] pzotov
    Test fully re-implemented:
    * random data for inserting into TEST table not used anymore;
    * database *will* be encrypted (previous version of this test did not that).
    * because this test must be performed on FB-3.x, we have to check that encryption thread completed by parsing
      of 'gstat -h' log for presense of line "Attributes encrypted, plugin {ENCRYPTION_PLUGIN}".
      We can NOT use mon$database table: FB 3.x has no 'mon$crypt_state' column in it.
    * content 'garbage bytes' that are written into DB pages is fixed;
    * following page types will be fulfilled with 'garbage bytes': DATA_PAGE, IDX_B_TREE, BLOB_PAGE, GENS_PAGE;
    * following page types will be *preserved* from damage: POINTER_PAGE, IDX_ROOT_PAGE;
    * validation is performed using 'gfix -v -full' rather than online validation, otherwise info about broken
      generators page not reported.
    
    Commits:
        FB 4.x (was 'master' at that time): 10-mar-2017 17:08
        https://github.com/FirebirdSQL/firebird/commit/8e865303b0afe00c28795d9f6ee9983d14d85e1a
        Fixed CORE-5501: Unclear gstat's diagnostic when damaged page in DB file appears encrypted

        FB 3.x: 10-mar-2017 17:08
        https://github.com/FirebirdSQL/firebird/commit/3e5ac855467fe334e2f350d5210cb237bcefe0a6
        Backported fix for CORE-5501: Unclear gstat's diagnostic when damaged page in DB file appears encrypted

    Checked on:
        * 3.0.2.32962; output of 'gstat -e' contains only THREE lines in this snapshot ("Data pages"; "Index pages"; "Blob pages");
        * 3.0.2.32963; line "Other pages: total ..., ENCRYPTED ... (DB problem!), non-crypted ..." appearted in the 'gstat -e' output since this snapshot;
        * 3.0.12.33731, 4.0.5.3059, 5.0.1.1340, 6.0.0.264; line "Generator pages" presents in all these snapshots.
"""

import os
import time
import datetime as py_dt
from typing import Dict
import pytest
import re
from difflib import unified_diff
from struct import unpack_from
from firebird.qa import *
from firebird.driver import Connection, DatabaseError

###########################
###   S E T T I N G S   ###
###########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN' if os.name == 'nt' else 'MAX_WAIT_FOR_ENCR_FINISH_NIX'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red

DB_PAGE_SIZE = 8192
TXT_LEN = 500
init_script = f"""
    alter database drop linger;
    commit;

    create sequence g;
    create table test(id int, s varchar({TXT_LEN}) unique using index test_s_unq, b blob);
    commit;

    set count on;
    set term ^;
    execute block as
        declare n_cnt int = 2000;
        declare v_b blob;
        declare v_c varchar({TXT_LEN});
    begin
        select left( list( r ), {DB_PAGE_SIZE}+1) from (select row_number()over() as r from rdb$types,rdb$types) into v_b;
        v_c = rpad( '', {TXT_LEN} - 6, 'A');
        while (n_cnt > 0) do
        begin
            insert into test(id, s, b) values(gen_id(g,1), rpad( '', {TXT_LEN}, 'QWERTYUIOPLKJHGFDSAZXCVBNM' || :n_cnt ), iif(:n_cnt = 1, :v_b, null));
            n_cnt = n_cnt - 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script, page_size = DB_PAGE_SIZE)

substitutions=[
                ('total \\d+,', 'total'),
                ('non-crypted \\d+', 'non-crypted'),
                ('crypted \\d+', 'crypted'),
                ('ENCRYPTED \\d+', 'ENCRYPTED'),
              ]

act = python_act('db', substitutions = substitutions)

POINTER_PAGE = 4
DATA_PAGE = 5
IDX_ROOT_PAGE = 6
IDX_B_TREE = 7
BLOB_PAGE = 8
GENS_PAGE = 9

PAGE_TYPES = {0				: "undef/free",
              1				: "DB header",
              2				: "PIP",
              3				: "TIP",
              POINTER_PAGE	: "Pntr Page",
              DATA_PAGE		: "Data Page",
              IDX_ROOT_PAGE	: "Indx Root",
              IDX_B_TREE	: "Indx Data",
              BLOB_PAGE		: "Blob Page",
              GENS_PAGE		: "Gens Page",
              10			: "SCN" # only for ODS>=12
             }


#-----------------------------------------------------------------------

def run_encr_decr(act: Action, mode, max_wait_encr_thread_finish, capsys):
    if mode == 'encrypt':
        # alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}"' # <<< ::: NB ::: DO NOT add '... key "{ENCRYPTION_KEY}"' here!
        alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
         
        wait_for_state = 'Database encrypted'
    elif mode == 'decrypt':
        alter_db_sttm = 'alter database decrypt'
        wait_for_state = 'Database not encrypted'

    e_thread_finished = False

    # 0 = non crypted;
    # 1 = has been encrypted;
    # 2 = is DEcrypting;
    # 3 = is Encrypting;
    #
    # only since FB 4.x: REQUIRED_CRYPT_STATE = 1 if mode == 'encrypt' else 0
    current_crypt_state = -1

    REQUIRED_CRYPT_PAGE = 0
    current_crypt_page = -1

    d1 = py_dt.timedelta(0)
    with act.db.connect() as con:
        t1=py_dt.datetime.now()
        try:
            d1 = t1-t1
            con.execute_immediate(alter_db_sttm)
            con.commit()
            time.sleep(1)

            # Pattern to check for completed encryption thread:
            completed_encr_pattern = re.compile(f'Attributes\\s+encrypted,\\s+plugin\\s+{ENCRYPTION_PLUGIN}', re.IGNORECASE)
            while True:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > max_wait_encr_thread_finish:
                    break
    
                ######################################################
                ###   C H E C K    M O N $ C R Y P T _ S T A T E   ###
                ######################################################
                # Invoke 'gstat -h' and read its ouput.
                # Encryption can be considered as COMPLETED when we will found:
                # "Attributes              encrypted, plugin fbSampleDbCrypt"
                #
                act.gstat(switches=['-h'])
                for line in act.stdout.splitlines():
                    if completed_encr_pattern.match(line.strip()):
                        e_thread_finished = True
                        break
                if e_thread_finished:
                    break
                else:
                    time.sleep(0.5)

        except DatabaseError as e:
            print( e.__str__() )

    assert e_thread_finished, f'TIMEOUT EXPIRATION. Mode="{mode}" took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {max_wait_encr_thread_finish} ms; current_crypt_page={current_crypt_page}'

#-----------------------------------------------------------------------

def fill_dbo(con: Connection, map_dbo: Dict):
    cur = con.cursor()
    sql = """
        select rel_id, rel_name, idx_id, idx_name
        from (
            select
                rr.rdb$relation_id rel_id,                  -- 0
                rr.rdb$relation_name rel_name,              -- 1
                -1 idx_id,                                  -- 2
                '' idx_name,                                -- 3
                rr.rdb$relation_type rel_type,
                rr.rdb$system_flag sys_flag
            from rdb$relations rr

            union all

            select
                rr.rdb$relation_id rel_id,                  -- 0
                rr.rdb$relation_name rel_name,              -- 1
                coalesce(ri.rdb$index_id-1,-1) idx_id,      -- 2
                coalesce(ri.rdb$index_name,'') idx_name,    -- 3
                rr.rdb$relation_type rel_type,
                rr.rdb$system_flag sys_flag
            from rdb$relations rr
            join rdb$indices ri on
            rr.rdb$relation_name = ri.rdb$relation_name
        ) r
        where
            coalesce(r.rel_type,0) = 0 --  exclude views, GTT and external tables
            and r.sys_flag is distinct from 1
    """
    cur.execute(sql)
    for r in cur:
        map_dbo[r[0], r[2]] = (r[1].strip(), r[3].strip())

#-----------------------------------------------------------------------

def parse_page_header(con: Connection, page_number: int, map_dbo: Dict):
    page_buffer = con.info.get_page_content(page_number)

    # dimitr, 20.01.2017 ~13:00
    # all *CHAR = 1 byte, *SHORT = 2 bytes, *LONG = 4 bytes.

    # https://docs.python.org/2/library/struct.html
    # struct.unpack_from(fmt, buffer[, offset=0])
    # Unpack the buffer according to the given format.
    # The result is a tuple even if it contains exactly one item.
    # The buffer must contain at least the amount of data required by the format
    # len(buffer[offset:]) must be at least calcsize(fmt).
    # First character of the format string can be used to indicate the byte order,
    # size and alignment of the packed data
    # Native byte order is big-endian or little-endian:
    # <     little-endian
    # >     big-endian
    # Intel x86 and AMD64 (x86-64) are little-endian
    # Use sys.byteorder to check the endianness of your system:
    # https://docs.python.org/2/library/struct.html#format-characters
    # c     char     string of length 1
    # b     signed char
    # B     unsigned char
    # h     short
    # H     unsigned short     integer
    # i     int     integer 4
    # I     unsigned int     integer     4
    # l     long (4)
    # L     unsigned long (4)
    # q     long long (8)
    # Q     unsigned long long

    page_type = unpack_from('<b', page_buffer)[0]

    relation_id = -1
    index_id = -1
    segment_cnt = -1 # for Data page: number of record segments on page
    index_id = -1
    ix_level = -1
    btr_len = -1

    if page_type == POINTER_PAGE:
        # POINTER pege:
        # *pag* dpg_header=16, SLONG dpg_sequence=4, SLONG ppg_next=4, USHORT ppg_count=2 ==> 16+4+4+2=26
        # struct pointer_page
        # {
        #     pag ppg_header;
        #     SLONG ppg_sequence;   // Sequence number in relation
        #     SLONG ppg_next;       // Next pointer page in relation
        #     USHORT ppg_count;     // Number of slots active
        #     USHORT ppg_relation;  // Relation id
        #     USHORT ppg_min_space; // Lowest slot with space available
        #     USHORT ppg_max_space; // Highest slot with space available
        #     SLONG ppg_page[1];    // Data page vector
        # };
        relation_id = unpack_from('<H', page_buffer, 26)[0] # 'H' ==> USHORT
    elif page_type == DATA_PAGE:
        # DATA page:
        # *pag* dpg_header=16, SLONG dpg_sequence=4 ==> 16+4 = 20:
        # struct data_page
        # {
        #   16  pag dpg_header;
        #    4 SLONG dpg_sequence;   // Sequence number in relation
        #    2 USHORT dpg_relation;  // Relation id
        #    2 USHORT dpg_count;     // Number of record segments on page
        #     struct dpg_repeat
        #     {
        #         USHORT dpg_offset; // Offset of record fragment
        #         USHORT dpg_length; // Length of record fragment
        #     } dpg_rpt[1];
        # };
        relation_id = unpack_from('<H', page_buffer, 20)[0] # 'H' ==> USHORT
        segment_cnt = unpack_from('<H', page_buffer, 22)[0]
    elif page_type == IDX_ROOT_PAGE:
        # Index root page
        # struct index_root_page
        # {
        #     pag irt_header;
        #     USHORT irt_relation;            // relation id (for consistency)
        relation_id = unpack_from('<H', page_buffer, 16)[0] # 'H' ==> USHORT
    elif page_type == IDX_B_TREE:
        # B-tree page ("bucket"):
        # struct btree_page
        # {
        # 16   pag btr_header;
        #  4   SLONG btr_sibling;         // right sibling page
        #  4   SLONG btr_left_sibling;    // left sibling page
        #  4   SLONG btr_prefix_total;    // sum of all prefixes on page
        #  2   USHORT btr_relation;       // relation id for consistency
        #  2   USHORT btr_length;         // length of data in bucket
        #  1   UCHAR btr_id;              // index id for consistency
        #  1   UCHAR btr_level;           // index level (0 = leaf)
        #     btree_nod btr_nodes[1];
        # };
        relation_id = unpack_from('<H', page_buffer, 28)[0]  # 'H' ==> USHORT
        btr_len = unpack_from('<H', page_buffer, 30)[0]  # 'H' ==> USHORT // length of data in bucket
        index_id = unpack_from('<B', page_buffer, 32)[0] # 'B' => UCHAR
        ix_level = unpack_from('<B', page_buffer, 33)[0]
    #
    if index_id>=0 and (relation_id, index_id) in map_dbo:
        u = map_dbo[ relation_id, index_id ]
        page_info = f'{PAGE_TYPES[page_type].ljust(9)}, {u[1].strip()}, data_len={btr_len}, lev={ix_level}'
        #page_info = ''.join((PAGE_TYPES[page_type].ljust(9), ', ', u[1].strip(), ', data_len=', str(btr_len), ', lev=', str(ix_level))) # 'Indx Page, <index_name>, <length of data in bucket>'
    elif (relation_id, -1) in map_dbo:
        u = map_dbo[ relation_id, -1 ]
        if page_type == DATA_PAGE:
            page_info = f'{PAGE_TYPES[page_type].ljust(9)}, {u[0].strip()}, segments on page: {segment_cnt}'
            #page_info = ''.join( ( PAGE_TYPES[page_type].ljust(9),', ',u[0].strip(),', segments on page: ',str(segment_cnt) ) ) # '<table_name>, segments on page: NNN' - for Data page
        else:
            page_info = f'{PAGE_TYPES[page_type].ljust(9)}, {u[0].strip()}'
            #page_info = ''.join( ( PAGE_TYPES[page_type].ljust(9),', ',u[0].strip() ) ) # '<table_name>' - for Pointer page
    elif relation_id == -1:
        page_info = PAGE_TYPES[page_type].ljust(9)
    else:
        page_info = f'UNKNOWN; {PAGE_TYPES[page_type].ljust(9)}; relation_id {relation_id}; index_id {index_id}'
        #page_info = ''.join( ('UNKNOWN; ',PAGE_TYPES[page_type].ljust(9),'; relation_id ', str(relation_id), '; index_id ', str(index_id)) )
    return (page_type, relation_id, page_info)


################################
###    M A I N    C O D E    ###
################################

@pytest.mark.encryption
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, capsys):
    map_dbo = {}

    # Query to find first generators page number:
    first_gens_page_sql = f"""
        select p.rdb$page_number
        from rdb$pages p
        where p.rdb$page_type = {GENS_PAGE}
        order by p.rdb$page_number desc
        rows 1
    """

    # Query to find relation_id and first PP for 'TEST' table:
    first_pp_sql = f"""
        select p.rdb$relation_id, p.rdb$page_number
        from rdb$pages p
        join rdb$relations r on p.rdb$relation_id = r.rdb$relation_id
        where r.rdb$relation_name=upper('TEST') and p.rdb$page_type = {POINTER_PAGE}
        order by p.rdb$page_number
        rows 1
    """

    broken_pages_map = { POINTER_PAGE : [], DATA_PAGE : [], IDX_ROOT_PAGE : [], IDX_B_TREE : [], BLOB_PAGE : [], GENS_PAGE : [] }
    with act.db.connect() as con:
        fill_dbo(con, map_dbo)
        c = con.cursor()

        broken_pages_map[GENS_PAGE] = [c.execute(first_gens_page_sql).fetchone()[0],]

        test_rel_id, test_rel_first_pp = c.execute(first_pp_sql).fetchone()

        # Found first page for each of three types: Data, Index and Blob
        # (loop starts from first PointerPage of table 'TEST')
        brk_datapage = brk_indxpage = brk_blobpage = -1

        for page_no in range(test_rel_first_pp, con.info.pages_allocated):
            page_type, relation_id, page_info = parse_page_header(con, page_no, map_dbo)
            #print('page:',page_no, '; page_type:',page_type, '; test_rel_id:',relation_id,';',  page_info)
            if relation_id == test_rel_id and page_type == POINTER_PAGE:
                brk_datapage = page_no
                broken_pages_map[POINTER_PAGE].append(page_no)
            elif relation_id == test_rel_id and page_type == DATA_PAGE:
                brk_datapage = page_no
                broken_pages_map[DATA_PAGE].append(page_no)
            elif relation_id == test_rel_id and page_type == IDX_ROOT_PAGE:
                brk_indxpage = page_no
                broken_pages_map[IDX_ROOT_PAGE].append(page_no)
            elif relation_id == test_rel_id and page_type == IDX_B_TREE:
                brk_indxpage = page_no
                broken_pages_map[IDX_B_TREE].append(page_no)
            elif page_type == BLOB_PAGE: #  relation_id == test_rel_id and
                brk_blobpage = page_no
                broken_pages_map[BLOB_PAGE].append(page_no)
            
            if min([ len(v) for v in broken_pages_map.values() ]) > 0:
                break

            #if brk_datapage > 0 and brk_indxpage > 0 and brk_blobpage > 0:
            #    break


    assert min([ len(v) for v in broken_pages_map.values() ]) > 0, f'At least one of required page types was not found: broken_pages_map = {broken_pages_map}'

    # Preserve binary content of .fdb for futher restore:
    #
    raw_db_content = act.db.db_path.read_bytes()

    #################################################
    ###   P U T    G A R B A G E    I N     D B   ###
    #################################################
    # DO NOT!! >>> garbage_bytes = bytearray(os.urandom(16)) -- this can cause missed 'Other pages:' line in the 'gstat -e' output
    garbage_bytes = bytearray(b'\x1e\xaa\\,es\x06\x92B3\x0c\xa7e\xa6\x04\x0f') # <<< this value WILL CAUSE appearance of line 'Other pages' in the 'gstat -e' output'
    with open(act.db.db_path, 'r+b') as w:
        for pg_type, pg_no_lst in broken_pages_map.items():
            # See letters to Vlad, 17-feb-2024.
            # We have to PRESERVE from damage TWO page types: POINTER_PAGE and IDX_ROOT_PAGE.
            # If we preserve from damage only POINTER_PAGE then
            # 5.0.1.1340 and 6.0.0.264 will crash during validation via TCP.
            # 3.0.12.33791 and 4.0.5.3059 will perform validation but DB remains opened and second call to validation fails with
            #     bad parameters on attach or create database
            #     -secondary server attachments cannot validate databases
            # Also, if IDX_ROOT_PAGE is damaged then b-tree pages will not be handled during validation (no messages in firebird.log about them).
            if pg_type in (POINTER_PAGE,IDX_ROOT_PAGE):
                 pass
            else:
                for p in pg_no_lst:
                    w.seek(p * con.info.page_size)
                    w.write(garbage_bytes)


    ############################################
    ###   E N C R Y P T    D A T A B A S E   ###
    ############################################
    run_encr_decr(act, 'encrypt', MAX_WAITING_ENCR_FINISH, capsys)

    #################################################
    ###    P A R S E    G S T A T   O U T P U T   ###
    #################################################
    act.gstat(switches=['-e'])
    pattern = re.compile('(data|index|blob|generator|other)\\s+page(s)?(:)?', re.IGNORECASE)
    for line in act.stdout.splitlines():
        if pattern.match(line.strip()):
            print(line.strip())
    
    # Check-1. Output of 'gstat -e' must contain lines:
    #
    act.expected_stdout = """
        Data pages: total encrypted, non-crypted
        Index pages: total encrypted, non-crypted
        Blob pages: total encrypted, non-crypted
        Generator pages: total encrypted, non-crypted
        Other pages: total ENCRYPTED (DB problem!), non-crypted
    """
    
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()


    # Check-2 [optional].
    ############################################
    ###    G F I X  - V A L I D A T I O N    ###
    ############################################
    # Get firebird.log content BEFORE running validation
    log_before = act.get_firebird_log()

    # ::: NB :::
    # do NOT use online validation: it does not check generators page.
    #
    act.gfix(switches=['-v', '-full', act.db.dsn])

    # Get firebird.log content AFTER running validation
    log_after = act.get_firebird_log()

    # Difference between old and new firebird.log should contain lines:
    #   "(expected data"
    #   "(expected index B-tree"
    #   "(expected blob"
    #   "(expected generators"
    found_broken_types_map = { DATA_PAGE : 0, IDX_B_TREE : 0, BLOB_PAGE : 0, GENS_PAGE : 0 }
    for line in unified_diff(log_before, log_after):
        if line.startswith('+'):
            if ' (expected data' in line:
                found_broken_types_map[DATA_PAGE] = 1
            elif ' (expected index B-tree' in line:
                found_broken_types_map[IDX_B_TREE] = 1
            elif ' (expected blob' in line:
                found_broken_types_map[BLOB_PAGE] = 1
            elif ' (expected generators' in line:
                found_broken_types_map[GENS_PAGE] = 1

    parse_validation_log_overall_msg = 'Result of parsing validation log:'
    print(parse_validation_log_overall_msg)
    for k,v in sorted( found_broken_types_map.items() ):
        print(k, v)

    act.expected_stdout = f"""
        {parse_validation_log_overall_msg}
        {DATA_PAGE} 1
        {IDX_B_TREE} 1
        {BLOB_PAGE} 1
        {GENS_PAGE} 1
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # restore NON-BROKEN DB content:
    act.db.db_path.write_bytes(raw_db_content)
