/**
 * @file protocol_handler_tests.cpp
 * @brief Tests for Phase 11.1.2: Protocol Handler Framework
 *
 * Testing generic protocol handler interface, message processing, and ScratchBird native protocol.
 */

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"
#include "scratchbird/engine/protocol_handler.h"
#include "scratchbird/engine/scratchbird_protocol.h"
#include "test_db_utils.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace scratchbird::engine;

//=============================================================================
// Test Category 1: Protocol Infrastructure Tests
//=============================================================================

void test_protocol_version_compatibility()
{
    std::cout << "\n=== Test 1.1: Protocol Version Compatibility ===" << std::endl;

    ProtocolVersion v1_0;
    v1_0.major = 1;
    v1_0.minor = 0;
    v1_0.type = ProtocolType::ScratchBirdNative;

    ProtocolVersion v1_1;
    v1_1.major = 1;
    v1_1.minor = 1;
    v1_1.type = ProtocolType::ScratchBirdNative;

    ProtocolVersion v2_0;
    v2_0.major = 2;
    v2_0.minor = 0;
    v2_0.type = ProtocolType::ScratchBirdNative;

    ProtocolVersion firebird_v1;
    firebird_v1.major = 1;
    firebird_v1.minor = 0;
    firebird_v1.type = ProtocolType::FirebirdWire;

    // Test compatibility
    assert(v1_1.is_compatible_with(v1_0));         // Newer server can handle older client
    assert(!v1_0.is_compatible_with(v1_1));        // Older server cannot handle newer client
    assert(!v2_0.is_compatible_with(v1_0));        // Major version mismatch
    assert(!v1_0.is_compatible_with(firebird_v1)); // Different protocol types

    std::cout << "✓ Protocol version compatibility working correctly" << std::endl;

    // Test string representation
    std::string version_str = v1_1.to_string();
    assert(version_str.find("ScratchBird") != std::string::npos);
    assert(version_str.find("1.1") != std::string::npos);
    std::cout << "✓ Protocol version string representation: " << version_str << std::endl;
}

void test_message_queue_operations()
{
    std::cout << "\n=== Test 1.2: Message Queue Operations ===" << std::endl;

    MessageQueue queue;
    assert(queue.empty());
    assert(queue.size() == 0);

    // Create test messages with different priorities
    auto high_msg = std::make_unique<ScratchBirdMessage>("HIGH");
    high_msg->priority = MessagePriority::High;
    high_msg->message_id = 1;

    auto normal_msg = std::make_unique<ScratchBirdMessage>("NORMAL");
    normal_msg->priority = MessagePriority::Normal;
    normal_msg->message_id = 2;

    auto low_msg = std::make_unique<ScratchBirdMessage>("LOW");
    low_msg->priority = MessagePriority::Low;
    low_msg->message_id = 3;

    // Enqueue messages in non-priority order
    queue.enqueue(std::move(normal_msg));
    queue.enqueue(std::move(low_msg));
    queue.enqueue(std::move(high_msg));

    assert(queue.size() == 3);
    assert(!queue.empty());
    std::cout << "✓ Message queue enqueue working correctly" << std::endl;

    // Dequeue should return highest priority first
    auto first = queue.dequeue();
    assert(first != nullptr);
    assert(first->priority == MessagePriority::High);
    assert(first->message_id == 1);

    auto second = queue.dequeue();
    assert(second != nullptr);
    assert(second->priority == MessagePriority::Normal);
    assert(second->message_id == 2);

    auto third = queue.dequeue();
    assert(third != nullptr);
    assert(third->priority == MessagePriority::Low);
    assert(third->message_id == 3);

    assert(queue.empty());
    std::cout << "✓ Message queue priority dequeue working correctly" << std::endl;
}

