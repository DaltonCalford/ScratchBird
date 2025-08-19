/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinTableScan.cpp  
 *	DESCRIPTION:	GIN index table scan record source implementation
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
 * 2025.07.22 - ScratchBird GIN Table Scan Implementation
 */

#include "scratchbird.h"
#include "GinTableScan.h"
#include "../jrd/StatusArg.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/btr.h"
#include "../jrd/Database.h"
#include "../jrd/met_proto.h"
#include "../jrd/vio_proto.h"
#include "../jrd/cch_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/cmp_proto.h"
#include "../jrd/rlck_proto.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinQueryProcessor.h"
#include "../common/gdsassert.h"
#include "../common/StatusArg.h"
#include "../common/utils_proto.h"
#include <cstring>
#include <algorithm>

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// GinTableScan Implementation
//----------------------------

GinTableScan::GinTableScan(CompilerScratch* csb, const string& alias,
							StreamType stream, jrd_rel* relation,
							const index_desc* gin_index, InversionNode* inversion,
							BoolExprNode* contains_condition, double selectivity)
	: RecordStream(csb, stream),
	  m_alias(csb->csb_pool, alias),
	  m_relation(relation),
	  m_gin_index(gin_index),
	  m_inversion(inversion),
	  m_contains_condition(contains_condition),
	  m_gin_index_impl(nullptr),
	  m_query_processor(nullptr),
	  m_tokenizer(nullptr),
	  m_default_query_type(GIN_QUERY_CONTAINS),
	  m_default_strategy(GIN_STRATEGY_HYBRID),
	  m_default_similarity_threshold(0.7),
	  m_default_proximity_distance(5),
	  m_total_queries(0),
	  m_cache_hits(0),
	  m_cache_misses(0),
	  m_average_selectivity(selectivity),
	  m_total_records_scanned(0),
	  m_total_records_returned(0),
	  m_query_time_ms(0),
	  m_bitmap_size(0),
	  m_tokens_processed(0)
{
	fb_assert(gin_index);
	fb_assert(gin_index->idx_type == IDX_TYPE_GIN);
	fb_assert(relation);
	
	// Allocate impure area
	FB_SIZE_T size = sizeof(Impure);
	m_impure = csb->allocImpure(FB_ALIGNMENT, static_cast<ULONG>(size));
	
	// Set cardinality estimate based on selectivity
	m_cardinality = csb->csb_rpt[stream].csb_cardinality * selectivity;
}

void GinTableScan::internalOpen(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);
	
	// Initialize impure area
	memset(impure, 0, sizeof(Impure));
	impure->irsb_flags = irsb_first | irsb_open;
	impure->irsb_eof_reached = false;
	impure->irsb_bitmap_computed = false;
	
	// Initialize record parameter block
	record_param* const rpb = &request->req_rpb[m_stream];
	RLCK_reserve_relation(tdbb, request->req_transaction, m_relation, false);
	rpb->rpb_number.setValue(BOF_NUMBER);
	
	// Initialize GIN context
	if (!initializeGinContext(tdbb, impure)) {
		status_exception::raise(Arg::Gds(isc_random) << "Failed to initialize GIN scan context");
	}
	
	// Lazy initialization of GIN components
	if (!m_gin_index_impl) {
		// Create GIN index implementation
		Database* database = tdbb->getDatabase();
		m_gin_index_impl = new GinIndex(tdbb, database, m_relation, m_gin_index);
		
		// Create query processor
		if (m_gin_index_impl) {
			m_query_processor = new GinQueryProcessor(tdbb, m_gin_index_impl);
			m_tokenizer = m_gin_index_impl->getTokenizer();
		}
	}
}

void GinTableScan::close(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	invalidateRecords(request);
	
	Impure* const impure = request->getImpure<Impure>(m_impure);
	
	if (impure->irsb_flags & irsb_open) {
		impure->irsb_flags &= ~irsb_open;
		
		// Cleanup GIN-specific resources
		cleanupGinContext(impure);
		cleanupBitmaps(impure);
	}
}

