# ScratchBird Transaction Management - Distributed Transactions
## Part 3 of Transaction and Lock Management Specification

## Overview

ScratchBird's distributed transaction support enables ACID properties across multiple database nodes, combining two-phase commit (2PC) protocol, three-phase commit (3PC) for better fault tolerance, and modern consensus algorithms like Raft for coordination.

## 1. Distributed Transaction Architecture

### 1.1 Global Transaction Manager

```c
// Global transaction manager for distributed transactions
typedef struct global_transaction_manager {
    // Node identity
    UUID            gtm_node_uuid;         // This node's UUID
    char            gtm_node_name[256];    // Node name
    NodeRole        gtm_role;              // Node role
    
    // Cluster configuration
    ClusterConfig*  gtm_cluster_config;    // Cluster configuration
    NodeRegistry*   gtm_node_registry;     // Known nodes
    
    // Transaction coordination
    HashTable*      gtm_global_txns;       // Global transactions
    HashTable*      gtm_local_txns;        // Local participants
    
    // Communication
    RPCClient*      gtm_rpc_client;        // RPC client
    RPCServer*      gtm_rpc_server;        // RPC server
    
    // Consensus (for coordinator election)
    RaftNode*       gtm_raft_node;         // Raft consensus node
    
    // Recovery
    PreparedTxnLog* gtm_prepared_log;      // Prepared transactions
    RecoveryManager* gtm_recovery_mgr;     // Recovery manager
    
    // Statistics
    DistTxnStats    gtm_stats;             // Statistics
} GlobalTransactionManager;

// Node roles in distributed system
typedef enum node_role {
    NODE_ROLE_COORDINATOR,      // Can coordinate distributed transactions
    NODE_ROLE_PARTICIPANT,      // Can participate in distributed transactions
    NODE_ROLE_WITNESS,          // Witness-only (for quorum)
    NODE_ROLE_STANDBY           // Standby replica
} NodeRole;

// Distributed transaction structure
typedef struct distributed_transaction {
    // Global identity
    UUID            dtx_global_id;         // Global transaction ID
    char            dtx_gid[200];          // Global ID string
    
    // Coordinator information
    UUID            dtx_coordinator_uuid;  // Coordinator node
    TransactionId   dtx_coordinator_xid;   // Coordinator's local XID
    
    // Participants
    ParticipantList* dtx_participants;     // Participant nodes
    uint32_t        dtx_n_participants;    // Number of participants
    
    // State
    DTxState        dtx_state;             // Current state
    DTxProtocol     dtx_protocol;          // Protocol (2PC/3PC/Raft)
    
    // Timestamps
    TimestampTz     dtx_start_time;        // Start time
    TimestampTz     dtx_prepare_time;      // Prepare time
    TimestampTz     dtx_commit_time;       // Commit/abort time
    
    // Voting results
    VoteResult*     dtx_votes;             // Participant votes
    uint32_t        dtx_yes_votes;         // Yes votes
    uint32_t        dtx_no_votes;          // No votes
    
    // Recovery information
    LSN             dtx_prepare_lsn;       // Prepare LSN
    LSN             dtx_commit_lsn;        // Commit LSN
    
    // Timeout management
    TimestampTz     dtx_timeout;           // Transaction timeout
} DistributedTransaction;

// Distributed transaction states
typedef enum dtx_state {
    DTX_STATE_ACTIVE,           // Transaction active
    DTX_STATE_PREPARING,        // Prepare phase
    DTX_STATE_PREPARED,         // Prepared (2PC)
    DTX_STATE_PRE_COMMIT,       // Pre-commit (3PC)
    DTX_STATE_COMMITTING,       // Commit phase
    DTX_STATE_COMMITTED,        // Committed
    DTX_STATE_ABORTING,         // Abort phase
    DTX_STATE_ABORTED,          // Aborted
    DTX_STATE_RECOVERING        // Recovery in progress
} DTxState;

// Distributed transaction protocols
typedef enum dtx_protocol {
    DTX_PROTOCOL_2PC,           // Two-phase commit
    DTX_PROTOCOL_3PC,           // Three-phase commit
    DTX_PROTOCOL_RAFT,          // Raft consensus
    DTX_PROTOCOL_SAGA           // Saga pattern (compensating)
} DTxProtocol;
```