void test_correlation_tracker()
{
    std::cout << "\n=== Test 1.3: Correlation Tracker ===" << std::endl;

    CorrelationTracker tracker;
    assert(tracker.get_pending_count() == 0);

    // Create correlation IDs
    auto corr_id_1 = tracker.create_correlation_id();
    auto corr_id_2 = tracker.create_correlation_id();
    assert(corr_id_1 != corr_id_2);
    std::cout << "✓ Unique correlation ID generation working" << std::endl;

    // Register requests
    auto request_1 = std::make_unique<ScratchBirdMessage>("REQUEST_1");
    auto request_2 = std::make_unique<ScratchBirdMessage>("REQUEST_2");

    tracker.register_request(corr_id_1, std::move(request_1));
    tracker.register_request(corr_id_2, std::move(request_2));

    assert(tracker.get_pending_count() == 2);
    assert(tracker.has_pending_request(corr_id_1));
    assert(tracker.has_pending_request(corr_id_2));
    std::cout << "✓ Request registration working correctly" << std::endl;

    // Retrieve requests
    auto retrieved_1 = tracker.get_request(corr_id_1);
    assert(retrieved_1 != nullptr);
    assert(!tracker.has_pending_request(corr_id_1));
    assert(tracker.get_pending_count() == 1);

    // Complete correlation
    tracker.complete_correlation(corr_id_2);
    assert(!tracker.has_pending_request(corr_id_2));
    assert(tracker.get_pending_count() == 0);

    std::cout << "✓ Correlation tracking and cleanup working correctly" << std::endl;
}

//=============================================================================
// Test Category 2: Protocol Handler Factory Tests
//=============================================================================

void test_protocol_handler_factory()
{
    std::cout << "\n=== Test 2.1: Protocol Handler Factory ===" << std::endl;

    auto& factory = ProtocolHandlerFactory::get_instance();

    // Register ScratchBird protocol handler
    factory.register_handler(ProtocolType::ScratchBirdNative,
                             []() -> std::unique_ptr<ProtocolHandler> {
                                 return std::make_unique<ScratchBirdProtocolHandler>();
                             });

    // Test handler creation
    auto handler = factory.create_handler(ProtocolType::ScratchBirdNative);
    assert(handler != nullptr);
    assert(handler->get_protocol_type() == ProtocolType::ScratchBirdNative);
    std::cout << "✓ Protocol handler factory registration and creation working" << std::endl;

    // Test protocol detection
    std::vector<std::uint8_t> test_data = {0x00, 0x00, 0x00, 0x08, 'C', 'O',
                                           'N',  'N',  'E',  'C',  'T'};
    ProtocolType detected = factory.detect_protocol(test_data);
    assert(detected == ProtocolType::ScratchBirdNative);
    std::cout << "✓ Protocol detection working correctly" << std::endl;

    // Test supported protocols
    auto supported = factory.get_supported_protocols();
    bool found_scratchbird = false;
    for (auto protocol : supported) {
        if (protocol == ProtocolType::ScratchBirdNative) {
            found_scratchbird = true;
            break;
        }
    }
    assert(found_scratchbird);
    std::cout << "✓ Supported protocols enumeration working" << std::endl;
}

//=============================================================================
// Test Category 3: ScratchBird Protocol Tests
//=============================================================================

void test_scratchbird_message()
{
    std::cout << "\n=== Test 3.1: ScratchBird Message ===" << std::endl;

    ScratchBirdMessage message("TEST");
    assert(message.get_message_type_string() == "TEST");

    // Test payload string operations
    std::string test_payload = "Hello, ScratchBird!";
    message.set_payload_string(test_payload);
    assert(message.get_payload_string() == test_payload);
    std::cout << "✓ ScratchBird message payload operations working" << std::endl;

    // Test message cloning
    auto cloned = message.clone();
    assert(cloned != nullptr);
    auto sb_cloned = dynamic_cast<ScratchBirdMessage*>(cloned.get());
    assert(sb_cloned != nullptr);
    assert(sb_cloned->get_message_type_string() == "TEST");
    assert(sb_cloned->get_payload_string() == test_payload);
    std::cout << "✓ ScratchBird message cloning working correctly" << std::endl;
}

void test_scratchbird_state_machine()
{
    std::cout << "\n=== Test 3.2: ScratchBird State Machine ===" << std::endl;

    ScratchBirdStateMachine state_machine;
    assert(state_machine.get_current_state() == "Disconnected");

    // Test valid transitions
    assert(state_machine.transition_to_state("Connected"));
    assert(state_machine.get_current_state() == "Connected");

    assert(state_machine.transition_to_state("Authenticating"));
    assert(state_machine.get_current_state() == "Authenticating");

    assert(state_machine.transition_to_state("Authenticated"));
    assert(state_machine.get_current_state() == "Authenticated");
    std::cout << "✓ ScratchBird state machine valid transitions working" << std::endl;

    // Test invalid transitions
    assert(!state_machine.transition_to_state("Disconnected_Invalid"));
    assert(state_machine.get_current_state() == "Authenticated"); // Should remain unchanged
    std::cout << "✓ ScratchBird state machine invalid transition rejection working" << std::endl;

    // Test valid next states
    auto next_states = state_machine.get_valid_next_states();
    bool found_processing = false;
    bool found_disconnected = false;
    for (const auto& state : next_states) {
        if (state == "Processing")
            found_processing = true;
        if (state == "Disconnected")
            found_disconnected = true;
    }
    assert(found_processing && found_disconnected);
    std::cout << "✓ ScratchBird state machine valid next states enumeration working" << std::endl;
}