bool GinTableScan::internalGetRecord(thread_db* tdbb) const
{
	Request* const request = tdbb->getRequest();
	Impure* const impure = request->getImpure<Impure>(m_impure);
	record_param* const rpb = &request->req_rpb[m_stream];
	
	// Check if we've reached end of file
	if (impure->irsb_eof_reached) {
		return false;
	}
	
	// Execute full-text query if not done yet
	if (!impure->irsb_bitmap_computed) {
		if (!executeFullTextQuery(tdbb, impure)) {
			impure->irsb_eof_reached = true;
			return false;
		}
		impure->irsb_bitmap_computed = true;
	}
	
	// Fetch next matching record
	while (fetchNextRecord(tdbb, impure)) {
		impure->irsb_records_read++;
		
		// Validate the record matches our criteria
		if (validateRecord(tdbb)) {
			// Record is valid, return it
			return true;
		} else {
			// Record doesn't match, continue searching
			impure->irsb_records_rejected++;
			continue;
		}
	}
	
	// No more records
	impure->irsb_eof_reached = true;
	return false;
}

void GinTableScan::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	if (level) {
		plan += '\n';
		for (unsigned i = 0; i < level; i++) {
			plan += "  ";
		}
	}
	
	string index_name = getIndexName();
	plan += printName(tdbb, m_relation->rel_name.c_str(), m_alias);
	plan += " GIN INDEX (";
	plan += index_name;
	plan += ")";
}

void GinTableScan::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, 
								   unsigned level, bool recurse) const
{
	planEntry.className = "GinTableScan";
	planEntry.lines.add().text = "GIN Index Scan";
	
	if (m_alias.hasData() && m_alias != m_relation->rel_name.c_str()) {
		planEntry.alias = m_alias;
	}
	
	planEntry.objectType = obj_relation;
	planEntry.objectName = QualifiedName(m_relation->rel_name.c_str());
	planEntry.accessPath = this;
	
	// Add index information
	string index_name = getIndexName();
	PlanEntry::Line indexLine(planEntry.getPool());
	indexLine.text = "index \"" + index_name + "\"";
	planEntry.lines.add(indexLine);
	
	// Add query type information
	PlanEntry::Line queryLine(planEntry.getPool());
	switch (m_default_query_type) {
		case GIN_QUERY_CONTAINS:
			queryLine.text = "CONTAINS query";
			break;
		case GIN_QUERY_CONTAINS_ANY:
			queryLine.text = "CONTAINS ANY query";
			break;
		case GIN_QUERY_CONTAINS_ALL:
			queryLine.text = "CONTAINS ALL query";
			break;
		case GIN_QUERY_PHRASE:
			queryLine.text = "PHRASE query";
			break;
		case GIN_QUERY_SIMILAR:
			queryLine.text = "SIMILARITY query";
			break;
		default:
			queryLine.text = "Full-text query";
			break;
	}
	planEntry.lines.add(queryLine);
	
	printOptInfo(planEntry.lines);
}

//----------------------------
// GIN-specific interface implementation
//----------------------------

void GinTableScan::setSearchText(const string& search_text)
{
	// This would be called during query compilation
	// For now, we'll extract from the condition during execution
}

void GinTableScan::setQueryType(GinQueryType query_type)
{
	m_default_query_type = query_type;
}

void GinTableScan::setSimilarityThreshold(double threshold)
{
	m_default_similarity_threshold = threshold;
}

void GinTableScan::setProximityDistance(USHORT distance)
{
	m_default_proximity_distance = distance;
}

void GinTableScan::setExecutionStrategy(GinExecutionStrategy strategy)
{
	m_default_strategy = strategy;
}

void GinTableScan::enableQueryOptimization(bool enable)
{
	// Implementation would enable/disable query optimization
}

void GinTableScan::enableResultCaching(bool enable)
{
	// Implementation would enable/disable result caching
}

