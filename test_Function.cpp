/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/***************** gpre version SB-T0.6.0.1-dev ScratchBird 0.6 f90eae0 **********************/
/*
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
 * Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "../common/gdsassert.h"
#include "../jrd/flags.h"
#include "../jrd/jrd.h"
#include "../jrd/val.h"
#include "../jrd/irq.h"
#include "../jrd/tra.h"
#include "../jrd/lck.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/blb.h"
#include "../jrd/met.h"
#include "../jrd/align.h"
#include "../dsql/ExprNodes.h"
#include "../dsql/StmtNodes.h"
#include "../jrd/blb_proto.h"
#include "../jrd/cmp_proto.h"
#include "../common/dsc_proto.h"
#include "../jrd/evl_proto.h"
#include "../jrd/exe_proto.h"
#include "../jrd/flu_proto.h"
#include "../jrd/fun_proto.h"
#include "../jrd/lck_proto.h"
#include "../jrd/met_proto.h"
#include "../jrd/mov_proto.h"
#include "../jrd/par_proto.h"
#include "../jrd/vio_proto.h"
#include "../common/utils_proto.h"
#include "../jrd/DebugInterface.h"
#include "../jrd/trace/TraceJrdHelpers.h"

#include "../jrd/Function.h"

using namespace ScratchBird;
using namespace Jrd;

/*DATABASE DB = FILENAME "ODS.RDB";*/
/**** GDS Preprocessor Definitions ****/
#ifndef JRD_IBASE_H
#include <ibase.h>
#endif

static const ISC_QUAD
   isc_blob_null = {0, 0};	/* initializer for blobs */
isc_db_handle
   DB = 0;		/* database handle */

isc_tr_handle
   gds_trans = 0;		/* default transaction handle */
ISC_STATUS
   isc_status [20],	/* status vector */
   isc_status2 [20];	/* status vector */
ISC_LONG
   isc_array_length, 	/* array return size */
   SQLCODE;		/* SQL status code */
static const short
   isc_0l = 152;
static const unsigned char
   isc_0 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_field, 0, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O', 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','F','U','N','C','T','I','O','N','_','B','L','R', 
			   blr_parameter2, 1, 1,0, 4,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_0 */

static const short
   isc_9l = 333;
static const unsigned char
   isc_9 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 10,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 10, 'R','D','B','$','F','I','E','L','D','S', 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E', 
			      blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_field, 0, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E', 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D', 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D', 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_field, 0, 18, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E', 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H', 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E', 
			   blr_parameter, 1, 7,0, 
			blr_assignment, 
			   blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E', 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E', 
			   blr_parameter, 1, 9,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_9 */

static const short
   isc_24l = 708;
static const unsigned char
   isc_24 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 26,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 22, 'R','D','B','$','F','U','N','C','T','I','O','N','_','A','R','G','U','M','E','N','T','S', 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_field, 0, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E', 
				 blr_parameter, 0, 1,0, 
			      blr_equiv, 
				 blr_field, 0, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E', 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 2,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 2,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_field, 0, 21, 'R','D','B','$','A','R','G','U','M','E','N','T','_','P','O','S','I','T','I','O','N', 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_field, 0, 17, 'R','D','B','$','D','E','F','A','U','L','T','_','V','A','L','U','E', 
			   blr_parameter2, 1, 0,0, 10,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_field, 0, 20, 'R','D','B','$','C','H','A','R','A','C','T','E','R','_','S','E','T','_','I','D', 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_field, 0, 18, 'R','D','B','$','F','I','E','L','D','_','S','U','B','_','T','Y','P','E', 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','L','E','N','G','T','H', 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_field, 0, 15, 'R','D','B','$','F','I','E','L','D','_','S','C','A','L','E', 
			   blr_parameter, 1, 7,0, 
			blr_assignment, 
			   blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','T','Y','P','E', 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','C','O','L','L','A','T','I','O','N','_','I','D', 
			   blr_parameter2, 1, 12,0, 11,0, 
			blr_assignment, 
			   blr_field, 0, 22, 'R','D','B','$','A','R','G','U','M','E','N','T','_','M','E','C','H','A','N','I','S','M', 
			   blr_parameter2, 1, 14,0, 13,0, 
			blr_assignment, 
			   blr_field, 0, 13, 'R','D','B','$','N','U','L','L','_','F','L','A','G', 
			   blr_parameter2, 1, 16,0, 15,0, 
			blr_assignment, 
			   blr_field, 0, 13, 'R','D','B','$','M','E','C','H','A','N','I','S','M', 
			   blr_parameter, 1, 18,0, 
			blr_assignment, 
			   blr_field, 0, 21, 'R','D','B','$','A','R','G','U','M','E','N','T','_','P','O','S','I','T','I','O','N', 
			   blr_parameter, 1, 19,0, 
			blr_assignment, 
			   blr_field, 0, 17, 'R','D','B','$','A','R','G','U','M','E','N','T','_','N','A','M','E', 
			   blr_parameter2, 1, 20,0, 17,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E', 
			   blr_parameter2, 1, 21,0, 9,0, 
			blr_assignment, 
			   blr_field, 0, 28, 'R','D','B','$','F','I','E','L','D','_','S','O','U','R','C','E','_','S','C','H','E','M','A','_','N','A','M','E', 
			   blr_parameter, 1, 22,0, 
			blr_assignment, 
			   blr_field, 0, 17, 'R','D','B','$','R','E','L','A','T','I','O','N','_','N','A','M','E', 
			   blr_parameter2, 1, 23,0, 3,0, 
			blr_assignment, 
			   blr_field, 0, 24, 'R','D','B','$','R','E','L','A','T','I','O','N','_','S','C','H','E','M','A','_','N','A','M','E', 
			   blr_parameter, 1, 24,0, 
			blr_assignment, 
			   blr_field, 0, 14, 'R','D','B','$','F','I','E','L','D','_','N','A','M','E', 
			   blr_parameter2, 1, 25,0, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_24 */

static const short
   isc_56l = 184;
static const unsigned char
   isc_56 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 12, 'R','D','B','$','P','A','C','K','A','G','E','S', 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_field, 0, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E', 
			      blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_field, 0, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S', 
			   blr_parameter2, 1, 3,0, 2,0, 
			blr_assignment, 
			   blr_field, 0, 16, 'R','D','B','$','S','Q','L','_','S','E','C','U','R','I','T','Y', 
			   blr_parameter2, 1, 4,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_56 */