void test_scratchbird_framer()
{
    std::cout << "\n=== Test 3.3: ScratchBird Message Framer ===" << std::endl;

    ScratchBirdFramer framer;
    assert(!framer.needs_more_data());

    // Create test message data with header
    std::string payload = "CONNECT";
    std::uint32_t payload_size = static_cast<std::uint32_t>(payload.size());

    std::vector<std::uint8_t> message_data;
    // Add size header (little-endian)
    message_data.push_back(payload_size & 0xFF);
    message_data.push_back((payload_size >> 8) & 0xFF);
    message_data.push_back((payload_size >> 16) & 0xFF);
    message_data.push_back((payload_size >> 24) & 0xFF);

    // Add payload
    for (char c : payload) {
        message_data.push_back(static_cast<std::uint8_t>(c));
    }

    // Test complete message framing
    auto framed_messages = framer.frame_messages(message_data);
    assert(framed_messages.size() == 1);
    assert(framed_messages[0].size() == message_data.size());
    std::cout << "✓ ScratchBird message framing working correctly" << std::endl;

    // Test partial message handling
    framer.reset();
    std::vector<std::uint8_t> partial_data(message_data.begin(), message_data.begin() + 2);
    auto partial_messages = framer.frame_messages(partial_data);
    assert(partial_messages.empty());
    assert(framer.needs_more_data());
    std::cout << "✓ ScratchBird partial message handling working correctly" << std::endl;
}

//=============================================================================
// Test Category 4: Protocol Handler Integration Tests
//=============================================================================

void test_scratchbird_protocol_handler()
{
    std::cout << "\n=== Test 4.1: ScratchBird Protocol Handler ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("protocol_handler_test");

    // Create mock connection (we'll use null for testing basic functionality)
    ScratchBirdProtocolHandler handler;
    assert(!handler.is_initialized());
    assert(handler.get_protocol_type() == ProtocolType::ScratchBirdNative);

    // Test version support
    ProtocolVersion version = handler.get_supported_version();
    assert(version.type == ProtocolType::ScratchBirdNative);
    assert(version.major == 1);
    std::cout << "✓ ScratchBird protocol handler basic properties working" << std::endl;

    // For now, we can't fully test with real connections, but we can test basic structure
    assert(handler.requires_authentication());
    assert(!handler.is_authenticated());
    assert(!handler.has_outgoing_messages());
    std::cout << "✓ ScratchBird protocol handler authentication properties working" << std::endl;
}

void test_protocol_handler_manager()
{
    std::cout << "\n=== Test 4.2: Protocol Handler Manager ===" << std::endl;

    scratchbird::tests::TestDatabaseRAII test_db("protocol_manager_test");

    // Register ScratchBird protocol handler
    auto& factory = ProtocolHandlerFactory::get_instance();
    factory.register_handler(ProtocolType::ScratchBirdNative,
                             []() -> std::unique_ptr<ProtocolHandler> {
                                 return std::make_unique<ScratchBirdProtocolHandler>();
                             });

    // Create a test catalog manager
    CatalogManager catalog(test_db.path());

    // Create protocol handler manager (using nullptr for connection since we're testing manager
    // logic) In real usage, connection would be valid, but for testing basic manager functionality
    // this is OK
    ProtocolHandlerManager manager(nullptr, &catalog);
    assert(!manager.is_initialized());

    // Test initialization - should succeed even with null connection for basic initialization
    bool initialized = manager.initialize();
    assert(initialized);
    assert(manager.is_initialized());
    std::cout << "✓ Protocol handler manager initialization working" << std::endl;

    // Test basic state
    assert(manager.get_current_protocol() == ProtocolType::Unknown); // No protocol detected yet
    assert(!manager.has_pending_messages());
    assert(manager.get_messages_processed() == 0);
    assert(manager.get_protocol_errors() == 0);
    std::cout << "✓ Protocol handler manager basic state working correctly" << std::endl;

    // Test shutdown
    manager.shutdown();
    assert(!manager.is_initialized());
    std::cout << "✓ Protocol handler manager shutdown working correctly" << std::endl;
}

//=============================================================================
// Test Category 5: Message Processing Integration Tests
//=============================================================================