GinQueryStats GinTableScan::getLastQueryStats() const
{
	GinQueryStats stats;
	memset(&stats, 0, sizeof(stats));
	
	if (m_query_processor) {
		stats = m_query_processor->getLastQueryStats();
	}
	
	return stats;
}

ULONG GinTableScan::getRecordsRead() const
{
	return m_total_records_scanned;
}

ULONG GinTableScan::getRecordsRejected() const
{
	return m_total_records_scanned - m_total_records_returned;
}

double GinTableScan::getQuerySelectivity() const
{
	return m_average_selectivity;
}

string GinTableScan::getIndexName() const
{
	if (m_gin_index && m_gin_index->idx_name) {
		return string(m_gin_index->idx_name);
	}
	return "GIN_INDEX";
}

//----------------------------
// Private method implementations
//----------------------------

bool GinTableScan::executeFullTextQuery(thread_db* tdbb, Impure* impure) const
{
	fb_assert(impure->irsb_scan_context);
	fb_assert(m_query_processor);
	
	GinScanContext* context = impure->irsb_scan_context;
	
	try {
		// Start timing
		ULONG start_time = 0; // Simplified timing for now
		
		// Compute result bitmap
		bool success = computeResultBitmap(tdbb, context);
		
		// End timing and calculate duration
		context->query_stats.query_time_ms = 10; // Placeholder timing
		
		// Update statistics
		m_total_queries++;
		updateQueryStatistics(impure, context->query_stats);
		
		return success;
	}
	catch (const Exception& ex) {
		handleQueryError(tdbb, ex);
		return false;
	}
}

bool GinTableScan::initializeGinContext(thread_db* tdbb, Impure* impure) const
{
	try {
		// Allocate scan context
		impure->irsb_scan_context = new GinScanContext();
		GinScanContext* context = impure->irsb_scan_context;
		
		// Extract query parameters from condition
		if (m_contains_condition) {
			context->query_type = inferQueryType(m_contains_condition);
			context->search_text = extractSearchValue(m_contains_condition);
			context->search_tokens = extractSearchTokens(context->search_text);
		}
		
		// Set default parameters
		context->similarity_threshold = m_default_similarity_threshold;
		context->proximity_distance = m_default_proximity_distance;
		context->strategy = m_default_strategy;
		context->enable_optimization = true;
		context->enable_caching = true;
		
		// Choose optimal strategy
		if (context->enable_optimization && !context->search_tokens.isEmpty()) {
			context->strategy = chooseOptimalStrategy(context->search_tokens);
		}
		
		return true;
	}
	catch (...) {
		return false;
	}
}

bool GinTableScan::computeResultBitmap(thread_db* tdbb, GinScanContext* context) const
{
	fb_assert(m_query_processor);
	fb_assert(context);
	
	try {
		// Get impure area for bitmap storage
		Request* request = tdbb->getRequest();
		Impure* impure = request->getImpure<Impure>(m_impure);
		
		// Create result bitmap
		impure->irsb_gin_bitmap = new RecordBitmap(tdbb->getDefaultPool());
		
		// Execute the appropriate query type
		bool success = false;
		switch (context->query_type) {
			case GIN_QUERY_CONTAINS:
			case GIN_QUERY_CONTAINS_ALL:
				success = m_query_processor->executeContainsAllQuery(
					context->search_tokens, impure->irsb_gin_bitmap, &context->query_stats);
				break;
				
			case GIN_QUERY_CONTAINS_ANY:
				success = m_query_processor->executeContainsAnyQuery(
					context->search_tokens, impure->irsb_gin_bitmap, &context->query_stats);
				break;
				
			case GIN_QUERY_PHRASE:
				success = m_query_processor->executePhraseQuery(
					context->search_tokens, context->proximity_distance, 
					impure->irsb_gin_bitmap, &context->query_stats);
				break;
				
			case GIN_QUERY_SIMILAR: {
				dsc search_desc;
				search_desc.dsc_dtype = dtype_text;
				search_desc.dsc_length = context->search_text.length();
				search_desc.dsc_address = (UCHAR*)context->search_text.c_str();
				
				success = m_query_processor->executeSimilarityQuery(
					&search_desc, context->similarity_threshold,
					impure->irsb_gin_bitmap, &context->query_stats);
				break;
			}
				
			default:
				// Fallback to basic CONTAINS
				success = m_query_processor->executeContainsAllQuery(
					context->search_tokens, impure->irsb_gin_bitmap, &context->query_stats);
				break;
		}
		
		if (success && impure->irsb_gin_bitmap) {
			// Create bitmap iterator
			impure->irsb_gin_iterator = new RecordBitmap::iterator(*impure->irsb_gin_bitmap);
			m_bitmap_size = impure->irsb_gin_bitmap->getCardinality();
		}
		
		return success;
	}
	catch (...) {
		return false;
	}
}

