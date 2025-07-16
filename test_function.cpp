/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/***************** gpre version SB-T0.6.0.1-dev ScratchBird 0.6 f90eae0 **********************/
#include "firebird.h"
using namespace ScratchBird;

/*DATABASE DB = FILENAME "test.fdb";*/
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
   isc_0l = 141;
static const unsigned char
   isc_0 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_relation, 11, 'R','D','B','$','S','C','H','E','M','A','S', 1, 
		  blr_relation, 13, 'R','D','B','$','F','U','N','C','T','I','O','N','S', 0, 
		  blr_boolean, 
		     blr_and, 
			blr_eql, 
			   blr_field, 0, 15, 'R','D','B','$','F','U','N','C','T','I','O','N','_','I','D', 
			   blr_literal, blr_long, 0, 1,0,0,0,
			blr_eql, 
			   blr_field, 1, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
			   blr_field, 0, 15, 'R','D','B','$','S','C','H','E','M','A','_','N','A','M','E', 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request isc_0 */


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


int test_function()
{
   struct isc_1_struct {
          short isc_2;	/* isc_utility */
   } isc_1;
    /*FOR(REQUEST_HANDLE request)
        X IN RDB$FUNCTIONS
        CROSS SCH IN RDB$SCHEMAS
        WITH X.RDB$FUNCTION_ID EQ 1 AND
             SCH.RDB$SCHEMA_NAME EQ X.RDB$SCHEMA_NAME*/
    {
    if (!request)
       {
       isc_compile_request (NULL, (FB_API_HANDLE*) &DB, (FB_API_HANDLE*) &request, (short) sizeof(isc_0), (char*) isc_0);
       }
    isc_start_request (NULL, (FB_API_HANDLE*) &request, (FB_API_HANDLE*) &gds_trans, (short) 0);
    while (1)
       {
       isc_receive (NULL, (FB_API_HANDLE*) &request, (short) 0, (short) 2, &isc_1, (short) 0);
       if (!isc_1.isc_2) break;
    {
        // Do something
    }
    /*END_FOR*/
       }
    }
    
    return 0;
}