### 1.2 Participant Management

```c
// Participant in distributed transaction
typedef struct participant {
    // Identity
    UUID            p_node_uuid;           // Node UUID
    char            p_node_address[256];   // Network address
    uint16_t        p_node_port;           // Port number
    
    // Transaction info
    TransactionId   p_local_xid;           // Local transaction ID
    
    // State
    ParticipantState p_state;              // Current state
    VoteType        p_vote;                // Vote (yes/no/abstain)
    
    // Communication
    RPCConnection*  p_connection;          // RPC connection
    uint32_t        p_timeout_ms;          // Communication timeout
    
    // Statistics
    uint64_t        p_messages_sent;       // Messages sent
    uint64_t        p_messages_received;   // Messages received
    TimestampTz     p_last_contact;        // Last contact time
} Participant;

// Participant states
typedef enum participant_state {
    PART_STATE_ACTIVE,          // Active in transaction
    PART_STATE_PREPARED,        // Prepared to commit
    PART_STATE_PRE_COMMITTED,   // Pre-committed (3PC)
    PART_STATE_COMMITTED,       // Committed
    PART_STATE_ABORTED,         // Aborted
    PART_STATE_FAILED,          // Failed/unreachable
    PART_STATE_RECOVERING       // Recovering
} ParticipantState;

// Vote types
typedef enum vote_type {
    VOTE_UNKNOWN,               // Not voted yet
    VOTE_YES,                   // Can commit
    VOTE_NO,                    // Must abort
    VOTE_ABSTAIN                // Abstain (read-only)
} VoteType;
```

## 2. Two-Phase Commit (2PC) Protocol

### 2.1 Coordinator Side 2PC