bool GinTableScan::fetchNextRecord(thread_db* tdbb, Impure* impure) const
{
	if (!impure->irsb_gin_iterator) {
		return false;
	}
	
	// Get next record number from bitmap
	RecordNumber record_number;
	if (impure->irsb_gin_iterator->getNext(record_number)) {
		return positionOnRecord(tdbb, record_number);
	}
	
	return false;
}

bool GinTableScan::positionOnRecord(thread_db* tdbb, RecordNumber record_number) const
{
	Request* request = tdbb->getRequest();
	record_param* rpb = &request->req_rpb[m_stream];
	
	// Set the record number
	rpb->rpb_number = record_number;
	
	// Fetch the actual record
	if (VIO_get(tdbb, rpb, request->req_transaction, tdbb->getDefaultPool())) {
		return true;
	}
	
	return false;
}

bool GinTableScan::validateRecord(thread_db* tdbb) const
{
	// Additional validation could be performed here
	// For now, we trust the GIN index results
	return true;
}

TokenList GinTableScan::extractSearchTokens(const string& search_text) const
{
	if (m_tokenizer && !search_text.isEmpty()) {
		return m_tokenizer->tokenize(search_text.c_str());
	}
	
	// Fallback: create simple tokenizer
	GinTokenizer simple_tokenizer(GinTokenizer::SIMPLE_TOKENIZER);
	return simple_tokenizer.tokenize(search_text.c_str());
}

GinQueryType GinTableScan::inferQueryType(const BoolExprNode* condition) const
{
	// For now, default to basic CONTAINS
	// This would be enhanced to parse different types of conditions
	return GIN_QUERY_CONTAINS;
}

string GinTableScan::extractSearchValue(const BoolExprNode* condition) const
{
	// This would extract the search text from the condition
	// For now, return a placeholder
	return string("search_text");
}

GinExecutionStrategy GinTableScan::chooseOptimalStrategy(const TokenList& tokens) const
{
	if (tokens.getCount() == 1) {
		return GIN_STRATEGY_BITMAP;
	} else if (tokens.getCount() <= 3) {
		return GIN_STRATEGY_SORTED_SCAN;
	} else {
		return GIN_STRATEGY_HYBRID;
	}
}

double GinTableScan::estimateQuerySelectivity(const TokenList& tokens) const
{
	// Simple heuristic: fewer tokens = higher selectivity
	if (tokens.getCount() == 0) return 1.0;
	if (tokens.getCount() == 1) return 0.1;
	if (tokens.getCount() <= 3) return 0.05;
	return 0.01;
}

ULONG GinTableScan::estimateResultSize(const TokenList& tokens) const
{
	double selectivity = estimateQuerySelectivity(tokens);
	return static_cast<ULONG>(m_cardinality * selectivity);
}

RecordBitmap* GinTableScan::createFirebirdBitmap(thread_db* tdbb, const TokenList& tokens) const
{
	return new RecordBitmap(tdbb->getDefaultPool());
}

void GinTableScan::convertGinBitmapToFirebird(thread_db* tdbb, RecordBitmap* gin_bitmap, 
											   RecordBitmap* fb_bitmap) const
{
	// Convert between bitmap formats if needed
	// For now, assume they're compatible
}

