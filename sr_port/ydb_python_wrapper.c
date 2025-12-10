/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "cmd_qlf.h"
#include "op.h"
#include "mvalconv.h"
#include "stringpool.h"
#include "ast_dump_json.h"
#include "ydb_python_wrapper.h"
#include <string.h>
#include <stdio.h>

GBLREF command_qualifier	cmd_qlf;
GBLREF unsigned char		source_file_name[];
GBLREF unsigned short		source_name_len;

/* External declaration of op_fnzycompile from op_fnzycompile.c */
void op_fnzycompile(mval *string, mval *ret);

/* Global variable to store the last generated JSON filename */
static char last_json_filename[1024] = {0};

/* 
 * Python wrapper function to parse MUMPS code and generate AST JSON.
 * 
 * Parameters:
 *   code             - NULL-terminated string containing MUMPS code to parse
 *   json_filename_out - Buffer to receive the generated JSON filename
 *   filename_len     - Size of json_filename_out buffer
 *   error_out        - Buffer to receive error message if compilation fails
 *   error_len        - Size of error_out buffer
 * 
 * Returns:
 *   0 on success (JSON file created)
 *   Non-zero on error (error message in error_out)
 */
int ydb_parse_mumps_to_json(
	const char *code,
	char *json_filename_out,
	size_t filename_len,
	char *error_out,
	size_t error_len
)
{
	mval code_mval = {0};
	mval result_mval = {0};
	command_qualifier saved_cmd_qlf;
	char base_name[256];
	char *dot_pos;
	int status = 0;
	
	/* Validate input parameters */
	if (!code || !json_filename_out || filename_len == 0 || !error_out || error_len == 0) {
		if (error_out && error_len > 0) {
			snprintf(error_out, error_len, "Invalid parameters passed to ydb_parse_mumps_to_json");
		}
		return -1;
	}
	
	/* Clear output buffers */
	json_filename_out[0] = '\0';
	error_out[0] = '\0';
	
	/* Save current command qualifiers */
	saved_cmd_qlf = cmd_qlf;
	
	/* Enable AST JSON dump */
	cmd_qlf.qlf |= CQ_DUMP_AST_JSON;
	
	/* Initialize AST dumping */
	ast_dump_json_init();
	
	/* Set up mval for code string */
	code_mval.mvtype = MV_STR;
	code_mval.str.addr = (char *)code;
	code_mval.str.len = strlen(code);
	
	/* Call the MUMPS compiler function */
	op_fnzycompile(&code_mval, &result_mval);
	
	/* Complete AST dump (writes JSON file) */
	ast_dump_json_complete();
	
	/* Check if compilation succeeded */
	if (result_mval.mvtype & MV_STR && result_mval.str.len > 0) {
		/* Compilation failed - result contains error message */
		size_t copy_len = (result_mval.str.len < error_len - 1) ? result_mval.str.len : error_len - 1;
		memcpy(error_out, result_mval.str.addr, copy_len);
		error_out[copy_len] = '\0';
		status = -2;
	} else {
		/* Success - determine JSON filename */
		if (source_name_len > 0 && source_name_len < sizeof(base_name) - 10) {
			/* Use source filename as base */
			strncpy(base_name, (char *)source_file_name, sizeof(base_name) - 1);
			base_name[sizeof(base_name) - 1] = '\0';
			
			/* Remove extension if present */
			dot_pos = strrchr(base_name, '.');
			if (dot_pos != NULL) {
				*dot_pos = '\0';
			}
			
			snprintf(last_json_filename, sizeof(last_json_filename), "%s_ast.json", base_name);
		} else {
			/* Default filename */
			strcpy(last_json_filename, "mumps_ast.json");
		}
		
		/* Copy filename to output buffer */
		strncpy(json_filename_out, last_json_filename, filename_len - 1);
		json_filename_out[filename_len - 1] = '\0';
		status = 0;
	}
	
	/* Cleanup */
	ast_dump_json_cleanup();
	
	/* Restore command qualifiers */
	cmd_qlf = saved_cmd_qlf;
	
	return status;
}
