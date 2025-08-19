## **Specification: Database Replication Architectures**

**Document Version:** 1.0 **Date:** July 25, 2025

### **1. Introduction**

This document provides a detailed technical specification for three common database replication architectures: Single-Leader, Multi-Leader, and Leaderless. The purpose is to define the core concepts, operational mechanics, and trade-offs of each system to guide architectural decisions.

---

## **2. Single-Leader Replication**

Also known as **Master-Slave** or **Primary-Replica** replication. This is the most common and straightforward replication architecture.

### **2.1. Core Concept**

One server node is designated as the **leader** (primary). All write operations (`INSERT`, `UPDATE`, `DELETE`) must be sent to the leader. The leader applies these changes to its local database and then propagates them to one or more **follower** (replica) nodes. Follower nodes can be used to serve read-only queries.

### **2.2. Write Path**

1. A client application sends a write request to the leader node.

2. The leader validates the request and writes the data change to its transaction log (e.g., a write-ahead log or binary log).

3. The leader applies the change to its own database.

4. The leader sends the transaction log entry to all follower nodes. This can be done in one of two ways:
   
   - **Synchronous Replication:** The leader waits for at least one (or all) followers to confirm they have received the change before reporting success to the client. This guarantees consistency but increases write latency.
   
   - **Asynchronous Replication:** The leader reports success to the client as soon as the change is committed locally, without waiting for followers. This provides low write latency but introduces **replication lag**, where followers may be temporarily out of sync.

### **2.3. Read Path**

Read requests can be handled by either the leader or any of the followers.

- **Reading from the Leader:** Guarantees the client reads the most up-to-date data (known as **Read-Your-Writes Consistency**).

- **Reading from Followers:** Distributes read load, improving read throughput and scalability. However, due to replication lag, a client may read stale data.

### **2.4. Fault Tolerance & Failover**

If the leader node fails, the system cannot accept any new writes. To restore write availability, a failover process must occur:

1. **Failure Detection:** The system must detect that the leader is unavailable (e.g., via heartbeats).

2. **Leader Election:** A follower node must be promoted to become the new leader. This can be a manual process or an automated one handled by a consensus algorithm.

3. **Reconfiguration:** Clients and other followers must be notified of the new leader's address.

### **2.5. Key Characteristics**

| Characteristic   | Specification                                                                                                                                               |
| ---------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Consistency**  | **Strong.** All writes are serialized through a single leader, eliminating write conflicts. Stale reads are possible from followers in asynchronous setups. |
| **Availability** | **High for reads.** Write availability is dependent on a single leader node.                                                                                |
| **Latency**      | Write latency is dependent on network RTT to the leader. Read latency can be very low if followers are geographically distributed.                          |
| **Complexity**   | **Low to Medium.** The architecture is well-understood and relatively simple to reason about.                                                               |
| **Use Cases**    | General-purpose applications, financial systems, e-commerce backends, and any system where strong data integrity is paramount.                              |

---

## **3. Multi-Leader Replication**

Also known as **Master-Master** or **Active-Active** replication.

### **3.1. Core Concept**

Two or more nodes are designated as leaders. Each leader can accept write operations. Leaders are responsible for applying writes locally and propagating them to every other leader node to achieve a synchronized state.

### **3.2. Write Path**

1. A client sends a write request to any leader, typically the one that is geographically closest.

2. The receiving leader applies the change to its local database.

3. The leader asynchronously sends the change to all other leader nodes in the cluster.

4. Each remote leader receives the change and applies it to its local database.

### **3.3. Conflict Resolution**

Because writes can occur concurrently on different leaders, conflicts are inevitable and must be resolved. This is the central challenge of multi-leader replication.

- **Specification:** The system **must** implement a deterministic, automated conflict resolution strategy.