static const short
   isc_66l = 704;
static const unsigned char
   isc_66 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 3, 1,0, 
	    blr_short, 0, 
	 blr_message, 2, 2,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 1, 29,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_relation, 11, 'R','D','B','$','S','C','H','E','M','A','S', 1, 
		     blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_field, 1, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
		     blr_end, 
		  blr_begin, 
		     blr_send, 1, 
			blr_begin, 
			   blr_assignment, 
			      blr_field, 0, 14, 'R','D','B','$','D','E','B','U','G','_','I','N','F','O', 
			      blr_parameter2, 1, 0,0, 8,0, 
			   blr_assignment, 
			      blr_field, 0, 19, 'R','D','B','$','F','U','N','C','T','I','O','N','_','S','O','U','R','C','E', 
			      blr_parameter2, 1, 1,0, 9,0, 
			   blr_assignment, 
			      blr_field, 0, 16, 'R','D','B','$','F','U','N','C','T','I','O','N','_','B','L','R', 
			      blr_parameter2, 1, 2,0, 10,0, 
			   blr_assignment, 
			      blr_field, 0, 15, 'R','D','B','$','M','O','D','U','L','E','_','N','A','M','E', 
			      blr_parameter2, 1, 3,0, 13,0, 
			   blr_assignment, 
			      blr_field, 0, 14, 'R','D','B','$','E','N','T','R','Y','P','O','I','N','T', 
			      blr_parameter2, 1, 4,0, 12,0, 
			   blr_assignment, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			      blr_parameter, 1, 5,0, 
			   blr_assignment, 
			      blr_field, 0, 13, 'R','D','B','$','V','A','L','I','D','_','B','L','R', 
			      blr_parameter2, 1, 7,0, 6,0, 
			   blr_assignment, 
			      blr_field, 0, 22, 'R','D','B','$','D','E','T','E','R','M','I','N','I','S','T','I','C','_','F','L','A','G', 
			      blr_parameter2, 1, 15,0, 14,0, 
			   blr_assignment, 
			      blr_field, 0, 19, 'R','D','B','$','R','E','T','U','R','N','_','A','R','G','U','M','E','N','T', 
			      blr_parameter, 1, 16,0, 
			   blr_assignment, 
			      blr_field, 0, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E', 
			      blr_parameter, 1, 20,0, 
			   blr_assignment, 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 1, 21,0, 
			   blr_assignment, 
			      blr_field, 0, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E', 
			      blr_parameter2, 1, 22,0, 19,0, 
			   blr_assignment, 
			      blr_field, 0, 14, 'R','D','B','$','O','W','N','E','R','_','N','A','M','E', 
			      blr_parameter, 1, 23,0, 
			   blr_assignment, 
			      blr_field, 0, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S', 
			      blr_parameter2, 1, 24,0, 18,0, 
			   blr_assignment, 
			      blr_field, 1, 18, 'R','D','B','$','S','E','C','U','R','I','T','Y','_','C','L','A','S','S', 
			      blr_parameter, 1, 25,0, 
			   blr_assignment, 
			      blr_field, 1, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 1, 26,0, 
			   blr_assignment, 
			      blr_field, 0, 16, 'R','D','B','$','S','Q','L','_','S','E','C','U','R','I','T','Y', 
			      blr_parameter2, 1, 27,0, 17,0, 
			   blr_assignment, 
			      blr_field, 0, 15, 'R','D','B','$','E','N','G','I','N','E','_','N','A','M','E', 
			      blr_parameter2, 1, 28,0, 11,0, 
			   blr_end, 
		     blr_label, 0, 
			blr_loop, 
			   blr_select, 
			      blr_receive, 3, 
				 blr_leave, 0, 
			      blr_receive, 2, 
				 blr_modify, 0, 2, 
				    blr_begin, 
				       blr_assignment, 
					  blr_parameter2, 2, 1,0, 0,0, 
					  blr_field, 2, 13, 'R','D','B','$','V','A','L','I','D','_','B','L','R', 
				       blr_end, 
			      blr_end, 
		     blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 5,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_66 */

static const short
   isc_104l = 188;
static const unsigned char
   isc_104 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 3,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_field, 0, 17, 'R','D','B','$','F','U','N','C','T','I','O','N','_','N','A','M','E', 
				 blr_parameter, 0, 1,0, 
			      blr_equiv, 
				 blr_field, 0, 16, 'R','D','B','$','P','A','C','K','A','G','E','_','N','A','M','E', 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 2,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 2,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_104 */

static const short
   isc_112l = 119;
static const unsigned char
   isc_112 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_112 */


#define gds_blob_null	isc_blob_null	/* compatibility symbols */
#define gds_status	isc_status
#define gds_status2	isc_status2
#define gds_array_length	isc_array_length
#define gds_count	isc_count
#define gds_slack	isc_slack
#define gds_utility	isc_utility	/* end of compatibility symbols */

#ifndef isc_version4
    Generate a compile-time error.
    Picking up a V3 include file after preprocessing with V4 GPRE.
#endif

/**** end of GPRE definitions ****/


const char* const Function::EXCEPTION_MESSAGE = "The user defined function: \t%s\n\t   referencing"
	" entrypoint: \t%s\n\t                in module: \t%s\n\tcaused the fatal exception:";

