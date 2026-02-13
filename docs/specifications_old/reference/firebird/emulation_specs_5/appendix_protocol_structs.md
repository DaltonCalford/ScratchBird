# Appendix: Firebird 5 Wire Protocol Structs (Authoritative)

These C-style struct definitions specify the canonical field ordering and nesting for wire packets. Field types map to XDR encodings as defined in `30_wire_protocol.md`.

## Core Packet Structs

```c
typedef struct p_cnct
{
	USHORT	p_cnct_operation;			// unused
	USHORT	p_cnct_cversion;			// Version of connect protocol
	P_ARCH	p_cnct_client;				// Architecture of client
	CSTRING_CONST	p_cnct_file;		// File name
	USHORT	p_cnct_count;				// Protocol versions understood
	CSTRING_CONST	p_cnct_user_id;		// User identification stuff
	struct	p_cnct_repeat
	{
		USHORT	p_cnct_version;			// Protocol version number
		P_ARCH	p_cnct_architecture;	// Architecture of client
		USHORT	p_cnct_min_type;		// Minimum type (unused)
		USHORT	p_cnct_max_type;		// Maximum type
		USHORT	p_cnct_weight;			// Preference weight
	}	p_cnct_versions[MAX_CNCT_VERSIONS];
} P_CNCT;

typedef struct p_acpt
{
	USHORT	p_acpt_version;			// Protocol version number
	P_ARCH	p_acpt_architecture;	// Architecture for protocol
	USHORT	p_acpt_type;			// Minimum type
} P_ACPT;

struct p_acpd : public p_acpt
{
	CSTRING	p_acpt_data;			// Returned auth data
	CSTRING	p_acpt_plugin;			// Plugin to continue with
	USHORT	p_acpt_authenticated;	// Auth complete in single step (few! strange...)
	CSTRING p_acpt_keys;			// Keys known to the server
};

typedef struct p_resp
{
	OBJCT		p_resp_object;		// Object id
	SQUAD		p_resp_blob_id;		// Blob id
	CSTRING		p_resp_data;		// Data
	Firebird::DynamicStatusVector* p_resp_status_vector;
} P_RESP;

typedef struct p_atch
{
	OBJCT	p_atch_database;		// Database object id
	CSTRING_CONST	p_atch_file;	// File name
	CSTRING_CONST	p_atch_dpb;		// Database parameter block
} P_ATCH;

typedef struct p_cmpl
{
	OBJCT	p_cmpl_database;		// Database object id
	CSTRING_CONST	p_cmpl_blr;		// Request blr
} P_CMPL;

typedef struct p_sttr
{
	OBJCT	p_sttr_database;		// Database object id
	CSTRING_CONST	p_sttr_tpb;		// Transaction parameter block
} P_STTR;

typedef struct p_rlse
{
	OBJCT	p_rlse_object;			// Object to be released
} P_RLSE;

typedef struct p_data
{
    OBJCT	p_data_request;			// Request object id
    USHORT	p_data_incarnation;		// Incarnation of request
    OBJCT	p_data_transaction;		// Transaction object id
    USHORT	p_data_message_number;	// Message number in request
    USHORT	p_data_messages;		// Number of messages
} P_DATA;

typedef struct p_trrq
{
    OBJCT	p_trrq_database;		// Database object id
    OBJCT	p_trrq_transaction;		// Transaction object id
    CSTRING	p_trrq_blr;				// Message blr
    USHORT	p_trrq_messages;		// Number of messages
} P_TRRQ;

typedef struct p_blob
{
    OBJCT	p_blob_transaction;		// Transaction
    SQUAD	p_blob_id;				// Blob id for open
    CSTRING_CONST	p_blob_bpb;		// Blob parameter block
} P_BLOB;

typedef struct p_sgmt
{
    OBJCT	p_sgmt_blob;			// Blob handle id
    USHORT	p_sgmt_length;			// Length of segment
    CSTRING_CONST	p_sgmt_segment;	// Data segment
} P_SGMT;

typedef struct p_seek
{
    OBJCT	p_seek_blob;		// Blob handle id
    SSHORT	p_seek_mode;		// mode of seek
    SLONG	p_seek_offset;		// Offset of seek
} P_SEEK;

typedef struct p_info
{
    OBJCT	p_info_object;				// Object of information
    USHORT	p_info_incarnation;			// Incarnation of object
    CSTRING_CONST	p_info_items;		// Information
    CSTRING_CONST	p_info_recv_items;	// Receive information
    ULONG	p_info_buffer_length;		// Target buffer length
} P_INFO;

typedef struct p_event
{
    OBJCT	p_event_database;			// Database object id
    CSTRING_CONST	p_event_items;		// Event description block
    FPTR_EVENT_CALLBACK p_event_ast;	// Address of ast routine
    SLONG	p_event_arg;				// Argument to ast routine
    SLONG	p_event_rid;				// Client side id of remote event
} P_EVENT;

typedef struct p_prep
{
    OBJCT	p_prep_transaction;
    CSTRING_CONST	p_prep_data;
} P_PREP;

typedef struct p_req
{
    USHORT	p_req_type;			// Connection type
    OBJCT	p_req_object;		// Related object
    ULONG	p_req_partner;		// Partner identification
} P_REQ;

typedef struct p_ddl
{
     OBJCT	p_ddl_database;		// Database object id
     OBJCT	p_ddl_transaction;	// Transaction
     CSTRING_CONST	p_ddl_blr;	// Request blr
} P_DDL;

typedef struct p_slc
{
    OBJCT	p_slc_transaction;	// Transaction
    SQUAD	p_slc_id;			// Slice id
    CSTRING	p_slc_sdl;			// Slice description language
    CSTRING	p_slc_parameters;	// Slice parameters
    lstring	p_slc_slice;		// Slice proper
    ULONG	p_slc_length;		// Number of elements
} P_SLC;

typedef struct p_slr
{
    lstring	p_slr_slice;		// Slice proper
    ULONG	p_slr_length;		// Total length of slice
    UCHAR* p_slr_sdl;			// *** not transferred ***
    USHORT	p_slr_sdl_length;	// *** not transferred ***
} P_SLR;

typedef struct p_sqlst
{
    OBJCT	p_sqlst_transaction;		// transaction object
    OBJCT	p_sqlst_statement;			// statement object
    USHORT	p_sqlst_SQL_dialect;		// the SQL dialect
    CSTRING_CONST	p_sqlst_SQL_str;	// statement to be prepared
    ULONG	p_sqlst_buffer_length;		// Target buffer length
    CSTRING_CONST	p_sqlst_items;		// Information
    // This should be CSTRING_CONST
    CSTRING	p_sqlst_blr;				// blr describing message
    USHORT	p_sqlst_message_number;
    USHORT	p_sqlst_messages;			// Number of messages
    CSTRING	p_sqlst_out_blr;			// blr describing output message
    USHORT	p_sqlst_out_message_number;
	USHORT	p_sqlst_flags;				// prepare flags
	ULONG	p_sqlst_inline_blob_size;	// maximum size of inlined blob
} P_SQLST;

typedef struct p_sqldata
{
    OBJCT	p_sqldata_statement;		// statement object
    OBJCT	p_sqldata_transaction;		// transaction object
    // This should be CSTRING_CONST, but fetch() has strange behavior.
    CSTRING	p_sqldata_blr;				// blr describing message
    USHORT	p_sqldata_message_number;
    USHORT	p_sqldata_messages;			// Number of messages
    CSTRING	p_sqldata_out_blr;			// blr describing output message
    USHORT	p_sqldata_out_message_number;
    ULONG	p_sqldata_status;			// final eof status
	ULONG	p_sqldata_timeout;			// statement timeout
	ULONG	p_sqldata_cursor_flags;		// cursor flags
	P_FETCH	p_sqldata_fetch_op;			// Fetch operation
	SLONG	p_sqldata_fetch_pos;		// Fetch position
	ULONG	p_sqldata_inline_blob_size;	// maximum size of inlined blob
} P_SQLDATA;

typedef struct p_sqlfree
{
    OBJCT	p_sqlfree_statement;	// statement object
    USHORT	p_sqlfree_option;		// option
} P_SQLFREE;

typedef struct p_sqlcur
{
    OBJCT	p_sqlcur_statement;				// statement object
    CSTRING_CONST	p_sqlcur_cursor_name;	// cursor name
    USHORT	p_sqlcur_type;					// type of cursor
} P_SQLCUR;

typedef struct p_trau
{
	CSTRING	p_trau_data;					// Context
} P_TRAU;

typedef struct p_auth_continue
{
	CSTRING	p_data;							// Specific data
	CSTRING p_name;							// Plugin name
	CSTRING p_list;							// Plugin list
	CSTRING p_keys;							// Keys available on server
} P_AUTH_CONT;

struct p_update_account
{
    OBJCT			p_account_database;		// Database object id
    CSTRING_CONST	p_account_apb;			// Account parameter block (apb)
};

struct p_authenticate
{
    OBJCT			p_auth_database;		// Database object id
    CSTRING_CONST	p_auth_dpb;				// Database parameter block w/ user credentials
	CSTRING			p_auth_items;			// Information
	CSTRING			p_auth_recv_items;		// Receive information
	USHORT			p_auth_buffer_length;	// Target buffer length (transmitted but not used)
};

typedef struct p_cancel_op
{
    USHORT	p_co_kind;			// Kind of cancelation
} P_CANCEL_OP;

typedef struct p_crypt
{
	CSTRING p_plugin;						// Crypt plugin name
	CSTRING p_key;							// Key name / keys available on server
} P_CRYPT;

typedef struct p_crypt_callback
{
	CSTRING	p_cc_data;						// User's data
	USHORT p_cc_reply;
} P_CRYPT_CALLBACK;

typedef struct p_crypt_callback
{
	CSTRING	p_cc_data;						// User's data
	USHORT p_cc_reply;
} P_CRYPT_CALLBACK;

typedef struct p_batch_create
{
    OBJCT			p_batch_statement;	// statement object
    CSTRING_CONST	p_batch_blr;		// blr describing input messages
    ULONG			p_batch_msglen;		// explicit message length
    CSTRING_CONST   p_batch_pb;			// parameters block
} P_BATCH_CREATE;

typedef struct p_batch_msg
{
	OBJCT	p_batch_statement;			// statement object
	ULONG	p_batch_messages;			// number of messages
	CSTRING p_batch_data;
} P_BATCH_MSG;

typedef struct p_batch_exec
{
	OBJCT	p_batch_statement;			// statement object
	OBJCT   p_batch_transaction;		// transaction object
} P_BATCH_EXEC;

typedef struct p_batch_cs				// completion state
{
    OBJCT	p_batch_statement;			// statement object
	ULONG	p_batch_reccount;			// total records
	ULONG	p_batch_updates;			// update counters
	ULONG	p_batch_vectors;			// recnum + status vector pairs
	ULONG	p_batch_errors;				// error's recnums
} P_BATCH_CS;

typedef struct p_batch_blob
{
	OBJCT			p_batch_statement;	// statement object
	CSTRING			p_batch_blob_data;	// data
} P_BATCH_BLOB;

typedef struct p_batch_regblob
{
	OBJCT			p_batch_statement;	// statement object
	SQUAD			p_batch_exist_id;	// id of blob to register
	SQUAD			p_batch_blob_id;	// blob id
} P_BATCH_REGBLOB;

typedef struct p_batch_setbpb
{
	OBJCT			p_batch_statement;	// statement object
	CSTRING_CONST	p_batch_blob_bpb;	// BPB
} P_BATCH_SETBPB;

typedef struct p_replicate
{
     OBJCT			p_repl_database;	// database object id
     CSTRING_CONST	p_repl_data;		// replication data
} P_REPLICATE;

typedef struct packet
{
#ifdef DEBUG_XDR_MEMORY
	// When XDR memory debugging is enabled, p_malloc must be
	// the first subpacket and be followed by p_operation (see
	// server.c/zap_packet())

    P_MALLOC	p_malloc [P_MALLOC_SIZE]; // Debug xdr memory allocations
#endif
    P_OP	p_operation;		// Operation/packet type
    P_CNCT	p_cnct;				// Connect block
    P_ACPT	p_acpt;				// Accept connection
    P_ACPD	p_acpd;				// Accept connection with data
    P_RESP	p_resp;				// Generic response to a call
    P_ATCH	p_atch;				// Attach or create database
    P_RLSE	p_rlse;				// Release object
    P_DATA	p_data;				// Data packet
    P_CMPL	p_cmpl;				// Compile request
    P_STTR	p_sttr;				// Start transactions
    P_BLOB	p_blob;				// Create/Open blob
    P_SGMT	p_sgmt;				// Put_segment
    P_INFO	p_info;				// Information
    P_EVENT	p_event;			// Que event
    P_PREP	p_prep;				// New improved prepare
    P_REQ	p_req;				// Connection request
    P_DDL	p_ddl;				// Data definition call
    P_SLC	p_slc;				// Slice operator
    P_SLR	p_slr;				// Slice response
    P_SEEK	p_seek;				// Blob seek
    P_SQLST	p_sqlst;			// DSQL Prepare & Execute immediate
    P_SQLDATA	p_sqldata;		// DSQL Open Cursor, Execute, Fetch
    P_SQLCUR	p_sqlcur;		// DSQL Set cursor name
    P_SQLFREE	p_sqlfree;		// DSQL Free statement
    P_TRRQ	p_trrq;				// Transact request packet
	P_TRAU	p_trau;				// Trusted authentication
	p_update_account p_account_update;
	p_authenticate p_authenticate_user;
	P_CANCEL_OP p_cancel_op;	// Cancel operation
	P_AUTH_CONT p_auth_cont;	// Request more auth data
	P_CRYPT p_crypt;			// Start wire crypt
	P_CRYPT_CALLBACK p_cc;		// Database crypt callback
	P_BATCH_CREATE p_batch_create; // Create batch interface
	P_BATCH_MSG p_batch_msg;	// Add messages to batch
	P_BATCH_EXEC p_batch_exec;	// Run batch
	P_BATCH_CS p_batch_cs;		// Batch completion state
	P_BATCH_BLOB p_batch_blob;	// BLOB stream portion in batch
	P_BATCH_REGBLOB p_batch_regblob;	// Register already existing BLOB in batch
	P_BATCH_SETBPB p_batch_setbpb;		// Set default BPB for batch
	P_REPLICATE p_replicate;	// replicate
	P_INLINE_BLOB p_inline_blob;		// inline blob

public:
	packet() noexcept
	{
		memset(this, 0, sizeof(*this));
	}
} PACKET;

#endif // REMOTE_PROTOCOL_H
```
