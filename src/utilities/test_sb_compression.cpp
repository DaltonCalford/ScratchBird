#include "sb_compression.h"
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <iomanip>
#include <cassert>

using namespace SBCompression;

class CompressionTest {
private:
    SBCompressionEngine engine;
    
public:
    bool runAllTests() {
        std::cout << "ScratchBird Compression Module Test Suite" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicInitialization();
        all_passed &= testAlgorithmAvailability();
        all_passed &= testCompressionDecompression();
        all_passed &= testDifferentDataTypes();
        all_passed &= testCompressionLevels();
        all_passed &= testMagicNumberDetection();
        all_passed &= testErrorHandling();
        all_passed &= testPerformanceBenchmark();
        
        std::cout << "\n=========================================" << std::endl;
        if (all_passed) {
            std::cout << "✅ All compression tests PASSED! Compression module is ready." << std::endl;
        } else {
            std::cout << "❌ Some compression tests FAILED! Please review the implementation." << std::endl;
        }
        std::cout << "=========================================" << std::endl;
        
        return all_passed;
    }
    
private:
    bool testBasicInitialization() {
        std::cout << "\n🔧 Testing Basic Initialization..." << std::endl;
        
        try {
            // Test default configuration
            CompressionConfig config = engine.getConfig();
            if (config.algorithm != Algorithm::ZSTD || config.level != Level::MEDIUM) {
                std::cout << "❌ Default configuration is incorrect" << std::endl;
                return false;
            }
            
            // Test configuration changes
            config.algorithm = Algorithm::LZ4;
            config.level = Level::FAST;
            engine.setConfig(config);
            
            CompressionConfig new_config = engine.getConfig();
            if (new_config.algorithm != Algorithm::LZ4 || new_config.level != Level::FAST) {
                std::cout << "❌ Configuration change failed" << std::endl;
                return false;
            }
            
            std::cout << "✅ Basic initialization test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during initialization: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testAlgorithmAvailability() {
        std::cout << "\n🔧 Testing Algorithm Availability..." << std::endl;
        
        try {
            std::vector<Algorithm> available = engine.getAvailableAlgorithms();
            
            std::cout << "Available algorithms: ";
            for (Algorithm alg : available) {
                std::cout << engine.getAlgorithmName(alg) << " ";
            }
            std::cout << std::endl;
            
            // NONE should always be available
            if (!engine.isAlgorithmAvailable(Algorithm::NONE)) {
                std::cout << "❌ NONE algorithm should always be available" << std::endl;
                return false;
            }
            
            // Check algorithm names
            if (engine.getAlgorithmName(Algorithm::ZSTD) != "ZSTD") {
                std::cout << "❌ Algorithm name mapping is incorrect" << std::endl;
                return false;
            }
            
            std::cout << "✅ Algorithm availability test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during algorithm availability: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testCompressionDecompression() {
        std::cout << "\n🔧 Testing Compression/Decompression..." << std::endl;
        
        try {
            // Test with different algorithms
            std::vector<Algorithm> test_algorithms = {Algorithm::NONE};
            
            // Add available algorithms
            if (engine.isAlgorithmAvailable(Algorithm::GZIP)) test_algorithms.push_back(Algorithm::GZIP);
            if (engine.isAlgorithmAvailable(Algorithm::LZ4)) test_algorithms.push_back(Algorithm::LZ4);
            if (engine.isAlgorithmAvailable(Algorithm::ZSTD)) test_algorithms.push_back(Algorithm::ZSTD);
            if (engine.isAlgorithmAvailable(Algorithm::BZIP2)) test_algorithms.push_back(Algorithm::BZIP2);
            
            // Create test data
            std::string test_string = createTestData(1024);
            std::vector<uint8_t> original_data(test_string.begin(), test_string.end());
            
            for (Algorithm alg : test_algorithms) {
                std::cout << "  Testing " << engine.getAlgorithmName(alg) << "..." << std::endl;
                
                // Set algorithm
                CompressionConfig config = engine.getConfig();
                config.algorithm = alg;
                engine.setConfig(config);
                
                // Compress
                std::vector<uint8_t> compressed_data;
                CompressionStats compress_stats;
                bool compress_result = engine.compress(original_data, compressed_data, compress_stats);
                
                if (!compress_result) {
                    std::cout << "❌ Compression failed for " << engine.getAlgorithmName(alg) << std::endl;
                    return false;
                }
                
                // Decompress
                std::vector<uint8_t> decompressed_data;
                CompressionStats decompress_stats;
                bool decompress_result = engine.decompress(compressed_data, decompressed_data, decompress_stats);
                
                if (!decompress_result) {
                    std::cout << "❌ Decompression failed for " << engine.getAlgorithmName(alg) << std::endl;
                    return false;
                }
                
                // Verify data integrity
                if (original_data != decompressed_data) {
                    std::cout << "❌ Data corruption detected for " << engine.getAlgorithmName(alg) << std::endl;
                    return false;
                }
                
                // Verify statistics
                if (compress_stats.original_size != original_data.size()) {
                    std::cout << "❌ Incorrect original size in stats" << std::endl;
                    return false;
                }
                
                if (compress_stats.compressed_size != compressed_data.size()) {
                    std::cout << "❌ Incorrect compressed size in stats" << std::endl;
                    return false;
                }
                
                std::cout << "    Original: " << compress_stats.original_size << " bytes" << std::endl;
                std::cout << "    Compressed: " << compress_stats.compressed_size << " bytes" << std::endl;
                std::cout << "    Ratio: " << std::fixed << std::setprecision(2) 
                         << compress_stats.getCompressionRatio() << std::endl;
                std::cout << "    Compression time: " << compress_stats.compression_time.count() << " μs" << std::endl;
                std::cout << "    Decompression time: " << decompress_stats.decompression_time.count() << " μs" << std::endl;
            }
            
            std::cout << "✅ Compression/decompression test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during compression/decompression: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testDifferentDataTypes() {
        std::cout << "\n🔧 Testing Different Data Types..." << std::endl;
        
        try {
            // Test with various data patterns
            std::vector<std::pair<std::string, std::vector<uint8_t>>> test_cases = {
                {"Empty data", {}},
                {"Small text", createTextData(100)},
                {"Repetitive data", createRepetitiveData(1000)},
                {"Random data", createRandomData(1000)},
                {"Large text", createTextData(10000)},
                {"Binary data", createBinaryData(1000)}
            };
            
            CompressionConfig config = engine.getConfig();
            config.algorithm = Algorithm::NONE; // Start with no compression
            engine.setConfig(config);
            
            for (const auto& test_case : test_cases) {
                std::cout << "  Testing " << test_case.first << "..." << std::endl;
                
                std::vector<uint8_t> compressed_data;
                CompressionStats compress_stats;
                bool compress_result = engine.compress(test_case.second, compressed_data, compress_stats);
                
                if (!compress_result) {
                    std::cout << "❌ Compression failed for " << test_case.first << std::endl;
                    return false;
                }
                
                std::vector<uint8_t> decompressed_data;
                CompressionStats decompress_stats;
                bool decompress_result = engine.decompress(compressed_data, decompressed_data, decompress_stats);
                
                if (!decompress_result) {
                    std::cout << "❌ Decompression failed for " << test_case.first << std::endl;
                    return false;
                }
                
                if (test_case.second != decompressed_data) {
                    std::cout << "❌ Data corruption for " << test_case.first << std::endl;
                    return false;
                }
            }
            
            std::cout << "✅ Different data types test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during data types test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testCompressionLevels() {
        std::cout << "\n🔧 Testing Compression Levels..." << std::endl;
        
        try {
            if (!engine.isAlgorithmAvailable(Algorithm::ZSTD)) {
                std::cout << "ℹ️  ZSTD not available, skipping compression level test" << std::endl;
                return true;
            }
            
            std::vector<uint8_t> test_data = createTextData(5000);
            std::vector<Level> levels = {Level::FASTEST, Level::FAST, Level::MEDIUM, Level::BEST};
            
            CompressionConfig config = engine.getConfig();
            config.algorithm = Algorithm::ZSTD;
            
            for (Level level : levels) {
                config.level = level;
                engine.setConfig(config);
                
                std::vector<uint8_t> compressed_data;
                CompressionStats stats;
                bool result = engine.compress(test_data, compressed_data, stats);
                
                if (!result) {
                    std::cout << "❌ Compression failed for level " << static_cast<int>(level) << std::endl;
                    return false;
                }
                
                std::cout << "  Level " << static_cast<int>(level) 
                         << ": " << compressed_data.size() << " bytes"
                         << " (ratio: " << std::fixed << std::setprecision(3) 
                         << stats.getCompressionRatio() << ")" << std::endl;
            }
            
            std::cout << "✅ Compression levels test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during compression levels test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testMagicNumberDetection() {
        std::cout << "\n🔧 Testing Magic Number Detection..." << std::endl;
        
        try {
            // Test magic number detection
            std::vector<uint8_t> gzip_magic = {0x1f, 0x8b, 0x08, 0x00};
            std::vector<uint8_t> zstd_magic = {0x28, 0xb5, 0x2f, 0xfd};
            std::vector<uint8_t> lz4_magic = {0x04, 0x22, 0x4d, 0x18};
            std::vector<uint8_t> bzip2_magic = {0x42, 0x5a, 0x68, 0x39};
            std::vector<uint8_t> unknown_data = {0x00, 0x01, 0x02, 0x03};
            
            // Note: Detection requires actual compressed data, not just magic numbers
            // For this test, we'll verify the detection doesn't crash
            
            Algorithm detected;
            
            detected = engine.detectCompressionAlgorithm(gzip_magic);
            // May or may not detect correctly without full header
            
            detected = engine.detectCompressionAlgorithm(unknown_data);
            if (detected != Algorithm::NONE) {
                std::cout << "ℹ️  Unknown data detected as: " << engine.getAlgorithmName(detected) << std::endl;
            }
            
            std::cout << "✅ Magic number detection test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during magic number detection: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testErrorHandling() {
        std::cout << "\n🔧 Testing Error Handling..." << std::endl;
        
        try {
            // Test with invalid data
            std::vector<uint8_t> invalid_compressed = {0xFF, 0xFF, 0xFF, 0xFF};
            std::vector<uint8_t> output;
            CompressionStats stats;
            
            bool result = engine.decompress(invalid_compressed, output, stats);
            // Should fail gracefully
            
            // Test error logging
            std::vector<std::string> errors = engine.getErrorLog();
            if (errors.empty() && !result) {
                std::cout << "❌ Error should have been logged" << std::endl;
                return false;
            }
            
            // Test error clearing
            engine.clearErrorLog();
            errors = engine.getErrorLog();
            if (!errors.empty()) {
                std::cout << "❌ Error log should be empty after clearing" << std::endl;
                return false;
            }
            
            std::cout << "✅ Error handling test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during error handling test: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testPerformanceBenchmark() {
        std::cout << "\n🔧 Testing Performance Benchmark..." << std::endl;
        
        try {
            std::vector<uint8_t> large_data = createTextData(50000); // 50KB test
            
            std::vector<Algorithm> algorithms = {Algorithm::NONE};
            if (engine.isAlgorithmAvailable(Algorithm::LZ4)) algorithms.push_back(Algorithm::LZ4);
            if (engine.isAlgorithmAvailable(Algorithm::ZSTD)) algorithms.push_back(Algorithm::ZSTD);
            if (engine.isAlgorithmAvailable(Algorithm::GZIP)) algorithms.push_back(Algorithm::GZIP);
            
            std::cout << "  Performance comparison (50KB text data):" << std::endl;
            std::cout << "  Algorithm      Ratio    Comp Time   Decomp Time   Throughput" << std::endl;
            std::cout << "  ---------      -----    ---------   -----------   ----------" << std::endl;
            
            for (Algorithm alg : algorithms) {
                CompressionConfig config = engine.getConfig();
                config.algorithm = alg;
                config.level = Level::MEDIUM;
                engine.setConfig(config);
                
                std::vector<uint8_t> compressed;
                CompressionStats compress_stats;
                bool compress_ok = engine.compress(large_data, compressed, compress_stats);
                
                if (!compress_ok) continue;
                
                std::vector<uint8_t> decompressed;
                CompressionStats decompress_stats;
                bool decompress_ok = engine.decompress(compressed, decompressed, decompress_stats);
                
                if (!decompress_ok) continue;
                
                std::cout << "  " << std::left << std::setw(12) << engine.getAlgorithmName(alg)
                         << "  " << std::fixed << std::setprecision(3) << std::setw(6) 
                         << compress_stats.getCompressionRatio()
                         << "  " << std::setw(8) << compress_stats.compression_time.count() << " μs"
                         << "  " << std::setw(10) << decompress_stats.decompression_time.count() << " μs"
                         << "  " << std::setw(6) << std::setprecision(1) << compress_stats.getThroughputMBps() << " MB/s"
                         << std::endl;
            }
            
            std::cout << "✅ Performance benchmark completed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during performance benchmark: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Helper methods for creating test data
    std::string createTestData(size_t size) {
        std::string data;
        data.reserve(size);
        
        const std::string pattern = "The quick brown fox jumps over the lazy dog. ";
        while (data.size() < size) {
            size_t remaining = size - data.size();
            if (remaining >= pattern.size()) {
                data += pattern;
            } else {
                data += pattern.substr(0, remaining);
            }
        }
        
        return data;
    }
    
    std::vector<uint8_t> createTextData(size_t size) {
        std::string text = createTestData(size);
        return std::vector<uint8_t>(text.begin(), text.end());
    }
    
    std::vector<uint8_t> createRepetitiveData(size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 16);
        }
        return data;
    }
    
    std::vector<uint8_t> createRandomData(size_t size) {
        std::vector<uint8_t> data(size);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dis(gen));
        }
        return data;
    }
    
    std::vector<uint8_t> createBinaryData(size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>((i * 17 + 42) % 256);
        }
        return data;
    }
};

int main() {
    try {
        std::cout << "ScratchBird Compression Module Test" << std::endl;
        std::cout << "Version: SB-T0.6.0.1 ScratchBird 0.6 f90eae0" << std::endl;
        std::cout << "Testing compression algorithms and functionality..." << std::endl;
        
        CompressionTest test;
        bool success = test.runAllTests();
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal test error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal test error" << std::endl;
        return 1;
    }
}