- **Common Strategies:**
  
  1. **Last Write Wins (LWW):** Each write is tagged with a high-precision timestamp. If two writes conflict, the one with the later timestamp is kept, and the other is discarded. This is simple but can lead to data loss.
  
  2. **Source Priority:** Each leader is assigned a unique, static rank (e.g., `Leader_1 > Leader_2`). If writes conflict, the change from the higher-ranked leader always wins.
  
  3. **Application Logic:** Custom code merges conflicting data based on business rules (e.g., merging two users' edits to the same document).

### **3.4. Fault Tolerance**

The system is highly available for writes. If one leader fails or a data-center goes offline, other leaders can continue to accept writes without interruption. The failed leader can re-synchronize its data from other leaders upon recovery.

### **3.5. Key Characteristics**

| Characteristic   | Specification                                                                                                                                                          |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Consistency**  | **Eventual.** Data is consistent across leaders, but with a delay. The primary challenge is handling write conflicts correctly.                                        |
| **Availability** | **High for reads and writes.** Tolerates the failure of individual leader nodes.                                                                                       |
| **Latency**      | **Very low.** Clients can perform reads and writes on a local leader, avoiding cross-continent network hops.                                                           |
| **Complexity**   | **High.** The logic for conflict resolution adds significant complexity to the system.                                                                                 |
| **Use Cases**    | Multi-datacenter applications requiring local performance, collaborative software (e.g., Google Docs), and applications that need to continue operating while offline. |

---

## **4. Leaderless Replication**

Also known as **Peer-to-Peer** or **Dynamo-style** replication.

### **4.1. Core Concept**

There are no special leader nodes. All replica nodes are peers and can accept both read and write requests for the data they store. Consistency is managed through the use of **quorums**.

### **4.2. Key Terminology**

- **N (Replication Factor):** The total number of nodes that will store a copy of each piece of data.

- **W (Write Quorum):** The number of replicas that must acknowledge a write for it to be considered successful.

- **R (Read Quorum):** The number of replicas that must respond to a read request before the result is returned to the client.

### **4.3. Write Path**

1. A client sends a write request to any node, which acts as a **coordinator** for that request.

2. The coordinator sends the write operation to all `N` replicas for that piece of data.

3. The coordinator waits for acknowledgements from at least `W` replicas.

4. Once `W` acknowledgements are received, the coordinator reports success to the client. The remaining `N-W` replicas are updated in the background.

### **4.4. Read Path**

1. A client sends a read request to a coordinator node.

2. The coordinator sends read requests for the data to all `N` replicas.

3. The coordinator waits for responses from at least `R` replicas.

4. It analyzes the version numbers of the data from the `R` responses and returns the value with the most recent version to the client.

### **4.5. Consistency & Conflict Resolution**

Consistency is tunable based on the quorum settings.

- **Strong Consistency:** If `W + R > N`, the read and write sets are guaranteed to overlap on at least one node, ensuring a client will never read stale data.

- **Eventual Consistency:** If `W + R <= N`, it's possible for a read to miss the latest write, resulting in eventual consistency.

- **Conflict Resolution:**
  
  - **Version Clocks:** To resolve concurrent writes (when two clients write to the same data before either write has been replicated), each piece of data is stored with a version clock. If a read returns data with conflicting versions ("siblings"), the coordinator returns both, and the client application is responsible for merging them.
  
  - **Read Repair:** When a coordinator detects different versions on replicas during a read, it automatically pushes the newest version to any replicas with stale data.
  
  - **Anti-Entropy:** Background processes constantly run to compare data between replicas and resolve any inconsistencies found.

### **4.6. Key Characteristics**

| Characteristic   | Specification                                                                                                                                                        |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Consistency**  | **Tunable.** Can be configured for strong or eventual consistency based on `W`, `R`, and `N` values.                                                                 |
| **Availability** | **Extremely High.** The system provides maximum "always-on" availability and partition tolerance. It can sustain multiple node failures.                             |
| **Latency**      | **Low and consistent.** Reads and writes can be handled by any node.                                                                                                 |
| **Complexity**   | **High.** Application logic may need to be aware of data merging. Reasoning about system state is more difficult than in leader-based models.                        |
| **Use Cases**    | "Always-on" services where availability is the top priority (e.g., Amazon's shopping cart), high-volume data stores, and systems that must be highly fault-tolerant. |