Function* Function::lookup(thread_db* tdbb, USHORT id, bool return_deleted, bool noscan, USHORT flags)
{
   struct isc_115_struct {
          short isc_116;	/* isc_utility */
          short isc_117;	/* RDB$FUNCTION_ID */
   } isc_115;
   struct isc_113_struct {
          short isc_114;	/* RDB$FUNCTION_ID */
   } isc_113;
	Jrd::Attachment* attachment = tdbb->getAttachment();
	Function* check_function = NULL;

	Function* function = (id < attachment->att_functions.getCount()) ? attachment->att_functions[id] : NULL;

	if (function && function->getId() == id &&
		!(function->flags & Routine::FLAG_CLEARED) &&
		!(function->flags & Routine::FLAG_BEING_SCANNED) &&
		((function->flags & Routine::FLAG_SCANNED) || noscan) &&
		!(function->flags & Routine::FLAG_BEING_ALTERED) &&
		(!(function->flags & Routine::FLAG_OBSOLETE) || return_deleted))
	{
		if (!(function->flags & Routine::FLAG_CHECK_EXISTENCE))
		{
			return function;
		}

		check_function = function;
		LCK_lock(tdbb, check_function->existenceLock, LCK_SR, LCK_WAIT);
	}

	// We need to look up the function in RDB$FUNCTIONS

	function = NULL;

	AutoCacheRequest request(tdbb, irq_l_fun_id, IRQ_REQUESTS);

	/*FOR(REQUEST_HANDLE request)
		X IN RDB$FUNCTIONS WITH X.RDB$FUNCTION_ID EQ id*/
	{
	if (!request)
	   {
	   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request, (short) sizeof(isc_112), (char*) isc_112);
	   }
	isc_113.isc_114 = id;
	isc_start_and_send (NULL, (FB_API_HANDLE*) &request, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 2, &isc_113, (short) 0);
	while (1)
	   {
	   isc_receive (NULL, (FB_API_HANDLE*) &request, (short) 1, (short) 4, &isc_115, (short) 0);
	   if (!isc_115.isc_116) break;
	{
		function = loadMetadata(tdbb, /*X.RDB$FUNCTION_ID*/
					      isc_115.isc_117, noscan, flags);
	}
	/*END_FOR*/
	   }
	}

	if (check_function)
	{
		check_function->flags &= ~Routine::FLAG_CHECK_EXISTENCE;
		if (check_function != function)
		{
			LCK_release(tdbb, check_function->existenceLock);
			check_function->flags |= Routine::FLAG_OBSOLETE;
		}
	}

	return function;
}

Function* Function::lookup(thread_db* tdbb, const QualifiedName& name, bool noscan)
{
   struct isc_109_struct {
          short isc_110;	/* isc_utility */
          short isc_111;	/* RDB$FUNCTION_ID */
   } isc_109;
   struct isc_105_struct {
          char  isc_106 [253];	/* RDB$SCHEMA_NAME */
          char  isc_107 [253];	/* RDB$FUNCTION_NAME */
          char  isc_108 [253];	/* RDB$PACKAGE_NAME */
   } isc_105;
	Jrd::Attachment* attachment = tdbb->getAttachment();

	Function* check_function = NULL;

	// See if we already know the function by name

	for (Function** iter = attachment->att_functions.begin(); iter < attachment->att_functions.end(); ++iter)
	{
		Function* const function = *iter;

		if (function && !(function->flags & Routine::FLAG_OBSOLETE) &&
			!(function->flags & Routine::FLAG_CLEARED) &&
			((function->flags & Routine::FLAG_SCANNED) || noscan) &&
			!(function->flags & Routine::FLAG_BEING_SCANNED) &&
			!(function->flags & Routine::FLAG_BEING_ALTERED))
		{
			if (function->getName() == name)
			{
				if (function->flags & Routine::FLAG_CHECK_EXISTENCE)
				{
					check_function = function;
					LCK_lock(tdbb, check_function->existenceLock, LCK_SR, LCK_WAIT);
					break;
				}

				return function;
			}
		}
	}

	// We need to look up the function in RDB$FUNCTIONS

	Function* function = NULL;

	AutoCacheRequest request(tdbb, irq_l_fun_name, IRQ_REQUESTS);

	/*FOR(REQUEST_HANDLE request)
		X IN RDB$FUNCTIONS
		WITH X.RDB$SCHEMA_NAME EQ name.schema.c_str() AND
			 X.RDB$FUNCTION_NAME EQ name.object.c_str() AND
			 X.RDB$PACKAGE_NAME EQUIV NULLIF(name.package.c_str(), '')*/
	{
	if (!request)
	   {
	   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request, (short) sizeof(isc_104), (char*) isc_104);
	   }
	isc_vtov ((const char*) name.schema.c_str(), (char*) isc_105.isc_106, 253);
	isc_vtov ((const char*) name.object.c_str(), (char*) isc_105.isc_107, 253);
	isc_vtov ((const char*) name.package.c_str(), (char*) isc_105.isc_108, 253);
	isc_start_and_send (NULL, (FB_API_HANDLE*) &request, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 759, &isc_105, (short) 0);
	while (1)
	   {
	   isc_receive (NULL, (FB_API_HANDLE*) &request, (short) 1, (short) 4, &isc_109, (short) 0);
	   if (!isc_109.isc_110) break;
	{
		function = loadMetadata(tdbb, /*X.RDB$FUNCTION_ID*/
					      isc_109.isc_111, noscan, 0);
	}
	/*END_FOR*/
	   }
	}

	if (check_function)
	{
		check_function->flags &= ~Routine::FLAG_CHECK_EXISTENCE;
		if (check_function != function)
		{
			LCK_release(tdbb, check_function->existenceLock);
			check_function->flags |= Routine::FLAG_OBSOLETE;
		}
	}

	return function;
}

