// Extension with remaining dfw.epp converted functions
// These functions need to be integrated into the main dfw.cpp file

// Converted from GPRE: compute_security - Compute security class for relation 
static bool compute_security(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra*)
{
/**************************************
 *
 *	c o m p u t e _ s e c u r i t y
 *
 **************************************
 *
 * Functional description
 *	Compute security class for relation.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = MET_lookup_relation(tdbb, work->getQualifiedName());
			if (relation)
			{
				SCL_compute_class(tdbb, work->getQualifiedName().toMetaString());
				MET_clear_cache(tdbb);
			}
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: modify_index - Modify existing index
static bool modify_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Modify an existing index.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = MET_lookup_relation(tdbb, work->getQualifiedName());
			if (!relation)
				return false;

			// Get index information
			AutoRequest request;
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR IDX IN RDB$INDICES "
				"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
				"IDX.RDB$INDEX_NAME EQ ?NAME"), sizeof(SQL_TEXT));

			request.start(transaction);
			request.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
			request.send(0, work->dfw_name.length(), work->dfw_name.c_str());

			if (request.receive(0))
			{
				USHORT index_id;
				request.getData(0, sizeof(index_id), &index_id);

				if (index_id)
				{
					// Delete and recreate the index
					IDX_delete_index(tdbb, relation, index_id - 1);
					
					// Update statistics
					SelectivityList selectivity(*tdbb->getDefaultPool());
					IDX_statistics(tdbb, relation, index_id - 1, selectivity);
					DFW_update_index(work->getQualifiedName(), index_id - 1, selectivity, transaction);
				}
			}
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: create_index - Create a new regular index 
static bool create_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Create a new index.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 0:
		cleanup_index_creation(tdbb, work, transaction);
		return false;

	case 1:
	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = nullptr;
			index_desc idx;
			MOVE_CLEAR(&idx, sizeof(index_desc));

			const auto dbb = tdbb->getDatabase();

			// Look up index metadata
			AutoRequest request;
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR IDX IN RDB$INDICES "
				"CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME "
				"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
				"IDX.RDB$INDEX_NAME EQ ?NAME AND "
				"IDX.RDB$EXPRESSION_BLR MISSING"), sizeof(SQL_TEXT));

			request.start(transaction);
			request.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
			request.send(0, work->dfw_name.length(), work->dfw_name.c_str());

			if (request.receive(0))
			{
				// Get relation
				USHORT relation_id;
				request.getData(0, sizeof(relation_id), &relation_id);
				relation = MET_relation(tdbb, relation_id);

				if (!relation)
					return false;

				// Handle existing index ID
				USHORT index_id;
				request.getData(0, sizeof(index_id), &index_id);

				if (index_id)
				{
					double statistics;
					request.getData(0, sizeof(statistics), &statistics);

					if (statistics < 0.0)
					{
						SelectivityList selectivity(*tdbb->getDefaultPool());
						const USHORT localId = index_id - 1;
						IDX_statistics(tdbb, relation, localId, selectivity);
						DFW_update_index(work->getQualifiedName(), localId, selectivity, transaction);
						return false;
					}

					IDX_delete_index(tdbb, relation, index_id - 1);
					
					// Clear index ID in metadata
					AutoRequest modifyRequest;
					modifyRequest.compile(tdbb, reinterpret_cast<const UCHAR*>(
						"MODIFY IDX IN RDB$INDICES "
						"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
						"IDX.RDB$INDEX_NAME EQ ?NAME "
						"IDX.RDB$INDEX_ID.NULL = TRUE"), sizeof(SQL_TEXT));
					modifyRequest.start(transaction);
					modifyRequest.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
					modifyRequest.send(0, work->dfw_name.length(), work->dfw_name.c_str());
				}

				// Check if index is inactive
				USHORT inactive_flag;
				request.getData(0, sizeof(inactive_flag), &inactive_flag);
				if (inactive_flag)
					return false;

				// Get index properties
				USHORT unique_flag, index_type, segment_count;
				request.getData(0, sizeof(unique_flag), &unique_flag);
				request.getData(0, sizeof(index_type), &index_type);
				request.getData(0, sizeof(segment_count), &segment_count);

				if (unique_flag)
					idx.idx_flags |= idx_unique;
				if (index_type == 1)
					idx.idx_flags |= idx_descending;

				idx.idx_count = segment_count;

				// Get segment information
				AutoRequest segmentRequest;
				segmentRequest.compile(tdbb, reinterpret_cast<const UCHAR*>(
					"FOR SEG IN RDB$INDEX_SEGMENTS "
					"WITH SEG.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
					"SEG.RDB$INDEX_NAME EQ ?NAME "
					"SORTED BY SEG.RDB$FIELD_POSITION"), sizeof(SQL_TEXT));

				segmentRequest.start(transaction);
				segmentRequest.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
				segmentRequest.send(0, work->dfw_name.length(), work->dfw_name.c_str());

				USHORT seg_pos = 0;
				while (segmentRequest.receive(0) && seg_pos < MAX_INDEX_SEGMENTS)
				{
					// Get field information for this segment
					FB_SIZE_T field_name_len;
					char field_name[MAX_SQL_IDENTIFIER_LEN];
					segmentRequest.getData(0, sizeof(field_name_len), &field_name_len);
					segmentRequest.getData(0, field_name_len, field_name);

					// Find field in relation format
					for (USHORT i = 0; i < relation->rel_current_format->fmt_count; i++)
					{
						const Field* field = MET_get_field(relation, i);
						if (field && strcmp(field->fld_name.c_str(), field_name) == 0)
						{
							idx.idx_rpt[seg_pos].idx_field = i;
							idx.idx_rpt[seg_pos].idx_itype = 
								DFW_assign_index_type(tdbb, work->getQualifiedName(), 
									field->fld_dtype, field->fld_sub_type);
							idx.idx_rpt[seg_pos].idx_selectivity = 0;
							break;
						}
					}
					seg_pos++;
				}
			}

			if (!relation)
			{
				// Msg308: can't create index %s
				ERR_post(Arg::Gds(isc_no_meta_update) <<
					Arg::Gds(isc_idx_create_err) << work->getQualifiedName().toQuotedString());
			}

			// Actually create the index
			ProtectRelations protectRelation(tdbb, transaction, relation);

			SelectivityList selectivity(*tdbb->getDefaultPool());

			jrd_tra* const current_transaction = tdbb->getTransaction();
			Request* const current_request = tdbb->getRequest();

			try
			{
				fb_assert(work->dfw_id <= dbb->dbb_max_idx);
				idx.idx_id = work->dfw_id;
				IDX_create_index(tdbb, relation, &idx, work->getQualifiedName(), &work->dfw_id,
					transaction, selectivity);

				fb_assert(work->dfw_id == idx.idx_id);
			}
			catch (const Exception&)
			{
				tdbb->setTransaction(current_transaction);
				tdbb->setRequest(current_request);
				throw;
			}

			tdbb->setTransaction(current_transaction);
			tdbb->setRequest(current_request);

			DFW_update_index(work->getQualifiedName(), idx.idx_id, selectivity, transaction);
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: create_relation - Create a new relation
static bool create_relation(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Create a new relation.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			// Create relation format
			DeferredWork* format_work;
			{
				dsc nameDesc, schemaDesc;
				nameDesc.makeText(static_cast<USHORT>(work->dfw_name.length()), CS_METADATA,
					(UCHAR*) work->dfw_name.c_str());
				schemaDesc.makeText(static_cast<USHORT>(work->dfw_schema.length()), CS_METADATA,
					(UCHAR*) work->dfw_schema.c_str());

				format_work = DFW_post_work(transaction, dfw_make_version, &nameDesc, &schemaDesc, 0);
			}

			// Force relation creation by looking it up
			jrd_rel* relation = MET_lookup_relation(tdbb, work->getQualifiedName());
			if (relation)
			{
				// Relation was successfully created
				MET_scan_relation(tdbb, relation);
			}
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: create_trigger - Create a new trigger
static bool create_trigger(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	c r e a t e _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Create a new trigger.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
	case 2:
		return true;

	case 3:
		{
			// Get trigger information
			AutoRequest request;
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR TRG IN RDB$TRIGGERS "
				"WITH TRG.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
				"TRG.RDB$TRIGGER_NAME EQ ?NAME"), sizeof(SQL_TEXT));

			request.start(transaction);
			request.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
			request.send(0, work->dfw_name.length(), work->dfw_name.c_str());

			if (request.receive(0))
			{
				// Get relation name if trigger is on a table
				FB_SIZE_T relation_name_len;
				char relation_name[MAX_SQL_IDENTIFIER_LEN];
				USHORT relation_name_null;
				request.getData(0, sizeof(relation_name_null), &relation_name_null);

				if (!relation_name_null)
				{
					request.getData(0, sizeof(relation_name_len), &relation_name_len);
					request.getData(0, relation_name_len, relation_name);

					// Get relation
					QualifiedName relName(relation_name, work->dfw_schema.c_str());
					jrd_rel* relation = MET_lookup_relation(tdbb, relName);

					if (relation)
					{
						// Clear relation's trigger cache to force reload
						for (USHORT i = 0; i < TRIGGER_MAX; i++)
						{
							delete relation->rel_pre_triggers[i];
							delete relation->rel_post_triggers[i];
							relation->rel_pre_triggers[i] = nullptr;
							relation->rel_post_triggers[i] = nullptr;
						}

						// Force trigger compilation by scanning relation
						MET_scan_relation(tdbb, relation);
					}
				}

				// Handle dependencies
				bid trigger_blr;
				USHORT blr_null;
				request.getData(0, sizeof(blr_null), &blr_null);

				if (!blr_null)
				{
					request.getData(0, sizeof(trigger_blr), &trigger_blr);
					
					// Create dependencies for trigger BLR
					CompilerScratch* csb = nullptr;
					MET_get_dependencies(tdbb, nullptr, nullptr, 0, nullptr, &trigger_blr,
						nullptr, &csb, work->getQualifiedName(), obj_trigger, 0, transaction);
					delete csb;
				}
			}
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: delete_index - Delete an existing index
static bool delete_index(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	d e l e t e _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Delete an existing index.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_index, transaction);
		break;

	case 2:
		return true;

	case 3:
		{
			// Find relation and index ID
			AutoRequest request;
			request.compile(tdbb, reinterpret_cast<const UCHAR*>(
				"FOR IDX IN RDB$INDICES "
				"CROSS REL IN RDB$RELATIONS OVER RDB$SCHEMA_NAME, RDB$RELATION_NAME "
				"WITH IDX.RDB$SCHEMA_NAME EQ ?SCHEMA AND "
				"IDX.RDB$INDEX_NAME EQ ?NAME"), sizeof(SQL_TEXT));

			request.start(transaction);
			request.send(0, work->dfw_schema.length(), work->dfw_schema.c_str());
			request.send(0, work->dfw_name.length(), work->dfw_name.c_str());

			if (request.receive(0))
			{
				USHORT relation_id, index_id;
				request.getData(0, sizeof(relation_id), &relation_id);
				request.getData(0, sizeof(index_id), &index_id);

				if (index_id)
				{
					jrd_rel* relation = MET_relation(tdbb, relation_id);
					if (relation)
					{
						IDX_delete_index(tdbb, relation, index_id - 1);
					}
				}
			}
		}
		break;

	default:
		break;
	}

	return false;
}

// Converted from GPRE: delete_relation - Delete an existing relation
static bool delete_relation(thread_db* tdbb, SSHORT phase, DeferredWork* work, jrd_tra* transaction)
{
/**************************************
 *
 *	d e l e t e _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Delete an existing relation.
 *
 **************************************/
	SET_TDBB(tdbb);

	switch (phase)
	{
	case 1:
		MET_delete_dependencies(tdbb, work->getQualifiedName(), obj_relation, transaction);
		break;

	case 2:
		return true;

	case 3:
		{
			jrd_rel* relation = MET_lookup_relation(tdbb, work->getQualifiedName());
			if (relation)
			{
				// Delete all indexes for this relation
				for (USHORT i = 0; i < relation->rel_index_count; i++)
				{
					IDX_delete_index(tdbb, relation, i);
				}

				// Clear relation from cache
				MET_release_relation(tdbb, relation);
			}
		}
		break;

	default:
		break;
	}

	return false;
}