```c
// Begin distributed transaction (coordinator)
DistributedTransaction* begin_distributed_transaction(
    GlobalTransactionManager* gtm,
    NodeList* participants)
{
    // Create distributed transaction
    DistributedTransaction* dtx = allocate(sizeof(DistributedTransaction));
    dtx->dtx_global_id = generate_uuid_v7();
    snprintf(dtx->dtx_gid, sizeof(dtx->dtx_gid), 
             "GTX_%s_%ld", 
             uuid_to_string(dtx->dtx_global_id),
             GetCurrentTimestamp());
    
    dtx->dtx_coordinator_uuid = gtm->gtm_node_uuid;
    dtx->dtx_coordinator_xid = allocate_transaction_id();
    dtx->dtx_protocol = DTX_PROTOCOL_2PC;
    dtx->dtx_state = DTX_STATE_ACTIVE;
    
    // Initialize participants
    dtx->dtx_participants = create_participant_list();
    
    for (Node* node : participants) {
        Participant* p = allocate(sizeof(Participant));
        p->p_node_uuid = node->uuid;
        p->p_state = PART_STATE_ACTIVE;
        p->p_connection = establish_rpc_connection(node);
        
        // Send BEGIN to participant
        RPCMessage msg;
        msg.type = RPC_BEGIN_TRANSACTION;
        msg.global_id = dtx->dtx_global_id;
        msg.coordinator = gtm->gtm_node_uuid;
        
        RPCResponse* resp = send_rpc_message(p->p_connection, &msg);
        
        if (resp && resp->status == RPC_OK) {
            p->p_local_xid = resp->local_xid;
            add_participant(dtx->dtx_participants, p);
            dtx->dtx_n_participants++;
        } else {
            // Failed to start on participant
            abort_distributed_transaction(dtx);
            return NULL;
        }
    }
    
    // Add to global transaction table
    add_global_transaction(gtm, dtx);
    
    return dtx;
}

// Commit distributed transaction (2PC coordinator)
Status commit_distributed_2pc(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    // Phase 1: Prepare
    dtx->dtx_state = DTX_STATE_PREPARING;
    bool can_commit = true;
    
    // Send PREPARE to all participants
    for (Participant* p : dtx->dtx_participants) {
        RPCMessage msg;
        msg.type = RPC_PREPARE_TRANSACTION;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, PREPARE_TIMEOUT_MS);
        
        if (resp == NULL) {
            // Timeout or failure - must abort
            p->p_vote = VOTE_NO;
            p->p_state = PART_STATE_FAILED;
            can_commit = false;
            break;
        }
        
        p->p_vote = resp->vote;
        
        if (resp->vote == VOTE_YES) {
            p->p_state = PART_STATE_PREPARED;
            dtx->dtx_yes_votes++;
        } else if (resp->vote == VOTE_NO) {
            can_commit = false;
            dtx->dtx_no_votes++;
            break;  // Can abort immediately
        } else if (resp->vote == VOTE_ABSTAIN) {
            // Read-only participant
            p->p_state = PART_STATE_COMMITTED;
        }
    }
    
    // Make decision
    if (can_commit && dtx->dtx_yes_votes + 
        count_abstains(dtx) == dtx->dtx_n_participants) {
        // All voted yes or abstained - commit
        return commit_distributed_decision(gtm, dtx);
    } else {
        // At least one voted no or failed - abort
        return abort_distributed_decision(gtm, dtx);
    }
}

// Phase 2: Commit decision
Status commit_distributed_decision(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    dtx->dtx_state = DTX_STATE_COMMITTING;
    
    // Log commit decision (for recovery)
    log_commit_decision(dtx);
    
    // Send COMMIT to all prepared participants
    uint32_t committed = 0;
    
    for (Participant* p : dtx->dtx_participants) {
        if (p->p_state != PART_STATE_PREPARED) {
            continue;  // Skip non-prepared (abstained or failed)
        }
        
        RPCMessage msg;
        msg.type = RPC_COMMIT_PREPARED;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, COMMIT_TIMEOUT_MS);
        
        if (resp && resp->status == RPC_OK) {
            p->p_state = PART_STATE_COMMITTED;
            committed++;
        } else {
            // Participant will eventually recover and commit
            p->p_state = PART_STATE_RECOVERING;
        }
    }
    
    dtx->dtx_state = DTX_STATE_COMMITTED;
    dtx->dtx_commit_time = GetCurrentTimestamp();
    
    // Clean up
    cleanup_distributed_transaction(gtm, dtx);
    
    return STATUS_OK;
}

// Phase 2: Abort decision
Status abort_distributed_decision(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    dtx->dtx_state = DTX_STATE_ABORTING;
    
    // Log abort decision
    log_abort_decision(dtx);
    
    // Send ABORT to all participants
    for (Participant* p : dtx->dtx_participants) {
        if (p->p_state == PART_STATE_ABORTED ||
            p->p_state == PART_STATE_FAILED) {
            continue;  // Already aborted or failed
        }
        
        RPCMessage msg;
        msg.type = RPC_ABORT_PREPARED;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, ABORT_TIMEOUT_MS);
        
        if (resp && resp->status == RPC_OK) {
            p->p_state = PART_STATE_ABORTED;
        } else {
            // Participant will eventually recover and abort
            p->p_state = PART_STATE_RECOVERING;
        }
    }
    
    dtx->dtx_state = DTX_STATE_ABORTED;
    
    // Clean up
    cleanup_distributed_transaction(gtm, dtx);
    
    return STATUS_OK;
}
```

### 2.2 Participant Side 2PC