Function* Function::loadMetadata(thread_db* tdbb, USHORT id, bool noscan, USHORT flags)
{
   struct isc_13_struct {
          ISC_QUAD isc_14;	/* RDB$DEFAULT_VALUE */
          short isc_15;	/* isc_utility */
          short isc_16;	/* gds__null_flag */
          short isc_17;	/* RDB$COLLATION_ID */
          short isc_18;	/* RDB$CHARACTER_SET_ID */
          short isc_19;	/* RDB$FIELD_SUB_TYPE */
          short isc_20;	/* RDB$FIELD_LENGTH */
          short isc_21;	/* RDB$FIELD_SCALE */
          short isc_22;	/* RDB$FIELD_TYPE */
          char  isc_23 [253];	/* RDB$FIELD_NAME */
   } isc_13;
   struct isc_10_struct {
          char  isc_11 [253];	/* RDB$FIELD_SOURCE_SCHEMA_NAME */
          char  isc_12 [253];	/* RDB$FIELD_SOURCE */
   } isc_10;
   struct isc_29_struct {
          ISC_QUAD isc_30;	/* RDB$DEFAULT_VALUE */
          short isc_31;	/* isc_utility */
          short isc_32;	/* gds__null_flag */
          short isc_33;	/* gds__null_flag */
          short isc_34;	/* RDB$CHARACTER_SET_ID */
          short isc_35;	/* RDB$FIELD_SUB_TYPE */
          short isc_36;	/* RDB$FIELD_LENGTH */
          short isc_37;	/* RDB$FIELD_SCALE */
          short isc_38;	/* RDB$FIELD_TYPE */
          short isc_39;	/* gds__null_flag */
          short isc_40;	/* gds__null_flag */
          short isc_41;	/* gds__null_flag */
          short isc_42;	/* RDB$COLLATION_ID */
          short isc_43;	/* gds__null_flag */
          short isc_44;	/* RDB$ARGUMENT_MECHANISM */
          short isc_45;	/* gds__null_flag */
          short isc_46;	/* RDB$NULL_FLAG */
          short isc_47;	/* gds__null_flag */
          short isc_48;	/* RDB$MECHANISM */
          short isc_49;	/* RDB$ARGUMENT_POSITION */
          char  isc_50 [253];	/* RDB$ARGUMENT_NAME */
          char  isc_51 [253];	/* RDB$FIELD_SOURCE */
          char  isc_52 [253];	/* RDB$FIELD_SOURCE_SCHEMA_NAME */
          char  isc_53 [253];	/* RDB$RELATION_NAME */
          char  isc_54 [253];	/* RDB$RELATION_SCHEMA_NAME */
          char  isc_55 [253];	/* RDB$FIELD_NAME */
   } isc_29;
   struct isc_25_struct {
          char  isc_26 [253];	/* RDB$SCHEMA_NAME */
          char  isc_27 [253];	/* RDB$FUNCTION_NAME */
          char  isc_28 [253];	/* RDB$PACKAGE_NAME */
   } isc_25;
   struct isc_60_struct {
          short isc_61;	/* isc_utility */
          short isc_62;	/* gds__null_flag */
          short isc_63;	/* gds__null_flag */
          char  isc_64 [253];	/* RDB$SECURITY_CLASS */
          FB_BOOLEAN isc_65;	/* RDB$SQL_SECURITY */
   } isc_60;
   struct isc_57_struct {
          char  isc_58 [253];	/* RDB$SCHEMA_NAME */
          char  isc_59 [253];	/* RDB$PACKAGE_NAME */
   } isc_57;
   struct isc_102_struct {
          short isc_103;	/* isc_utility */
   } isc_102;
   struct isc_99_struct {
          short isc_100;	/* gds__null_flag */
          short isc_101;	/* RDB$VALID_BLR */
   } isc_99;
   struct isc_69_struct {
          ISC_QUAD isc_70;	/* RDB$DEBUG_INFO */
          ISC_QUAD isc_71;	/* RDB$FUNCTION_SOURCE */
          ISC_QUAD isc_72;	/* RDB$FUNCTION_BLR */
          char  isc_73 [256];	/* RDB$MODULE_NAME */
          char  isc_74 [256];	/* RDB$ENTRYPOINT */
          short isc_75;	/* isc_utility */
          short isc_76;	/* gds__null_flag */
          short isc_77;	/* RDB$VALID_BLR */
          short isc_78;	/* gds__null_flag */
          short isc_79;	/* gds__null_flag */
          short isc_80;	/* gds__null_flag */
          short isc_81;	/* gds__null_flag */
          short isc_82;	/* gds__null_flag */
          short isc_83;	/* gds__null_flag */
          short isc_84;	/* gds__null_flag */
          short isc_85;	/* RDB$DETERMINISTIC_FLAG */
          short isc_86;	/* RDB$RETURN_ARGUMENT */
          short isc_87;	/* gds__null_flag */
          short isc_88;	/* gds__null_flag */
          short isc_89;	/* gds__null_flag */
          char  isc_90 [253];	/* RDB$FUNCTION_NAME */
          char  isc_91 [253];	/* RDB$SCHEMA_NAME */
          char  isc_92 [253];	/* RDB$PACKAGE_NAME */
          char  isc_93 [253];	/* RDB$OWNER_NAME */
          char  isc_94 [253];	/* RDB$SECURITY_CLASS */
          char  isc_95 [253];	/* RDB$SECURITY_CLASS */
          char  isc_96 [253];	/* RDB$SCHEMA_NAME */
          FB_BOOLEAN isc_97;	/* RDB$SQL_SECURITY */
          char  isc_98 [253];	/* RDB$ENGINE_NAME */
   } isc_69;
   struct isc_67_struct {
          short isc_68;	/* RDB$FUNCTION_ID */
   } isc_67;
	Jrd::Attachment* attachment = tdbb->getAttachment();
	jrd_tra* sysTransaction = attachment->getSysTransaction();
	Database* const dbb = tdbb->getDatabase();

	if (id >= attachment->att_functions.getCount())
		attachment->att_functions.grow(id + 1);

	Function* function = attachment->att_functions[id];

	if (function && !(function->flags & Routine::FLAG_OBSOLETE))
	{
		// Make sure Routine::FLAG_BEING_SCANNED and Routine::FLAG_SCANNED are not set at the same time
		fb_assert(!(function->flags & Routine::FLAG_BEING_SCANNED) ||
			!(function->flags & Routine::FLAG_SCANNED));

		if ((function->flags & Routine::FLAG_BEING_SCANNED) ||
			(function->flags & Routine::FLAG_SCANNED))
		{
			return function;
		}
	}

	if (!function)
		function = FB_NEW_POOL(*attachment->att_pool) Function(*attachment->att_pool);

	try
	{
	function->flags |= (Routine::FLAG_BEING_SCANNED | flags);
	function->flags &= ~(Routine::FLAG_OBSOLETE | Routine::FLAG_CLEARED);

	function->setId(id);
	attachment->att_functions[id] = function;

	if (!function->existenceLock)
	{
		Lock* const lock = FB_NEW_RPT(*attachment->att_pool, 0)
			Lock(tdbb, sizeof(SLONG), LCK_fun_exist, function, blockingAst);
		function->existenceLock = lock;
		lock->setKey(function->getId());
	}

	LCK_lock(tdbb, function->existenceLock, LCK_SR, LCK_WAIT);

	if (!noscan)
	{
		AutoCacheRequest request_fun(tdbb, irq_l_functions, IRQ_REQUESTS);

		/*FOR(REQUEST_HANDLE request_fun)
			X IN RDB$FUNCTIONS
			CROSS SCH IN RDB$SCHEMAS
			WITH X.RDB$FUNCTION_ID EQ id AND
				 SCH.RDB$SCHEMA_NAME EQ X.RDB$SCHEMA_NAME*/
		{
		if (!request_fun)
		   {
		   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request_fun, (short) sizeof(isc_66), (char*) isc_66);
		   }
		isc_67.isc_68 = id;
		isc_start_and_send (NULL, (FB_API_HANDLE*) &request_fun, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 2, &isc_67, (short) 0);
		while (1)
		   {
		   isc_receive (NULL, (FB_API_HANDLE*) &request_fun, (short) 1, (short) 2591, &isc_69, (short) 0);
		   if (!isc_69.isc_75) break;
		{
			function->setName(QualifiedName(/*X.RDB$FUNCTION_NAME*/
							isc_69.isc_90, /*X.RDB$SCHEMA_NAME*/
  isc_69.isc_91,
				(/*X.RDB$PACKAGE_NAME.NULL*/
				 isc_69.isc_89 ? NULL : /*X.RDB$PACKAGE_NAME*/
	  isc_69.isc_92)));

			function->owner = /*X.RDB$OWNER_NAME*/
					  isc_69.isc_93;
			ScratchBird::TriState ssDefiner;

			if (!/*X.RDB$SECURITY_CLASS.NULL*/
			     isc_69.isc_88)
				function->setSecurityName(QualifiedName(/*X.RDB$SECURITY_CLASS*/
									isc_69.isc_94, /*SCH.RDB$SECURITY_CLASS*/
  isc_69.isc_95));
			else if (!/*X.RDB$PACKAGE_NAME.NULL*/
				  isc_69.isc_89)
			{
				AutoCacheRequest requestHandle(tdbb, irq_l_procedure_pkg_class, IRQ_REQUESTS);

				/*FOR (REQUEST_HANDLE requestHandle)
					PKG IN RDB$PACKAGES
					WITH PKG.RDB$SCHEMA_NAME EQ SCH.RDB$SCHEMA_NAME AND
						 PKG.RDB$PACKAGE_NAME EQ X.RDB$PACKAGE_NAME*/
				{
				if (!requestHandle)
				   {
				   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &requestHandle, (short) sizeof(isc_56), (char*) isc_56);
				   }
				isc_vtov ((const char*) isc_69.isc_96, (char*) isc_57.isc_58, 253);
				isc_vtov ((const char*) isc_69.isc_92, (char*) isc_57.isc_59, 253);
				isc_start_and_send (NULL, (FB_API_HANDLE*) &requestHandle, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 506, &isc_57, (short) 0);
				while (1)
				   {
				   isc_receive (NULL, (FB_API_HANDLE*) &requestHandle, (short) 1, (short) 260, &isc_60, (short) 0);
				   if (!isc_60.isc_61) break;
				{
					if (!/*PKG.RDB$SECURITY_CLASS.NULL*/
					     isc_60.isc_63)
						function->setSecurityName(QualifiedName(/*PKG.RDB$SECURITY_CLASS*/
											isc_60.isc_64, /*SCH.RDB$SECURITY_CLASS*/
  isc_69.isc_95));

					// SQL SECURITY of function must be the same if it's defined in package
					if (!/*PKG.RDB$SQL_SECURITY.NULL*/
					     isc_60.isc_62)
						ssDefiner = (bool) /*PKG.RDB$SQL_SECURITY*/
								   isc_60.isc_65;
				}
				/*END_FOR*/
				   }
				}
			}

			if (!ssDefiner.isAssigned())
			{
				if (!/*X.RDB$SQL_SECURITY.NULL*/
				     isc_69.isc_87)
					ssDefiner = (bool) /*X.RDB$SQL_SECURITY*/
							   isc_69.isc_97;
				else
					ssDefiner = MET_get_ss_definer(tdbb, /*X.RDB$SCHEMA_NAME*/
									     isc_69.isc_91);
			}

			if (ssDefiner.asBool())
				function->invoker = attachment->getUserId(function->owner);

			size_t count = 0;
			ULONG length = 0;

			function->fun_inputs = 0;
			function->setDefaultCount(0);

			function->getInputFields().clear();
			function->getOutputFields().clear();

			AutoCacheRequest request_arg(tdbb, irq_l_args, IRQ_REQUESTS);

			/*FOR(REQUEST_HANDLE request_arg)
				Y IN RDB$FUNCTION_ARGUMENTS
				WITH Y.RDB$SCHEMA_NAME EQ function->getName().schema.c_str() AND
					 Y.RDB$FUNCTION_NAME EQ function->getName().object.c_str() AND
					 Y.RDB$PACKAGE_NAME EQUIV NULLIF(function->getName().package.c_str(), '')
				SORTED BY Y.RDB$ARGUMENT_POSITION*/
			{
			if (!request_arg)
			   {
			   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request_arg, (short) sizeof(isc_24), (char*) isc_24);
			   }
			isc_vtov ((const char*) function->getName().schema.c_str(), (char*) isc_25.isc_26, 253);
			isc_vtov ((const char*) function->getName().object.c_str(), (char*) isc_25.isc_27, 253);
			isc_vtov ((const char*) function->getName().package.c_str(), (char*) isc_25.isc_28, 253);
			isc_start_and_send (NULL, (FB_API_HANDLE*) &request_arg, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 759, &isc_25, (short) 0);
			while (1)
			   {
			   isc_receive (NULL, (FB_API_HANDLE*) &request_arg, (short) 1, (short) 1564, &isc_29, (short) 0);
			   if (!isc_29.isc_31) break;
			{
				Parameter* parameter = FB_NEW_POOL(function->getPool()) Parameter(function->getPool());

				if (/*Y.RDB$ARGUMENT_POSITION*/
				    isc_29.isc_49 != /*X.RDB$RETURN_ARGUMENT*/
    isc_69.isc_86)
				{
					function->fun_inputs++;
					int newCount = /*Y.RDB$ARGUMENT_POSITION*/
						       isc_29.isc_49 - function->getOutputFields().getCount();
					fb_assert(newCount >= 0);

					function->getInputFields().resize(newCount + 1);
					function->getInputFields()[newCount] = parameter;
				}
				else
				{
					fb_assert(function->getOutputFields().isEmpty());
					function->getOutputFields().add(parameter);
				}

				parameter->prm_fun_mechanism = (FUN_T) /*Y.RDB$MECHANISM*/
								       isc_29.isc_48;
				parameter->prm_number = /*Y.RDB$ARGUMENT_POSITION*/
							isc_29.isc_49;
				parameter->prm_name = /*Y.RDB$ARGUMENT_NAME.NULL*/
						      isc_29.isc_47 ? "" : /*Y.RDB$ARGUMENT_NAME*/
	isc_29.isc_50;
				parameter->prm_nullable = /*Y.RDB$NULL_FLAG.NULL*/
							  isc_29.isc_45 || /*Y.RDB$NULL_FLAG*/
    isc_29.isc_46 == 0;
				parameter->prm_mechanism = /*Y.RDB$ARGUMENT_MECHANISM.NULL*/
							   isc_29.isc_43 ?
					prm_mech_normal : (prm_mech_t) /*Y.RDB$ARGUMENT_MECHANISM*/
								       isc_29.isc_44;

				const SSHORT collation_id_null = /*Y.RDB$COLLATION_ID.NULL*/
								 isc_29.isc_41;
				const SSHORT collation_id = /*Y.RDB$COLLATION_ID*/
							    isc_29.isc_42;

				SSHORT default_value_null = /*Y.RDB$DEFAULT_VALUE.NULL*/
							    isc_29.isc_40;
				bid default_value;
				default_value.bid_quad.bid_quad_high = /*Y.RDB$DEFAULT_VALUE*/
								       isc_29.isc_30.gds_quad_high;
				default_value.bid_quad.bid_quad_low = /*Y.RDB$DEFAULT_VALUE*/
								      isc_29.isc_30.gds_quad_low;

				if (!/*Y.RDB$FIELD_SOURCE.NULL*/
				     isc_29.isc_39)
				{
					parameter->prm_field_source = QualifiedName(/*Y.RDB$FIELD_SOURCE*/
										    isc_29.isc_51, /*Y.RDB$FIELD_SOURCE_SCHEMA_NAME*/
  isc_29.isc_52);

					AutoCacheRequest request_arg_fld(tdbb, irq_l_arg_fld, IRQ_REQUESTS);

					/*FOR(REQUEST_HANDLE request_arg_fld)
						F IN RDB$FIELDS
						WITH F.RDB$SCHEMA_NAME = Y.RDB$FIELD_SOURCE_SCHEMA_NAME AND
							 F.RDB$FIELD_NAME = Y.RDB$FIELD_SOURCE*/
					{
					if (!request_arg_fld)
					   {
					   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request_arg_fld, (short) sizeof(isc_9), (char*) isc_9);
					   }
					isc_vtov ((const char*) isc_29.isc_52, (char*) isc_10.isc_11, 253);
					isc_vtov ((const char*) isc_29.isc_51, (char*) isc_10.isc_12, 253);
					isc_start_and_send (NULL, (FB_API_HANDLE*) &request_arg_fld, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 506, &isc_10, (short) 0);
					while (1)
					   {
					   isc_receive (NULL, (FB_API_HANDLE*) &request_arg_fld, (short) 1, (short) 277, &isc_13, (short) 0);
					   if (!isc_13.isc_15) break;
					{
						DSC_make_descriptor(&parameter->prm_desc, /*F.RDB$FIELD_TYPE*/
											  isc_13.isc_22,
											/*F.RDB$FIELD_SCALE*/
											isc_13.isc_21, /*F.RDB$FIELD_LENGTH*/
  isc_13.isc_20,
											/*F.RDB$FIELD_SUB_TYPE*/
											isc_13.isc_19, /*F.RDB$CHARACTER_SET_ID*/
  isc_13.isc_18,
											(collation_id_null ? /*F.RDB$COLLATION_ID*/
													     isc_13.isc_17 : collation_id));

						if (default_value_null && fb_utils::implicit_domain(/*F.RDB$FIELD_NAME*/
												    isc_13.isc_23))
						{
							default_value_null = /*F.RDB$DEFAULT_VALUE.NULL*/
									     isc_13.isc_16;
							default_value.bid_quad.bid_quad_high = /*F.RDB$DEFAULT_VALUE*/
											       isc_13.isc_14.gds_quad_high;
						default_value.bid_quad.bid_quad_low = /*F.RDB$DEFAULT_VALUE*/
										      isc_13.isc_14.gds_quad_low;
						}
					}
					/*END_FOR*/
					   }
					}
				}
				else
				{
					DSC_make_descriptor(&parameter->prm_desc, /*Y.RDB$FIELD_TYPE*/
										  isc_29.isc_38,
										/*Y.RDB$FIELD_SCALE*/
										isc_29.isc_37, /*Y.RDB$FIELD_LENGTH*/
  isc_29.isc_36,
										/*Y.RDB$FIELD_SUB_TYPE*/
										isc_29.isc_35, /*Y.RDB$CHARACTER_SET_ID*/
  isc_29.isc_34,
										(collation_id_null ? 0 : collation_id));
				}

				if (parameter->prm_desc.isText() && parameter->prm_desc.getTextType() != CS_NONE)
				{
					if (!collation_id_null ||
						(!/*Y.RDB$FIELD_SOURCE.NULL*/
						  isc_29.isc_39 && fb_utils::implicit_domain(/*Y.RDB$FIELD_SOURCE*/
			      isc_29.isc_51)))
					{
						parameter->prm_text_type = parameter->prm_desc.getTextType();
					}
				}

				if (!/*Y.RDB$RELATION_NAME.NULL*/
				     isc_29.isc_33)
					parameter->prm_type_of_table = QualifiedName(/*Y.RDB$RELATION_NAME*/
										     isc_29.isc_53, /*Y.RDB$RELATION_SCHEMA_NAME*/
  isc_29.isc_54);

				if (!/*Y.RDB$FIELD_NAME.NULL*/
				     isc_29.isc_32)
					parameter->prm_type_of_column = /*Y.RDB$FIELD_NAME*/
									isc_29.isc_55;

				if (/*Y.RDB$ARGUMENT_POSITION*/
				    isc_29.isc_49 != /*X.RDB$RETURN_ARGUMENT*/
    isc_69.isc_86 && !default_value_null)
				{
					function->setDefaultCount(function->getDefaultCount() + 1);

					MemoryPool* const csb_pool = attachment->createPool();
					Jrd::ContextPoolHolder context(tdbb, csb_pool);

					try
					{
						parameter->prm_default_value = static_cast<ValueExprNode*>(MET_parse_blob(
							tdbb, &function->getName().schema, NULL, &default_value, NULL, NULL, false, false));
					}
					catch (const ScratchBird::Exception&)
					{
						attachment->deletePool(csb_pool);
						throw; // an explicit error message would be better
					}
				}

				if (parameter->prm_desc.dsc_dtype == dtype_cstring)
					parameter->prm_desc.dsc_length++;

				length += (parameter->prm_desc.dsc_dtype == dtype_blob) ?
					sizeof(udf_blob) : FB_ALIGN(parameter->prm_desc.dsc_length, FB_DOUBLE_ALIGN);

				count = MAX(count, size_t(/*Y.RDB$ARGUMENT_POSITION*/
							  isc_29.isc_49));
			}
			/*END_FOR*/
			   }
			}

			for (int i = (int) function->getInputFields().getCount() - 1; i >= 0; --i)
			{
				if (!function->getInputFields()[i])
					function->getInputFields().remove(i);
			}

			function->fun_return_arg = /*X.RDB$RETURN_ARGUMENT*/
						   isc_69.isc_86;
			function->fun_temp_length = length;

			// Prepare the exception message to be used in case this function ever
			// causes an exception.  This is done at this time to save us from preparing
			// (thus allocating) this message every time the function is called.
			function->fun_exception_message.printf(EXCEPTION_MESSAGE,
				function->getName().toQuotedString().c_str(), /*X.RDB$ENTRYPOINT*/
									      isc_69.isc_74, /*X.RDB$MODULE_NAME*/
  isc_69.isc_73);

			if (!/*X.RDB$DETERMINISTIC_FLAG.NULL*/
			     isc_69.isc_84)
				function->fun_deterministic = (/*X.RDB$DETERMINISTIC_FLAG*/
							       isc_69.isc_85 != 0);

			function->setImplemented(true);
			function->setDefined(true);

			function->fun_entrypoint = NULL;
			function->fun_external = NULL;
			function->setStatement(NULL);

			if (!/*X.RDB$MODULE_NAME.NULL*/
			     isc_69.isc_83 && !/*X.RDB$ENTRYPOINT.NULL*/
     isc_69.isc_82)
			{
				function->fun_entrypoint =
					Module::lookup(/*X.RDB$MODULE_NAME*/
						       isc_69.isc_73, /*X.RDB$ENTRYPOINT*/
  isc_69.isc_74, dbb);

				// Could not find a function with given MODULE, ENTRYPOINT.
				// Try the list of internally implemented functions.
				if (!function->fun_entrypoint)
				{
					function->fun_entrypoint =
						BUILTIN_entrypoint(/*X.RDB$MODULE_NAME*/
								   isc_69.isc_73, /*X.RDB$ENTRYPOINT*/
  isc_69.isc_74);
				}

				if (!function->fun_entrypoint)
					function->setDefined(false);
			}
			else if (!/*X.RDB$ENGINE_NAME.NULL*/
				  isc_69.isc_81 || !/*X.RDB$FUNCTION_BLR.NULL*/
     isc_69.isc_80)
			{
				MemoryPool* const csb_pool = attachment->createPool();
				Jrd::ContextPoolHolder context(tdbb, csb_pool);

				try
				{
					ScratchBird::AutoPtr<CompilerScratch> csb(FB_NEW_POOL(*csb_pool) CompilerScratch(*csb_pool));

					if (!/*X.RDB$ENGINE_NAME.NULL*/
					     isc_69.isc_81)
					{
						ScratchBird::HalfStaticArray<UCHAR, 512> body;

						if (!/*X.RDB$FUNCTION_SOURCE.NULL*/
						     isc_69.isc_79)
						{
							bid function_source_bid;
							function_source_bid.bid_quad.bid_quad_high = /*X.RDB$FUNCTION_SOURCE*/
												     isc_69.isc_71.gds_quad_high;
							function_source_bid.bid_quad.bid_quad_low = /*X.RDB$FUNCTION_SOURCE*/
												    isc_69.isc_71.gds_quad_low;
							blb* const blob = blb::open(tdbb, sysTransaction, &function_source_bid);
							const ULONG len = blob->BLB_get_data(tdbb,
								body.getBuffer(blob->blb_length + 1), blob->blb_length + 1);
							body[MIN(blob->blb_length, len)] = 0;
						}
						else
							body.getBuffer(1)[0] = 0;

						dbb->dbb_extManager->makeFunction(tdbb, csb, function, /*X.RDB$ENGINE_NAME*/
												       isc_69.isc_98,
							(/*X.RDB$ENTRYPOINT.NULL*/
							 isc_69.isc_82 ? "" : /*X.RDB$ENTRYPOINT*/
	isc_69.isc_74), (char*) body.begin());

						if (!function->fun_external)
							function->setDefined(false);
					}
					else if (!/*X.RDB$FUNCTION_BLR.NULL*/
						  isc_69.isc_80)
					{
						const ScratchBird::string name = function->getName().toQuotedString();

						try
						{
							TraceFuncCompile trace(tdbb, name.c_str());

							bid function_blr_bid;
							function_blr_bid.bid_quad.bid_quad_high = /*X.RDB$FUNCTION_BLR*/
												  isc_69.isc_72.gds_quad_high;
							function_blr_bid.bid_quad.bid_quad_low = /*X.RDB$FUNCTION_BLR*/
												 isc_69.isc_72.gds_quad_low;
							bid debug_info_bid;
							if (!/*X.RDB$DEBUG_INFO.NULL*/
							     isc_69.isc_78) {
								debug_info_bid.bid_quad.bid_quad_high = /*X.RDB$DEBUG_INFO*/
													isc_69.isc_70.gds_quad_high;
								debug_info_bid.bid_quad.bid_quad_low = /*X.RDB$DEBUG_INFO*/
												       isc_69.isc_70.gds_quad_low;
							}
							function->parseBlr(tdbb, csb, &function_blr_bid,
								/*X.RDB$DEBUG_INFO.NULL*/
								isc_69.isc_78 ? NULL : &debug_info_bid);

							trace.finish(function->getStatement(), ITracePlugin::RESULT_SUCCESS);
						}
						catch (const ScratchBird::Exception& ex)
						{
							ScratchBird::StaticStatusVector temp_status;
							ex.stuffException(temp_status);
							(ScratchBird::Arg::Gds(isc_bad_fun_BLR) << ScratchBird::Arg::Str(name)
								<< ScratchBird::Arg::StatusVector(temp_status.begin())).raise();
						}
					}
				}
				catch (const ScratchBird::Exception&)
				{
					attachment->deletePool(csb_pool);
					throw;
				}

				fb_assert(!function->isDefined() || function->getStatement()->function == function);
			}
			else
			{
				ScratchBird::RefPtr<ScratchBird::MsgMetadata> inputMetadata(ScratchBird::REF_NO_INCR, createMetadata(function->getInputFields(), false));
				function->setInputFormat(createFormat(function->getPool(), inputMetadata, false));

				ScratchBird::RefPtr<ScratchBird::MsgMetadata> outputMetadata(ScratchBird::REF_NO_INCR, createMetadata(function->getOutputFields(), false));
				function->setOutputFormat(createFormat(function->getPool(), outputMetadata, true));

				function->setImplemented(false);
			}

			function->flags |= Routine::FLAG_SCANNED;

			if (!dbb->readOnly() &&
				!/*X.RDB$FUNCTION_BLR.NULL*/
				 isc_69.isc_80 &&
				!/*X.RDB$VALID_BLR.NULL*/
				 isc_69.isc_76 && /*X.RDB$VALID_BLR*/
    isc_69.isc_77 == FALSE)
			{
				// If the BLR was marked as invalid but the function was compiled,
				// mark the BLR as valid.

				/*MODIFY X USING*/
				{
					/*X.RDB$VALID_BLR*/
					isc_69.isc_77 = TRUE;
					/*X.RDB$VALID_BLR.NULL*/
					isc_69.isc_76 = FALSE;
				/*END_MODIFY*/
				isc_99.isc_100 = isc_69.isc_76;
				isc_99.isc_101 = isc_69.isc_77;
				isc_send (NULL, (FB_API_HANDLE*) &request_fun, (short) 2, (short) 4, &isc_99, (short) 0);
				}
			}
		}
		/*END_FOR*/
		   isc_send (NULL, (FB_API_HANDLE*) &request_fun, (short) 3, (short) 2, &isc_102, (short) 0);
		   }
		}
	}

	// Make sure that it is really being scanned
	fb_assert(function->flags & Routine::FLAG_BEING_SCANNED);

	function->flags &= ~Routine::FLAG_BEING_SCANNED;

	}	// try
	catch (const ScratchBird::Exception&)
	{
		function->flags &= ~(Routine::FLAG_BEING_SCANNED | Routine::FLAG_SCANNED);

		if (function->existenceLock)
		{
			LCK_release(tdbb, function->existenceLock);
			delete function->existenceLock;
			function->existenceLock = NULL;
		}

		throw;
	}

	return function;
}