void GinTableScan::handleQueryError(thread_db* tdbb, const Exception& ex) const
{
	// Log error and potentially fall back to table scan
	// Simplified error handling for now
}

void GinTableScan::cleanupGinContext(Impure* impure) const
{
	if (impure->irsb_scan_context) {
		delete impure->irsb_scan_context;
		impure->irsb_scan_context = nullptr;
	}
	
	if (impure->irsb_result_iterator) {
		delete impure->irsb_result_iterator;
		impure->irsb_result_iterator = nullptr;
	}
}

void GinTableScan::cleanupBitmaps(Impure* impure) const
{
	if (impure->irsb_gin_iterator) {
		delete impure->irsb_gin_iterator;
		impure->irsb_gin_iterator = nullptr;
	}
	
	if (impure->irsb_gin_bitmap) {
		delete impure->irsb_gin_bitmap;
		impure->irsb_gin_bitmap = nullptr;
	}
}

void GinTableScan::updateQueryStatistics(Impure* impure, const GinQueryStats& stats) const
{
	m_query_time_ms = stats.query_time_ms;
	m_tokens_processed = stats.tokens_processed;
	m_total_records_scanned += stats.records_examined;
	m_total_records_returned += stats.records_returned;
	
	// Update average selectivity
	if (m_total_queries > 0) {
		double new_selectivity = static_cast<double>(stats.records_returned) / 
								 static_cast<double>(stats.records_examined);
		m_average_selectivity = (m_average_selectivity * (m_total_queries - 1) + new_selectivity) / 
								m_total_queries;
	}
}

void GinTableScan::recordCacheHit() const
{
	m_cache_hits++;
}

void GinTableScan::recordCacheMiss() const
{
	m_cache_misses++;
}

//----------------------------
// GinContainsNode Implementation
//----------------------------

GinContainsNode::GinContainsNode(MemoryPool& pool, ValueExprNode* field, 
								 ValueExprNode* search_value, GinQueryType query_type)
	: BoolExprNode(pool),
	  m_field(field),
	  m_search_value(search_value),
	  m_query_type(query_type),
	  m_cached_index(nullptr),
	  m_cached_selectivity(0.1),
	  m_analysis_done(false)
{
	fb_assert(field);
	fb_assert(search_value);
}

BoolExprNode* GinContainsNode::pass2(thread_db* tdbb, CompilerScratch* csb)
{
	BoolExprNode::pass2(tdbb, csb);
	
	// Process field and search value
	if (m_field) {
		m_field = m_field->pass2(tdbb, csb);
	}
	
	if (m_search_value) {
		m_search_value = m_search_value->pass2(tdbb, csb);
	}
	
	return this;
}

bool GinContainsNode::execute(thread_db* tdbb, Request* request) const
{
	// This would be called during query execution
	// The actual work is done by GinTableScan
	return true;
}

void GinContainsNode::getLegacyPlan(thread_db* tdbb, string& plan, unsigned level) const
{
	plan += "CONTAINS(";
	if (m_field) {
		// Add field name
		plan += "field";
	}
	plan += ", ";
	if (m_search_value) {
		// Add search value
		plan += "value";
	}
	plan += ")";
}

void GinContainsNode::internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, 
									  unsigned level, bool recurse) const
{
	planEntry.className = "GinContainsNode";
	planEntry.lines.add().text = "CONTAINS condition";
}

bool GinContainsNode::canUseGinIndex(const index_desc* idx) const
{
	return (idx && idx->idx_type == IDX_TYPE_GIN);
}

double GinContainsNode::estimateSelectivity(const index_desc* idx) const
{
	if (!m_analysis_done) {
		// Perform selectivity analysis
		m_cached_selectivity = 0.1; // Default estimate
		m_analysis_done = true;
	}
	
	return m_cached_selectivity;
}

//----------------------------
// GinScanOptimizer Implementation
//----------------------------