```c
// Handle prepare request (participant)
RPCResponse* handle_prepare_request(
    GlobalTransactionManager* gtm,
    RPCMessage* msg)
{
    // Find local transaction
    LocalTransaction* ltx = find_local_transaction(gtm, msg->global_id);
    
    if (ltx == NULL) {
        return create_error_response("Transaction not found");
    }
    
    RPCResponse* resp = allocate(sizeof(RPCResponse));
    
    // Check if can prepare
    if (ltx->read_only) {
        // Read-only can always commit
        resp->vote = VOTE_ABSTAIN;
        resp->status = RPC_OK;
        
        // Can release resources immediately
        release_local_transaction(ltx);
    } else {
        // Prepare local transaction
        Status status = prepare_local_transaction(ltx);
        
        if (status == STATUS_OK) {
            resp->vote = VOTE_YES;
            resp->status = RPC_OK;
            
            // Log prepare (for recovery)
            log_prepare_record(ltx);
            
            // Flush log to ensure durability
            flush_transaction_log();
            
            ltx->state = LTX_STATE_PREPARED;
        } else {
            resp->vote = VOTE_NO;
            resp->status = RPC_ERROR;
            resp->error_message = get_error_message(status);
            
            // Abort local transaction
            abort_local_transaction(ltx);
        }
    }
    
    return resp;
}

// Handle commit request (participant)
RPCResponse* handle_commit_request(
    GlobalTransactionManager* gtm,
    RPCMessage* msg)
{
    // Find prepared transaction
    LocalTransaction* ltx = find_prepared_transaction(gtm, msg->global_id);
    
    if (ltx == NULL) {
        // May have already committed (idempotent)
        return create_ok_response();
    }
    
    // Commit local transaction
    Status status = commit_prepared_local(ltx);
    
    RPCResponse* resp = allocate(sizeof(RPCResponse));
    
    if (status == STATUS_OK) {
        resp->status = RPC_OK;
        
        // Log commit
        log_commit_record(ltx);
        
        ltx->state = LTX_STATE_COMMITTED;
        
        // Clean up
        cleanup_local_transaction(ltx);
    } else {
        resp->status = RPC_ERROR;
        resp->error_message = get_error_message(status);
    }
    
    return resp;
}

// Handle abort request (participant)
RPCResponse* handle_abort_request(
    GlobalTransactionManager* gtm,
    RPCMessage* msg)
{
    // Find transaction (may be active or prepared)
    LocalTransaction* ltx = find_any_transaction(gtm, msg->global_id);
    
    if (ltx == NULL) {
        // May have already aborted (idempotent)
        return create_ok_response();
    }
    
    // Abort local transaction
    Status status = abort_local_transaction(ltx);
    
    RPCResponse* resp = allocate(sizeof(RPCResponse));
    
    if (status == STATUS_OK) {
        resp->status = RPC_OK;
        
        // Log abort
        log_abort_record(ltx);
        
        ltx->state = LTX_STATE_ABORTED;
        
        // Clean up
        cleanup_local_transaction(ltx);
    } else {
        resp->status = RPC_ERROR;
        resp->error_message = get_error_message(status);
    }
    
    return resp;
}
```

## 3. Three-Phase Commit (3PC) Protocol

### 3.1 3PC Coordinator

```c
// Commit distributed transaction using 3PC
Status commit_distributed_3pc(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    // Phase 1: CanCommit
    dtx->dtx_state = DTX_STATE_PREPARING;
    bool can_commit = true;
    
    // Send CAN_COMMIT to all participants
    for (Participant* p : dtx->dtx_participants) {
        RPCMessage msg;
        msg.type = RPC_CAN_COMMIT;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, CAN_COMMIT_TIMEOUT_MS);
        
        if (resp == NULL || resp->vote != VOTE_YES) {
            can_commit = false;
            break;
        }
        
        p->p_vote = resp->vote;
    }
    
    if (!can_commit) {
        // Abort immediately
        return abort_distributed_3pc(gtm, dtx);
    }
    
    // Phase 2: PreCommit
    dtx->dtx_state = DTX_STATE_PRE_COMMIT;
    bool all_pre_committed = true;
    
    // Send PRE_COMMIT to all participants
    for (Participant* p : dtx->dtx_participants) {
        RPCMessage msg;
        msg.type = RPC_PRE_COMMIT;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, PRE_COMMIT_TIMEOUT_MS);
        
        if (resp == NULL || resp->status != RPC_OK) {
            all_pre_committed = false;
            p->p_state = PART_STATE_FAILED;
        } else {
            p->p_state = PART_STATE_PRE_COMMITTED;
        }
    }
    
    if (!all_pre_committed) {
        // Some participants failed - need recovery
        return recover_3pc_transaction(gtm, dtx);
    }
    
    // Phase 3: DoCommit
    dtx->dtx_state = DTX_STATE_COMMITTING;
    
    // Send DO_COMMIT to all participants
    for (Participant* p : dtx->dtx_participants) {
        if (p->p_state != PART_STATE_PRE_COMMITTED) {
            continue;
        }
        
        RPCMessage msg;
        msg.type = RPC_DO_COMMIT;
        msg.global_id = dtx->dtx_global_id;
        
        RPCResponse* resp = send_rpc_message_timeout(
            p->p_connection, &msg, DO_COMMIT_TIMEOUT_MS);
        
        if (resp && resp->status == RPC_OK) {
            p->p_state = PART_STATE_COMMITTED;
        } else {
            // Will recover and commit eventually
            p->p_state = PART_STATE_RECOVERING;
        }
    }
    
    dtx->dtx_state = DTX_STATE_COMMITTED;
    return STATUS_OK;
}

// 3PC recovery when coordinator fails
Status recover_3pc_transaction(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    // Elect new coordinator if needed
    if (!is_coordinator(gtm)) {
        return elect_new_coordinator(gtm, dtx);
    }
    
    // Query all reachable participants
    uint32_t pre_committed = 0;
    uint32_t aborted = 0;
    uint32_t unknown = 0;
    
    for (Participant* p : dtx->dtx_participants) {
        ParticipantState state = query_participant_state(p);
        
        switch (state) {
            case PART_STATE_PRE_COMMITTED:
                pre_committed++;
                break;
            case PART_STATE_ABORTED:
                aborted++;
                break;
            default:
                unknown++;
                break;
        }
    }
    
    // Make recovery decision
    if (aborted > 0) {
        // Some aborted - must abort all
        return abort_distributed_3pc(gtm, dtx);
    } else if (pre_committed > 0) {
        // Some pre-committed - can commit
        return complete_3pc_commit(gtm, dtx);
    } else {
        // All unknown - safe to abort
        return abort_distributed_3pc(gtm, dtx);
    }
}
```