int Function::blockingAst(void* ast_object)
{
	Function* const function = static_cast<Function*>(ast_object);

	try
	{
		Database* const dbb = function->existenceLock->lck_dbb;

		AsyncContextHolder tdbb(dbb, FB_FUNCTION, function->existenceLock);

		LCK_release(tdbb, function->existenceLock);
		function->flags |= Routine::FLAG_OBSOLETE;
	}
	catch (const ScratchBird::Exception&)
	{} // no-op

	return 0;
}

void Function::releaseLocks(thread_db* tdbb)
{
	if (existenceLock)
	{
		LCK_release(tdbb, existenceLock);
		flags |= Routine::FLAG_CHECK_EXISTENCE;
		useCount = 0;
	}
}

bool Function::checkCache(thread_db* tdbb) const
{
	return tdbb->getAttachment()->att_functions[getId()] == this;
}

void Function::clearCache(thread_db* tdbb)
{
	tdbb->getAttachment()->att_functions[getId()] = NULL;
}

bool Function::reload(thread_db* tdbb)
{
   struct isc_3_struct {
          ISC_QUAD isc_4;	/* RDB$DEBUG_INFO */
          ISC_QUAD isc_5;	/* RDB$FUNCTION_BLR */
          short isc_6;	/* isc_utility */
          short isc_7;	/* gds__null_flag */
          short isc_8;	/* gds__null_flag */
   } isc_3;
   struct isc_1_struct {
          short isc_2;	/* RDB$FUNCTION_ID */
   } isc_1;
	fb_assert(this->flags & Routine::FLAG_RELOAD);

	Attachment* attachment = tdbb->getAttachment();
	AutoCacheRequest request(tdbb, irq_l_funct_blr, IRQ_REQUESTS);

	/*FOR(REQUEST_HANDLE request)
		X IN RDB$FUNCTIONS
		WITH X.RDB$FUNCTION_ID EQ this->getId()*/
	{
	if (!request)
	   {
	   isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request, (short) sizeof(isc_0), (char*) isc_0);
	   }
	isc_1.isc_2 = this->getId();
	isc_start_and_send (NULL, (FB_API_HANDLE*) &request, (FB_API_HANDLE*) &gds_trans, (short) 0, (short) 2, &isc_1, (short) 0);
	while (1)
	   {
	   isc_receive (NULL, (FB_API_HANDLE*) &request, (short) 1, (short) 22, &isc_3, (short) 0);
	   if (!isc_3.isc_6) break;
	{
		if (/*X.RDB$FUNCTION_BLR.NULL*/
		    isc_3.isc_8)
			continue;

		MemoryPool* const csb_pool = attachment->createPool();
		Jrd::ContextPoolHolder context(tdbb, csb_pool);

		try
		{
			ScratchBird::AutoPtr<CompilerScratch> csb(FB_NEW_POOL(*csb_pool) CompilerScratch(*csb_pool));

			try
			{
				bid function_blr_bid;
				function_blr_bid.bid_quad.bid_quad_high = /*X.RDB$FUNCTION_BLR*/
									  isc_3.isc_5.gds_quad_high;
				function_blr_bid.bid_quad.bid_quad_low = /*X.RDB$FUNCTION_BLR*/
									 isc_3.isc_5.gds_quad_low;
				bid debug_info_bid;
				if (!/*X.RDB$DEBUG_INFO.NULL*/
				     isc_3.isc_7) {
					debug_info_bid.bid_quad.bid_quad_high = /*X.RDB$DEBUG_INFO*/
										isc_3.isc_4.gds_quad_high;
					debug_info_bid.bid_quad.bid_quad_low = /*X.RDB$DEBUG_INFO*/
									       isc_3.isc_4.gds_quad_low;
				}
				this->parseBlr(tdbb, csb, &function_blr_bid,
					/*X.RDB$DEBUG_INFO.NULL*/
					isc_3.isc_7 ? NULL : &debug_info_bid);

				// parseBlr() above could set FLAG_RELOAD again
				return !(this->flags & Routine::FLAG_RELOAD);
			}
			catch (const ScratchBird::Exception& ex)
			{
				ScratchBird::StaticStatusVector temp_status;
				ex.stuffException(temp_status);

				const ScratchBird::string name = this->getName().toQuotedString();
				(ScratchBird::Arg::Gds(isc_bad_fun_BLR) << ScratchBird::Arg::Str(name)
					<< ScratchBird::Arg::StatusVector(temp_status.begin())).raise();
			}
		}
		catch (const ScratchBird::Exception&)
		{
			attachment->deletePool(csb_pool);
			throw;
		}
	}
	/*END_FOR*/
	   }
	}

	return false;
}