void test_message_processing_workflow()
{
    std::cout << "\n=== Test 5.1: Message Processing Workflow ===" << std::endl;

    // Test complete message processing workflow with mock data
    MessageQueue queue;
    CorrelationTracker tracker;

    // Create a series of related messages
    auto correlation_id = tracker.create_correlation_id();

    auto request = std::make_unique<ScratchBirdMessage>("REQUEST");
    request->correlation_id = correlation_id;
    request->set_payload_string("Test request payload");

    tracker.register_request(correlation_id, request->clone());
    queue.enqueue(std::move(request));

    // Process the request
    auto processed_request = queue.dequeue();
    assert(processed_request != nullptr);
    assert(processed_request->correlation_id == correlation_id);
    std::cout << "✓ Message workflow correlation tracking working" << std::endl;

    // Simulate response
    auto response = std::make_unique<ScratchBirdMessage>("RESPONSE");
    response->correlation_id = correlation_id;
    response->set_payload_string("Test response payload");

    // Complete the correlation
    auto original_request = tracker.get_request(correlation_id);
    assert(original_request != nullptr);
    assert(!tracker.has_pending_request(correlation_id));
    std::cout << "✓ Complete message processing workflow working correctly" << std::endl;
}

void test_concurrent_message_processing()
{
    std::cout << "\n=== Test 5.2: Concurrent Message Processing ===" << std::endl;

    MessageQueue queue;
    std::atomic<int> messages_processed{0};
    const int num_messages = 100;

    // Producer thread - enqueue messages
    std::thread producer([&queue, num_messages]() {
        for (int i = 0; i < num_messages; ++i) {
            auto message = std::make_unique<ScratchBirdMessage>("MSG_" + std::to_string(i));
            message->message_id = i;
            queue.enqueue(std::move(message));
        }
    });

    // Consumer thread - dequeue messages
    std::thread consumer([&queue, &messages_processed, num_messages]() {
        while (messages_processed.load() < num_messages) {
            auto message = queue.dequeue();
            if (message) {
                messages_processed++;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });

    producer.join();
    consumer.join();

    assert(messages_processed.load() == num_messages);
    assert(queue.empty());
    std::cout << "✓ Concurrent message processing working correctly" << std::endl;
}

//=============================================================================
// Test Runner and Main Function
//=============================================================================

void run_protocol_infrastructure_tests()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 11.1.2: PROTOCOL INFRASTRUCTURE TESTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_protocol_version_compatibility();
    test_message_queue_operations();
    test_correlation_tracker();

    std::cout << "\n✅ All Protocol Infrastructure Tests PASSED" << std::endl;
}

void run_protocol_factory_tests()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 11.1.2: PROTOCOL HANDLER FACTORY TESTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_protocol_handler_factory();

    std::cout << "\n✅ All Protocol Handler Factory Tests PASSED" << std::endl;
}

void run_scratchbird_protocol_tests()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 11.1.2: SCRATCHBIRD PROTOCOL TESTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_scratchbird_message();
    test_scratchbird_state_machine();
    test_scratchbird_framer();

    std::cout << "\n✅ All ScratchBird Protocol Tests PASSED" << std::endl;
}

void run_protocol_handler_integration_tests()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 11.1.2: PROTOCOL HANDLER INTEGRATION TESTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_scratchbird_protocol_handler();
    test_protocol_handler_manager();

    std::cout << "\n✅ All Protocol Handler Integration Tests PASSED" << std::endl;
}

void run_message_processing_tests()
{
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "PHASE 11.1.2: MESSAGE PROCESSING TESTS" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_message_processing_workflow();
    test_concurrent_message_processing();

    std::cout << "\n✅ All Message Processing Tests PASSED" << std::endl;
}

int main()
{
    std::cout << std::string(80, '=') << std::endl;
    std::cout << "ScratchBird Phase 11.1.2: Protocol Handler Framework Tests" << std::endl;
    std::cout << "Generic Protocol Handler Interface and Message Processing" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    try {
        // Run all test categories
        run_protocol_infrastructure_tests();
        run_protocol_factory_tests();
        run_scratchbird_protocol_tests();
        run_protocol_handler_integration_tests();
        run_message_processing_tests();

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "🎉 ALL PROTOCOL HANDLER FRAMEWORK TESTS PASSED! 🎉" << std::endl;
        std::cout << "Phase 11.1.2: Protocol Handler Framework - COMPLETE" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILURE: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ UNKNOWN TEST FAILURE" << std::endl;
        return 1;
    }
}