## 4. Raft Consensus for Distributed Transactions

### 4.1 Raft-Based Transaction Commit

```c
// Raft node for consensus
typedef struct raft_node {
    // Node identity
    UUID            rn_node_id;            // Node UUID
    RaftRole        rn_role;               // Current role
    uint64_t        rn_term;               // Current term
    
    // Leader election
    UUID            rn_voted_for;          // Voted for in current term
    TimestampTz     rn_election_timeout;   // Election timeout
    
    // Log replication
    RaftLog*        rn_log;                // Raft log
    uint64_t        rn_commit_index;       // Committed log index
    uint64_t        rn_last_applied;       // Last applied index
    
    // Cluster
    RaftPeer*       rn_peers;              // Peer nodes
    uint32_t        rn_n_peers;            // Number of peers
    
    // State machine
    TransactionStateMachine* rn_state_machine;
} RaftNode;

// Commit transaction using Raft
Status commit_distributed_raft(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    RaftNode* raft = gtm->gtm_raft_node;
    
    // Only leader can propose
    if (raft->rn_role != RAFT_LEADER) {
        // Forward to leader
        return forward_to_leader(raft, dtx);
    }
    
    // Create log entry for transaction
    RaftLogEntry* entry = allocate(sizeof(RaftLogEntry));
    entry->term = raft->rn_term;
    entry->index = raft->rn_log->next_index++;
    entry->type = LOG_ENTRY_TRANSACTION;
    entry->data = serialize_transaction(dtx);
    
    // Append to local log
    append_log_entry(raft->rn_log, entry);
    
    // Replicate to followers
    uint32_t replicated = 1;  // Count self
    uint32_t majority = (raft->rn_n_peers + 1) / 2 + 1;
    
    for (RaftPeer* peer : raft->rn_peers) {
        AppendEntriesRequest req;
        req.term = raft->rn_term;
        req.leader_id = raft->rn_node_id;
        req.prev_log_index = entry->index - 1;
        req.prev_log_term = get_log_term(raft->rn_log, entry->index - 1);
        req.entries = entry;
        req.leader_commit = raft->rn_commit_index;
        
        AppendEntriesResponse* resp = send_append_entries(peer, &req);
        
        if (resp && resp->success) {
            replicated++;
            
            if (replicated >= majority) {
                // Majority replicated - can commit
                raft->rn_commit_index = entry->index;
                
                // Apply to state machine
                apply_transaction_commit(raft->rn_state_machine, dtx);
                
                // Notify participants
                notify_raft_commit(dtx);
                
                return STATUS_OK;
            }
        }
    }
    
    // Failed to get majority
    return STATUS_NO_QUORUM;
}

// Handle Raft append entries (follower)
AppendEntriesResponse* handle_append_entries(
    RaftNode* raft,
    AppendEntriesRequest* req)
{
    AppendEntriesResponse* resp = allocate(sizeof(AppendEntriesResponse));
    resp->term = raft->rn_term;
    
    // Check term
    if (req->term < raft->rn_term) {
        resp->success = false;
        return resp;
    }
    
    // Update term if needed
    if (req->term > raft->rn_term) {
        raft->rn_term = req->term;
        raft->rn_role = RAFT_FOLLOWER;
        raft->rn_voted_for = InvalidUUID;
    }
    
    // Reset election timeout
    reset_election_timeout(raft);
    
    // Check log consistency
    if (req->prev_log_index > 0) {
        RaftLogEntry* prev = get_log_entry(raft->rn_log, req->prev_log_index);
        
        if (prev == NULL || prev->term != req->prev_log_term) {
            resp->success = false;
            return resp;
        }
    }
    
    // Append entries
    append_log_entries(raft->rn_log, req->entries);
    
    // Update commit index
    if (req->leader_commit > raft->rn_commit_index) {
        raft->rn_commit_index = MIN(req->leader_commit, 
                                   get_last_log_index(raft->rn_log));
        
        // Apply committed entries
        apply_committed_entries(raft);
    }
    
    resp->success = true;
    return resp;
}
```