const index_desc* GinScanOptimizer::findBestGinIndex(jrd_rel* relation, const BoolExprNode* condition)
{
	// Search for GIN indexes on the relation
	for (USHORT i = 0; i < relation->rel_index_count; i++) {
		const index_desc* idx = &relation->rel_indices[i];
		if (idx->idx_type == IDX_TYPE_GIN && canUseGinIndex(idx, condition)) {
			return idx;
		}
	}
	
	return nullptr;
}

bool GinScanOptimizer::canUseGinIndex(const index_desc* idx, const BoolExprNode* condition)
{
	if (!idx || idx->idx_type != IDX_TYPE_GIN) {
		return false;
	}
	
	// Check if condition is a text search condition
	return isTextSearchCondition(condition);
}

double GinScanOptimizer::estimateGinSelectivity(const index_desc* idx, const BoolExprNode* condition)
{
	// Extract tokens and estimate selectivity
	TokenList tokens = extractTokensFromCondition(condition);
	
	if (tokens.getCount() == 0) return 1.0;
	if (tokens.getCount() == 1) return 0.1;
	if (tokens.getCount() <= 3) return 0.05;
	return 0.01;
}

GinExecutionStrategy GinScanOptimizer::chooseExecutionStrategy(const TokenList& tokens, 
															   const index_desc* idx)
{
	if (tokens.getCount() <= 1) {
		return GIN_STRATEGY_BITMAP;
	} else if (tokens.getCount() <= 3) {
		return GIN_STRATEGY_SORTED_SCAN;
	} else {
		return GIN_STRATEGY_HYBRID;
	}
}

ULONG GinScanOptimizer::estimateQueryCost(const TokenList& tokens, const index_desc* idx)
{
	// Simple cost model based on token count
	return tokens.getCount() * 100 + 1000; // Base cost + per-token cost
}

bool GinScanOptimizer::shouldUseGinIndex(const BoolExprNode* condition, double table_cardinality)
{
	if (!isTextSearchCondition(condition)) {
		return false;
	}
	
	// Use GIN index for text search conditions
	return true;
}

RecordSource* GinScanOptimizer::createGinTableScan(CompilerScratch* csb,
													StreamType stream,
													jrd_rel* relation,
													const BoolExprNode* condition,
													const string& alias)
{
	// Find best GIN index for this condition
	const index_desc* gin_index = findBestGinIndex(relation, condition);
	if (!gin_index) {
		return nullptr;
	}
	
	// Estimate selectivity
	double selectivity = estimateGinSelectivity(gin_index, condition);
	
	// Create GIN table scan
	return new GinTableScan(csb, alias, stream, relation, gin_index, 
							nullptr, const_cast<BoolExprNode*>(condition), selectivity);
}

void GinScanOptimizer::analyzeGinPerformance(const index_desc* idx, const GinQueryStats& stats)
{
	// Analyze performance and update optimizer statistics
}

void GinScanOptimizer::updateOptimizerStatistics(const index_desc* idx, double actual_selectivity)
{
	// Update optimizer statistics with actual selectivity
}

TokenList GinScanOptimizer::extractTokensFromCondition(const BoolExprNode* condition)
{
	TokenList tokens;
	// This would extract tokens from the condition
	// For now, return empty list
	return tokens;
}

GinQueryType GinScanOptimizer::inferQueryTypeFromCondition(const BoolExprNode* condition)
{
	// Analyze condition to determine query type
	return GIN_QUERY_CONTAINS;
}

bool GinScanOptimizer::isTextSearchCondition(const BoolExprNode* condition)
{
	// Check if this is a text search condition (CONTAINS, etc.)
	return true; // Simplified for now
}

const index_desc* GinScanOptimizer::findGinIndexForField(jrd_rel* relation, USHORT field_id)
{
	// Find GIN index for specific field
	for (USHORT i = 0; i < relation->rel_index_count; i++) {
		const index_desc* idx = &relation->rel_indices[i];
		if (idx->idx_type == IDX_TYPE_GIN && idx->idx_fields[0] == field_id) {
			return idx;
		}
	}
	
	return nullptr;
}