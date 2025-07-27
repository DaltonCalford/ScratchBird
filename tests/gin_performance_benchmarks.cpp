/*
 *	PROGRAM:	ScratchBird Database Engine
 *	MODULE:		gin_performance_benchmarks.cpp
 *	DESCRIPTION:	Performance benchmarks for GIN full-text search operations
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.23 - ScratchBird GIN Index Performance Benchmarks
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <map>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <memory>

using namespace std;
using namespace std::chrono;

//----------------------------
// Benchmark Configuration
//----------------------------
struct BenchmarkConfig
{
	// Dataset configuration
	size_t small_dataset_size;		// 1,000 documents
	size_t medium_dataset_size;		// 100,000 documents  
	size_t large_dataset_size;		// 1,000,000 documents
	size_t xlarge_dataset_size;		// 10,000,000 documents
	
	// Document configuration
	size_t min_document_size;		// Minimum words per document
	size_t max_document_size;		// Maximum words per document
	size_t vocabulary_size;			// Total unique words in corpus
	
	// Query configuration
	size_t single_term_queries;		// Number of single-term queries
	size_t multi_term_queries;		// Number of multi-term queries
	size_t phrase_queries;			// Number of phrase queries
	size_t boolean_queries;			// Number of boolean queries
	
	// Benchmark configuration
	size_t warmup_iterations;		// Warmup runs before measurement
	size_t measurement_iterations;	// Actual measurement runs
	bool detailed_output;			// Enable detailed timing breakdown
	bool csv_output;				// Output results in CSV format
	string output_filename;			// Output file for results
	
	BenchmarkConfig()
		: small_dataset_size(1000), medium_dataset_size(100000),
		  large_dataset_size(1000000), xlarge_dataset_size(10000000),
		  min_document_size(10), max_document_size(1000),
		  vocabulary_size(50000), single_term_queries(1000),
		  multi_term_queries(500), phrase_queries(200),
		  boolean_queries(300), warmup_iterations(5),
		  measurement_iterations(10), detailed_output(false),
		  csv_output(false), output_filename("gin_benchmark_results.txt")
	{
	}
};

//----------------------------
// Performance Metrics
//----------------------------
struct PerformanceMetrics
{
	// Timing metrics (in microseconds)
	double min_time;
	double max_time;
	double avg_time;
	double median_time;
	double std_deviation;
	
	// Throughput metrics
	double queries_per_second;
	double documents_processed_per_second;
	double terms_processed_per_second;
	
	// Index metrics
	size_t total_terms_in_index;
	size_t average_posting_list_size;
	size_t index_size_bytes;
	double compression_ratio;
	
	// Query result metrics
	size_t total_results_returned;
	double average_results_per_query;
	double result_precision;
	double result_recall;
	
	PerformanceMetrics()
		: min_time(0.0), max_time(0.0), avg_time(0.0), median_time(0.0),
		  std_deviation(0.0), queries_per_second(0.0),
		  documents_processed_per_second(0.0), terms_processed_per_second(0.0),
		  total_terms_in_index(0), average_posting_list_size(0),
		  index_size_bytes(0), compression_ratio(0.0),
		  total_results_returned(0), average_results_per_query(0.0),
		  result_precision(0.0), result_recall(0.0)
	{
	}
};

//----------------------------
// Document Generator
//----------------------------
class DocumentGenerator
{
public:
	DocumentGenerator(size_t vocab_size, size_t min_words, size_t max_words)
		: m_vocabulary_size(vocab_size), m_min_words(min_words), 
		  m_max_words(max_words), m_rng(random_device{}())
	{
		generateVocabulary();
	}
	
	string generateDocument()
	{
		uniform_int_distribution<size_t> word_count_dist(m_min_words, m_max_words);
		uniform_int_distribution<size_t> word_index_dist(0, m_vocabulary.size() - 1);
		
		size_t word_count = word_count_dist(m_rng);
		string document;
		
		for (size_t i = 0; i < word_count; ++i) {
			if (i > 0) document += " ";
			document += m_vocabulary[word_index_dist(m_rng)];
		}
		
		return document;
	}
	
	vector<string> generateDocumentSet(size_t count)
	{
		vector<string> documents;
		documents.reserve(count);
		
		for (size_t i = 0; i < count; ++i) {
			documents.push_back(generateDocument());
		}
		
		return documents;
	}
	
	const vector<string>& getVocabulary() const { return m_vocabulary; }

private:
	size_t m_vocabulary_size;
	size_t m_min_words;
	size_t m_max_words;
	mutable mt19937 m_rng;
	vector<string> m_vocabulary;
	
	void generateVocabulary()
	{
		// Generate realistic vocabulary with varying word lengths
		m_vocabulary.reserve(m_vocabulary_size);
		
		for (size_t i = 0; i < m_vocabulary_size; ++i) {
			m_vocabulary.push_back(generateRandomWord());
		}
		
		// Add some common words that appear frequently
		vector<string> common_words = {
			"the", "and", "or", "but", "in", "on", "at", "to", "for", "of",
			"with", "by", "from", "about", "into", "through", "during", "before",
			"after", "above", "below", "up", "down", "out", "off", "over", "under",
			"again", "further", "then", "once", "here", "there", "when", "where",
			"why", "how", "all", "any", "both", "each", "few", "more", "most",
			"other", "some", "such", "no", "nor", "not", "only", "own", "same",
			"so", "than", "too", "very", "can", "will", "just", "should", "now"
		};
		
		// Replace some random words with common words
		uniform_int_distribution<size_t> replace_dist(0, m_vocabulary.size() - 1);
		for (const auto& word : common_words) {
			if (m_vocabulary.size() < m_vocabulary_size) {
				m_vocabulary.push_back(word);
			} else {
				m_vocabulary[replace_dist(m_rng)] = word;
			}
		}
	}
	
	string generateRandomWord()
	{
		uniform_int_distribution<size_t> length_dist(3, 12);
		uniform_int_distribution<char> char_dist('a', 'z');
		
		size_t length = length_dist(m_rng);
		string word;
		word.reserve(length);
		
		for (size_t i = 0; i < length; ++i) {
			word += char_dist(m_rng);
		}
		
		return word;
	}
};

//----------------------------
// Query Generator
//----------------------------
class QueryGenerator
{
public:
	QueryGenerator(const vector<string>& vocabulary)
		: m_vocabulary(vocabulary), m_rng(random_device{}())
	{
	}
	
	// Generate single term queries
	vector<string> generateSingleTermQueries(size_t count)
	{
		vector<string> queries;
		queries.reserve(count);
		uniform_int_distribution<size_t> word_dist(0, m_vocabulary.size() - 1);
		
		for (size_t i = 0; i < count; ++i) {
			queries.push_back(m_vocabulary[word_dist(m_rng)]);
		}
		
		return queries;
	}
	
	// Generate multi-term queries (2-5 terms)
	vector<string> generateMultiTermQueries(size_t count)
	{
		vector<string> queries;
		queries.reserve(count);
		uniform_int_distribution<size_t> word_dist(0, m_vocabulary.size() - 1);
		uniform_int_distribution<size_t> term_count_dist(2, 5);
		
		for (size_t i = 0; i < count; ++i) {
			size_t term_count = term_count_dist(m_rng);
			string query;
			
			for (size_t j = 0; j < term_count; ++j) {
				if (j > 0) query += " ";
				query += m_vocabulary[word_dist(m_rng)];
			}
			
			queries.push_back(query);
		}
		
		return queries;
	}
	
	// Generate phrase queries (quoted terms)
	vector<string> generatePhraseQueries(size_t count)
	{
		vector<string> queries;
		queries.reserve(count);
		uniform_int_distribution<size_t> word_dist(0, m_vocabulary.size() - 1);
		uniform_int_distribution<size_t> phrase_length_dist(2, 4);
		
		for (size_t i = 0; i < count; ++i) {
			size_t phrase_length = phrase_length_dist(m_rng);
			string query = "\"";
			
			for (size_t j = 0; j < phrase_length; ++j) {
				if (j > 0) query += " ";
				query += m_vocabulary[word_dist(m_rng)];
			}
			
			query += "\"";
			queries.push_back(query);
		}
		
		return queries;
	}
	
	// Generate boolean queries (AND, OR, NOT operators)
	vector<string> generateBooleanQueries(size_t count)
	{
		vector<string> queries;
		queries.reserve(count);
		uniform_int_distribution<size_t> word_dist(0, m_vocabulary.size() - 1);
		uniform_int_distribution<int> operator_dist(0, 2);
		
		vector<string> operators = {"AND", "OR", "NOT"};
		
		for (size_t i = 0; i < count; ++i) {
			string query;
			
			// Generate 2-4 terms with boolean operators
			size_t term_count = 2 + (i % 3); // 2-4 terms
			for (size_t j = 0; j < term_count; ++j) {
				if (j > 0) {
					query += " " + operators[operator_dist(m_rng)] + " ";
				}
				query += m_vocabulary[word_dist(m_rng)];
			}
			
			queries.push_back(query);
		}
		
		return queries;
	}

private:
	const vector<string>& m_vocabulary;
	mutable mt19937 m_rng;
};

//----------------------------
// Mock GIN Index Interface
//----------------------------
class MockGinIndex
{
public:
	MockGinIndex(const string& name) : m_name(name) {}
	
	// Simulate index creation time
	double createIndex(const vector<string>& documents)
	{
		auto start = high_resolution_clock::now();
		
		// Simulate indexing work
		m_document_count = documents.size();
		m_term_count = 0;
		
		// Simple tokenization and term counting simulation
		for (const auto& doc : documents) {
			vector<string> terms = tokenize(doc);
			for (const auto& term : terms) {
				m_term_frequencies[term]++;
				m_term_count++;
			}
		}
		
		// Simulate compression and storage
		this_thread::sleep_for(microseconds(documents.size() / 100));
		
		auto end = high_resolution_clock::now();
		return duration_cast<microseconds>(end - start).count();
	}
	
	// Simulate query execution time
	pair<double, size_t> executeQuery(const string& query)
	{
		auto start = high_resolution_clock::now();
		
		// Simulate query processing
		vector<string> query_terms = tokenize(query);
		size_t result_count = 0;
		
		// Simple intersection simulation
		for (const auto& term : query_terms) {
			auto it = m_term_frequencies.find(term);
			if (it != m_term_frequencies.end()) {
				result_count += it->second / 10; // Simulate result size
			}
		}
		
		// Simulate query processing time based on complexity
		size_t processing_time = query_terms.size() * 10 + result_count;
		this_thread::sleep_for(microseconds(processing_time));
		
		auto end = high_resolution_clock::now();
		double query_time = duration_cast<microseconds>(end - start).count();
		
		return make_pair(query_time, result_count);
	}
	
	// Get index statistics
	map<string, size_t> getStatistics() const
	{
		return {
			{"document_count", m_document_count},
			{"term_count", m_term_count},
			{"unique_terms", m_term_frequencies.size()},
			{"index_size", m_term_frequencies.size() * 100} // Estimated size
		};
	}

private:
	string m_name;
	size_t m_document_count = 0;
	size_t m_term_count = 0;
	map<string, size_t> m_term_frequencies;
	
	vector<string> tokenize(const string& text)
	{
		vector<string> tokens;
		stringstream ss(text);
		string token;
		
		while (ss >> token) {
			// Simple tokenization (remove punctuation, lowercase)
			string clean_token;
			for (char c : token) {
				if (isalnum(c)) {
					clean_token += tolower(c);
				}
			}
			if (!clean_token.empty()) {
				tokens.push_back(clean_token);
			}
		}
		
		return tokens;
	}
};

//----------------------------
// Benchmark Runner
//----------------------------
class GinBenchmarkRunner
{
public:
	GinBenchmarkRunner(const BenchmarkConfig& config)
		: m_config(config), m_doc_generator(config.vocabulary_size, 
										   config.min_document_size,
										   config.max_document_size)
	{
	}
	
	void runAllBenchmarks()
	{
		cout << "ScratchBird GIN Index Performance Benchmark Suite" << endl;
		cout << "=================================================" << endl << endl;
		
		// Run benchmarks for different dataset sizes
		runDatasetSizeBenchmarks();
		
		// Run query type benchmarks
		runQueryTypeBenchmarks();
		
		// Run scalability benchmarks
		runScalabilityBenchmarks();
		
		// Run comparison benchmarks
		runComparisonBenchmarks();
		
		// Generate final report
		generateFinalReport();
	}

private:
	BenchmarkConfig m_config;
	DocumentGenerator m_doc_generator;
	map<string, PerformanceMetrics> m_results;
	
	void runDatasetSizeBenchmarks()
	{
		cout << "Running Dataset Size Benchmarks..." << endl;
		cout << "===================================" << endl;
		
		vector<pair<string, size_t>> datasets = {
			{"Small (1K docs)", m_config.small_dataset_size},
			{"Medium (100K docs)", m_config.medium_dataset_size},
			{"Large (1M docs)", m_config.large_dataset_size}
		};
		
		for (const auto& dataset : datasets) {
			cout << "Testing " << dataset.first << "..." << endl;
			
			// Generate documents
			auto documents = m_doc_generator.generateDocumentSet(dataset.second);
			
			// Create index
			MockGinIndex index("test_index");
			double index_creation_time = index.createIndex(documents);
			
			// Generate queries
			QueryGenerator query_gen(m_doc_generator.getVocabulary());
			auto queries = query_gen.generateSingleTermQueries(m_config.single_term_queries);
			
			// Run benchmark
			PerformanceMetrics metrics = runQueryBenchmark(index, queries, dataset.first);
			metrics.index_size_bytes = index.getStatistics()["index_size"];
			metrics.total_terms_in_index = index.getStatistics()["unique_terms"];
			
			m_results[dataset.first] = metrics;
			
			printBenchmarkResults(dataset.first, metrics);
			cout << endl;
		}
	}
	
	void runQueryTypeBenchmarks()
	{
		cout << "Running Query Type Benchmarks..." << endl;
		cout << "================================" << endl;
		
		// Use medium dataset for query type testing
		auto documents = m_doc_generator.generateDocumentSet(m_config.medium_dataset_size);
		MockGinIndex index("query_type_test");
		index.createIndex(documents);
		
		QueryGenerator query_gen(m_doc_generator.getVocabulary());
		
		// Test different query types
		vector<pair<string, vector<string>>> query_types = {
			{"Single Term", query_gen.generateSingleTermQueries(m_config.single_term_queries)},
			{"Multi Term", query_gen.generateMultiTermQueries(m_config.multi_term_queries)},
			{"Phrase", query_gen.generatePhraseQueries(m_config.phrase_queries)},
			{"Boolean", query_gen.generateBooleanQueries(m_config.boolean_queries)}
		};
		
		for (const auto& query_type : query_types) {
			cout << "Testing " << query_type.first << " queries..." << endl;
			
			PerformanceMetrics metrics = runQueryBenchmark(index, query_type.second, 
														   query_type.first);
			m_results[query_type.first + " Queries"] = metrics;
			
			printBenchmarkResults(query_type.first + " Queries", metrics);
			cout << endl;
		}
	}
	
	void runScalabilityBenchmarks()
	{
		cout << "Running Scalability Benchmarks..." << endl;
		cout << "==================================" << endl;
		
		// Test with increasing query loads
		vector<size_t> query_loads = {100, 500, 1000, 5000, 10000};
		auto documents = m_doc_generator.generateDocumentSet(m_config.medium_dataset_size);
		MockGinIndex index("scalability_test");
		index.createIndex(documents);
		
		QueryGenerator query_gen(m_doc_generator.getVocabulary());
		
		for (size_t load : query_loads) {
			cout << "Testing with " << load << " queries..." << endl;
			
			auto queries = query_gen.generateSingleTermQueries(load);
			PerformanceMetrics metrics = runQueryBenchmark(index, queries, 
														   "Load_" + to_string(load));
			
			m_results["Scalability_" + to_string(load)] = metrics;
			
			cout << "  Queries/sec: " << fixed << setprecision(2) 
				 << metrics.queries_per_second << endl;
			cout << "  Avg time: " << fixed << setprecision(3) 
				 << metrics.avg_time / 1000.0 << " ms" << endl;
		}
		cout << endl;
	}
	
	void runComparisonBenchmarks()
	{
		cout << "Running Comparison Benchmarks..." << endl;
		cout << "================================" << endl;
		
		// Compare different scenarios
		auto documents = m_doc_generator.generateDocumentSet(m_config.medium_dataset_size);
		
		// Different index configurations (simulated)
		vector<string> configurations = {
			"Standard GIN",
			"Compressed GIN", 
			"Fast GIN",
			"Memory GIN"
		};
		
		QueryGenerator query_gen(m_doc_generator.getVocabulary());
		auto queries = query_gen.generateMultiTermQueries(1000);
		
		for (const auto& config : configurations) {
			cout << "Testing " << config << " configuration..." << endl;
			
			MockGinIndex index(config);
			index.createIndex(documents);
			
			PerformanceMetrics metrics = runQueryBenchmark(index, queries, config);
			m_results[config] = metrics;
			
			cout << "  Avg query time: " << fixed << setprecision(3) 
				 << metrics.avg_time / 1000.0 << " ms" << endl;
		}
		cout << endl;
	}
	
	PerformanceMetrics runQueryBenchmark(MockGinIndex& index, 
										 const vector<string>& queries,
										 const string& benchmark_name)
	{
		PerformanceMetrics metrics;
		vector<double> query_times;
		query_times.reserve(queries.size());
		
		// Warmup runs
		for (size_t i = 0; i < m_config.warmup_iterations && i < queries.size(); ++i) {
			index.executeQuery(queries[i]);
		}
		
		// Measurement runs
		auto benchmark_start = high_resolution_clock::now();
		size_t total_results = 0;
		
		for (const auto& query : queries) {
			auto result = index.executeQuery(query);
			query_times.push_back(result.first);
			total_results += result.second;
		}
		
		auto benchmark_end = high_resolution_clock::now();
		double total_benchmark_time = duration_cast<microseconds>(benchmark_end - benchmark_start).count();
		
		// Calculate statistics
		sort(query_times.begin(), query_times.end());
		
		metrics.min_time = query_times.front();
		metrics.max_time = query_times.back();
		metrics.avg_time = accumulate(query_times.begin(), query_times.end(), 0.0) / query_times.size();
		metrics.median_time = query_times[query_times.size() / 2];
		
		// Calculate standard deviation
		double variance = 0.0;
		for (double time : query_times) {
			variance += (time - metrics.avg_time) * (time - metrics.avg_time);
		}
		metrics.std_deviation = sqrt(variance / query_times.size());
		
		// Calculate throughput
		metrics.queries_per_second = queries.size() * 1000000.0 / total_benchmark_time;
		metrics.total_results_returned = total_results;
		metrics.average_results_per_query = static_cast<double>(total_results) / queries.size();
		
		return metrics;
	}
	
	void printBenchmarkResults(const string& benchmark_name, const PerformanceMetrics& metrics)
	{
		cout << "Results for " << benchmark_name << ":" << endl;
		cout << "  Min query time: " << fixed << setprecision(3) << metrics.min_time / 1000.0 << " ms" << endl;
		cout << "  Max query time: " << fixed << setprecision(3) << metrics.max_time / 1000.0 << " ms" << endl;
		cout << "  Avg query time: " << fixed << setprecision(3) << metrics.avg_time / 1000.0 << " ms" << endl;
		cout << "  Median time: " << fixed << setprecision(3) << metrics.median_time / 1000.0 << " ms" << endl;
		cout << "  Std deviation: " << fixed << setprecision(3) << metrics.std_deviation / 1000.0 << " ms" << endl;
		cout << "  Queries/sec: " << fixed << setprecision(2) << metrics.queries_per_second << endl;
		cout << "  Avg results: " << fixed << setprecision(1) << metrics.average_results_per_query << endl;
		
		if (metrics.total_terms_in_index > 0) {
			cout << "  Index terms: " << metrics.total_terms_in_index << endl;
			cout << "  Index size: " << metrics.index_size_bytes / 1024 << " KB" << endl;
		}
	}
	
	void generateFinalReport()
	{
		cout << "Final Performance Report" << endl;
		cout << "========================" << endl << endl;
		
		// Summary table
		cout << left << setw(25) << "Benchmark" 
			 << right << setw(12) << "Avg (ms)" 
			 << right << setw(12) << "QPS" 
			 << right << setw(12) << "Results" << endl;
		cout << string(61, '-') << endl;
		
		for (const auto& result : m_results) {
			cout << left << setw(25) << result.first
				 << right << setw(12) << fixed << setprecision(3) << result.second.avg_time / 1000.0
				 << right << setw(12) << fixed << setprecision(1) << result.second.queries_per_second
				 << right << setw(12) << fixed << setprecision(1) << result.second.average_results_per_query
				 << endl;
		}
		
		cout << endl;
		
		// Key findings
		cout << "Key Performance Findings:" << endl;
		cout << "========================" << endl;
		
		// Find best performing configurations
		double best_qps = 0.0;
		string best_qps_config;
		double best_avg_time = numeric_limits<double>::max();
		string best_time_config;
		
		for (const auto& result : m_results) {
			if (result.second.queries_per_second > best_qps) {
				best_qps = result.second.queries_per_second;
				best_qps_config = result.first;
			}
			if (result.second.avg_time < best_avg_time) {
				best_avg_time = result.second.avg_time;
				best_time_config = result.first;
			}
		}
		
		cout << "• Highest throughput: " << best_qps_config << " (" 
			 << fixed << setprecision(1) << best_qps << " QPS)" << endl;
		cout << "• Fastest queries: " << best_time_config << " (" 
			 << fixed << setprecision(3) << best_avg_time / 1000.0 << " ms avg)" << endl;
		
		// Write results to file if requested
		if (m_config.csv_output) {
			writeCSVReport();
		}
	}
	
	void writeCSVReport()
	{
		ofstream csv_file(m_config.output_filename);
		if (!csv_file.is_open()) {
			cerr << "Failed to open output file: " << m_config.output_filename << endl;
			return;
		}
		
		// CSV header
		csv_file << "Benchmark,Min_Time_ms,Max_Time_ms,Avg_Time_ms,Median_Time_ms,"
				 << "Std_Dev_ms,Queries_Per_Second,Avg_Results,Index_Terms,Index_Size_KB" << endl;
		
		// CSV data
		for (const auto& result : m_results) {
			csv_file << result.first << ","
					 << fixed << setprecision(3) << result.second.min_time / 1000.0 << ","
					 << fixed << setprecision(3) << result.second.max_time / 1000.0 << ","
					 << fixed << setprecision(3) << result.second.avg_time / 1000.0 << ","
					 << fixed << setprecision(3) << result.second.median_time / 1000.0 << ","
					 << fixed << setprecision(3) << result.second.std_deviation / 1000.0 << ","
					 << fixed << setprecision(2) << result.second.queries_per_second << ","
					 << fixed << setprecision(1) << result.second.average_results_per_query << ","
					 << result.second.total_terms_in_index << ","
					 << result.second.index_size_bytes / 1024 << endl;
		}
		
		csv_file.close();
		cout << "CSV report written to: " << m_config.output_filename << endl;
	}
};

//----------------------------
// Main Program
//----------------------------
int main(int argc, char* argv[])
{
	cout << "ScratchBird GIN Index Performance Benchmark Suite" << endl;
	cout << "Version: 1.0 - Full-Text Search Performance Analysis" << endl;
	cout << "====================================================" << endl << endl;
	
	// Parse command line arguments
	BenchmarkConfig config;
	
	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		
		if (arg == "--help" || arg == "-h") {
			cout << "Usage: gin_performance_benchmarks [options]" << endl;
			cout << "Options:" << endl;
			cout << "  --small-size N      Small dataset size (default: 1000)" << endl;
			cout << "  --medium-size N     Medium dataset size (default: 100000)" << endl;
			cout << "  --large-size N      Large dataset size (default: 1000000)" << endl;
			cout << "  --queries N         Number of queries per test (default: 1000)" << endl;
			cout << "  --iterations N      Measurement iterations (default: 10)" << endl;
			cout << "  --detailed          Enable detailed output" << endl;
			cout << "  --csv [filename]    Output CSV results" << endl;
			cout << "  --help              Show this help message" << endl;
			return 0;
		}
		else if (arg == "--detailed") {
			config.detailed_output = true;
		}
		else if (arg == "--csv") {
			config.csv_output = true;
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				config.output_filename = argv[++i];
			} else {
				config.output_filename = "gin_benchmark_results.csv";
			}
		}
		else if (arg == "--small-size" && i + 1 < argc) {
			config.small_dataset_size = stoul(argv[++i]);
		}
		else if (arg == "--medium-size" && i + 1 < argc) {
			config.medium_dataset_size = stoul(argv[++i]);
		}
		else if (arg == "--large-size" && i + 1 < argc) {
			config.large_dataset_size = stoul(argv[++i]);
		}
		else if (arg == "--queries" && i + 1 < argc) {
			size_t query_count = stoul(argv[++i]);
			config.single_term_queries = query_count;
			config.multi_term_queries = query_count / 2;
			config.phrase_queries = query_count / 5;
			config.boolean_queries = query_count / 3;
		}
		else if (arg == "--iterations" && i + 1 < argc) {
			config.measurement_iterations = stoul(argv[++i]);
		}
	}
	
	// Run benchmarks
	try {
		GinBenchmarkRunner runner(config);
		runner.runAllBenchmarks();
		
		cout << endl << "Benchmark suite completed successfully!" << endl;
		return 0;
	}
	catch (const exception& e) {
		cerr << "Benchmark failed with error: " << e.what() << endl;
		return 1;
	}
}