## 5. Failure Recovery

### 5.1 Coordinator Recovery

```c
// Recover after coordinator failure
Status recover_coordinator_failure(
    GlobalTransactionManager* gtm)
{
    // Read prepared transaction log
    PreparedTxnList* prepared = read_prepared_transactions(gtm->gtm_prepared_log);
    
    for (PreparedTxn* ptx : prepared) {
        // Reconstruct distributed transaction
        DistributedTransaction* dtx = reconstruct_transaction(ptx);
        
        // Query participant states
        QueryResult* results = query_all_participants(dtx);
        
        // Make recovery decision based on participant states
        RecoveryDecision decision = make_recovery_decision(results);
        
        switch (decision) {
            case RECOVERY_COMMIT:
                // All participants prepared/committed - commit
                complete_commit_recovery(gtm, dtx);
                break;
                
            case RECOVERY_ABORT:
                // Some participants aborted - abort all
                complete_abort_recovery(gtm, dtx);
                break;
                
            case RECOVERY_WAIT:
                // Cannot decide yet - wait for more info
                add_to_recovery_queue(gtm, dtx);
                break;
        }
    }
    
    return STATUS_OK;
}

// Query participant state
ParticipantState query_participant_state(Participant* p) {
    RPCMessage msg;
    msg.type = RPC_QUERY_STATE;
    msg.global_id = p->p_global_id;
    
    RPCResponse* resp = send_rpc_message_timeout(
        p->p_connection, &msg, QUERY_TIMEOUT_MS);
    
    if (resp == NULL) {
        return PART_STATE_FAILED;
    }
    
    return resp->participant_state;
}

// Make recovery decision
RecoveryDecision make_recovery_decision(QueryResult* results) {
    uint32_t prepared = 0;
    uint32_t committed = 0;
    uint32_t aborted = 0;
    uint32_t unknown = 0;
    
    for (ParticipantResult* pr : results) {
        switch (pr->state) {
            case PART_STATE_PREPARED:
                prepared++;
                break;
            case PART_STATE_COMMITTED:
                committed++;
                break;
            case PART_STATE_ABORTED:
                aborted++;
                break;
            default:
                unknown++;
                break;
        }
    }
    
    if (aborted > 0) {
        // Any abort means must abort
        return RECOVERY_ABORT;
    } else if (committed > 0 || 
              (prepared > 0 && unknown == 0)) {
        // Can commit if some committed or all prepared
        return RECOVERY_COMMIT;
    } else if (unknown > results->total / 2) {
        // Too many unknown - wait
        return RECOVERY_WAIT;
    } else {
        // Safe to abort
        return RECOVERY_ABORT;
    }
}
```

### 5.2 Participant Recovery

```c
// Recover participant after crash
Status recover_participant(GlobalTransactionManager* gtm) {
    // Read local prepared transaction log
    PreparedLocalTxnList* prepared = read_local_prepared_log();
    
    for (PreparedLocalTxn* pltx : prepared) {
        // Query coordinator for decision
        CoordinatorDecision decision = query_coordinator_decision(pltx);
        
        switch (decision) {
            case COORD_DECISION_COMMIT:
                // Coordinator says commit
                commit_prepared_local(pltx);
                break;
                
            case COORD_DECISION_ABORT:
                // Coordinator says abort
                abort_prepared_local(pltx);
                break;
                
            case COORD_DECISION_UNKNOWN:
                // Coordinator doesn't know - keep prepared
                keep_prepared(pltx);
                break;
                
            case COORD_DECISION_UNREACHABLE:
                // Cannot reach coordinator
                if (is_timeout_exceeded(pltx)) {
                    // Timeout - make heuristic decision
                    make_heuristic_decision(pltx);
                } else {
                    // Wait and retry
                    schedule_retry(pltx);
                }
                break;
        }
    }
    
    return STATUS_OK;
}

// Make heuristic decision when coordinator unreachable
void make_heuristic_decision(PreparedLocalTxn* pltx) {
    // Use configured heuristic policy
    HeuristicPolicy policy = get_heuristic_policy();
    
    switch (policy) {
        case HEURISTIC_ABORT:
            // Always abort on timeout
            abort_prepared_local(pltx);
            log_heuristic_abort(pltx);
            break;
            
        case HEURISTIC_COMMIT:
            // Always commit on timeout
            commit_prepared_local(pltx);
            log_heuristic_commit(pltx);
            break;
            
        case HEURISTIC_MAJORITY:
            // Query other participants
            MajorityDecision decision = query_participant_majority(pltx);
            
            if (decision == MAJORITY_COMMIT) {
                commit_prepared_local(pltx);
            } else {
                abort_prepared_local(pltx);
            }
            break;
            
        case HEURISTIC_WAIT:
            // Never make heuristic decision
            keep_prepared_indefinitely(pltx);
            break;
    }
}
```

## 6. Network Partition Handling

### 6.1 Split-Brain Prevention

```c
// Handle network partition
Status handle_network_partition(
    GlobalTransactionManager* gtm)
{
    // Detect partition
    PartitionInfo* partition = detect_partition(gtm);
    
    if (partition->is_partitioned) {
        // Check if we have quorum
        if (partition->nodes_in_partition < 
            gtm->gtm_cluster_config->total_nodes / 2 + 1) {
            // Minority partition - become read-only
            set_cluster_mode(CLUSTER_MODE_READ_ONLY);
            
            // Abort all active distributed transactions
            abort_all_distributed_transactions(gtm);
            
            return STATUS_NO_QUORUM;
        } else {
            // Majority partition - can continue
            set_cluster_mode(CLUSTER_MODE_DEGRADED);
            
            // Mark unreachable nodes as failed
            for (Node* node : partition->unreachable_nodes) {
                mark_node_failed(node);
                
                // Take over transactions from failed nodes
                takeover_failed_node_transactions(gtm, node);
            }
        }
    }
    
    return STATUS_OK;
}

// Detect partition using heartbeats
PartitionInfo* detect_partition(GlobalTransactionManager* gtm) {
    PartitionInfo* info = allocate(sizeof(PartitionInfo));
    info->nodes_in_partition = 1;  // Count self
    
    for (Node* node : gtm->gtm_node_registry->nodes) {
        HeartbeatResponse* resp = send_heartbeat(node);
        
        if (resp != NULL) {
            info->nodes_in_partition++;
            add_to_reachable(info, node);
        } else {
            add_to_unreachable(info, node);
        }
    }
    
    info->is_partitioned = (info->unreachable_nodes != NULL);
    
    return info;
}
```

## 7. Performance Optimizations

### 7.1 Read-Only Optimization

```c
// Optimize read-only distributed transactions
Status execute_read_only_distributed(
    GlobalTransactionManager* gtm,
    DistributedTransaction* dtx)
{
    // Use snapshot isolation for read-only
    GlobalSnapshot* snapshot = take_global_snapshot(gtm);
    
    // Send snapshot to all participants
    for (Participant* p : dtx->dtx_participants) {
        RPCMessage msg;
        msg.type = RPC_READ_ONLY_SNAPSHOT;
        msg.snapshot = snapshot;
        
        send_rpc_message_async(p->p_connection, &msg);
    }
    
    // No need for 2PC - can commit immediately
    dtx->dtx_state = DTX_STATE_COMMITTED;
    
    return STATUS_OK;
}

// Global snapshot for distributed reads
typedef struct global_snapshot {
    // Snapshot vector clock
    VectorClock*    gs_vector_clock;       // Vector of timestamps
    
    // Consistent cut
    TransactionId*  gs_xmax_per_node;      // Max XID per node
    TimestampTz     gs_timestamp;          // Global timestamp
    
    // Causality tracking
    DependencyGraph* gs_dependencies;      // Causal dependencies
} GlobalSnapshot;

// Take global snapshot
GlobalSnapshot* take_global_snapshot(GlobalTransactionManager* gtm) {
    GlobalSnapshot* snap = allocate(sizeof(GlobalSnapshot));
    
    // Initialize vector clock
    snap->gs_vector_clock = create_vector_clock(gtm->gtm_cluster_config->total_nodes);
    snap->gs_timestamp = GetCurrentTimestamp();
    
    // Get local snapshot
    snap->gs_xmax_per_node[gtm->gtm_node_id] = get_current_xid();
    snap->gs_vector_clock->clock[gtm->gtm_node_id] = snap->gs_timestamp;
    
    // Request snapshots from other nodes
    for (Node* node : gtm->gtm_node_registry->nodes) {
        SnapshotRequest req;
        req.requester = gtm->gtm_node_uuid;
        req.timestamp = snap->gs_timestamp;
        
        SnapshotResponse* resp = request_snapshot(node, &req);
        
        if (resp != NULL) {
            snap->gs_xmax_per_node[node->id] = resp->xmax;
            snap->gs_vector_clock->clock[node->id] = resp->timestamp;
        }
    }
    
    return snap;
}
```

### 7.2 Batching and Pipelining

```c
// Batch multiple operations in distributed transaction
typedef struct distributed_batch {
    UUID            db_batch_id;           // Batch ID
    OperationList*  db_operations;         // Operations to execute
    uint32_t        db_n_operations;       // Number of operations
    
    // Pipelining
    bool            db_pipelined;          // Use pipelining
    uint32_t        db_pipeline_depth;     // Pipeline depth
} DistributedBatch;

// Execute batched distributed transaction
Status execute_distributed_batch(
    GlobalTransactionManager* gtm,
    DistributedBatch* batch)
{
    // Start distributed transaction
    DistributedTransaction* dtx = begin_distributed_transaction(
        gtm, get_affected_nodes(batch));
    
    // Group operations by node
    NodeOperationMap* ops_by_node = group_operations_by_node(batch);
    
    // Send batched operations to each node
    for (NodeOps* node_ops : ops_by_node) {
        RPCMessage msg;
        msg.type = RPC_BATCH_EXECUTE;
        msg.global_id = dtx->dtx_global_id;
        msg.operations = node_ops->operations;
        msg.pipelined = batch->db_pipelined;
        
        if (batch->db_pipelined) {
            // Send without waiting for response
            send_rpc_message_async(node_ops->node->connection, &msg);
        } else {
            // Send and wait
            RPCResponse* resp = send_rpc_message(node_ops->node->connection, &msg);
            
            if (resp == NULL || resp->status != RPC_OK) {
                abort_distributed_transaction(dtx);
                return STATUS_BATCH_FAILED;
            }
        }
    }
    
    // If pipelined, collect responses
    if (batch->db_pipelined) {
        if (!collect_pipelined_responses(ops_by_node)) {
            abort_distributed_transaction(dtx);
            return STATUS_BATCH_FAILED;
        }
    }
    
    // Commit using chosen protocol
    return commit_distributed_transaction(gtm, dtx);
}
```

## Implementation Notes

This distributed transaction implementation provides:

1. **Multiple protocols**: 2PC, 3PC, and Raft consensus
2. **Fault tolerance**: Coordinator and participant recovery
3. **Network partition handling**: Quorum-based decision making
4. **Performance optimizations**: Read-only optimization, batching, pipelining
5. **Heuristic decisions**: Configurable policies for unreachable coordinators
6. **Global snapshots**: Consistent distributed reads
7. **Vector clocks**: Causality tracking for distributed operations

The system is designed to provide strong consistency guarantees while maintaining high availability and partition tolerance where possible, following the CAP theorem trade-offs.