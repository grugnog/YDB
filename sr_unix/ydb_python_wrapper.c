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
#include "libyottadb_int.h"
#include <string.h>
#include <stdio.h>

GBLREF command_qualifier	cmd_qlf;
GBLREF unsigned char		source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF boolean_t		ydb_init_complete;
GBLREF mident			routine_name, module_name, int_module_name;

/* External declaration of functions from other modules */
void op_fnzycompile(mval *string, mval *ret);
boolean_t compiler_startup(void);
int ydb_init(void);

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
	
	/* Initialize YottaDB if not already initialized */
	if (!ydb_init_complete) {
		status = ydb_init();
		if (status != 0) {
			snprintf(error_out, error_len, "YottaDB initialization failed with code %d", status);
			return -1;
		}
	}
	
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

/*
 * Python wrapper function to parse a MUMPS file and generate AST JSON.
 * This function uses the full compiler pipeline and properly handles multi-line code.
 * 
 * Parameters:
 *   mumps_filename   - Path to MUMPS source file (.m file)
 *   json_filename_out - Buffer to receive the generated JSON filename
 *   filename_len     - Size of json_filename_out buffer
 *   error_out        - Buffer to receive error message if compilation fails
 *   error_len        - Size of error_out buffer
 * 
 * Returns:
 *   0 on success (JSON file created)
 *   Non-zero on error (error message in error_out)
 */
int ydb_parse_mumps_file_to_json(
	const char *mumps_filename,
	char *json_filename_out,
	size_t filename_len,
	char *error_out,
	size_t error_len
)
{
	command_qualifier saved_cmd_qlf;
	boolean_t compile_success;
	char base_name[256];
	char *dot_pos, *slash_pos;
	int status = 0;
	
	/* Validate input parameters */
	if (!mumps_filename || !json_filename_out || filename_len == 0 || !error_out || error_len == 0) {
		if (error_out && error_len > 0) {
			snprintf(error_out, error_len, "Invalid parameters passed to ydb_parse_mumps_file_to_json");
		}
		return -1;
	}
	
	/* Clear output buffers */
	json_filename_out[0] = '\0';
	error_out[0] = '\0';
	
	/* Initialize YottaDB if not already initialized */
	if (!ydb_init_complete) {
		status = ydb_init();
		if (status != 0) {
			snprintf(error_out, error_len, "YottaDB initialization failed with code %d", status);
			return -1;
		}
	}
	
	/* Extract base filename for JSON output */
	strncpy(base_name, mumps_filename, sizeof(base_name) - 1);
	base_name[sizeof(base_name) - 1] = '\0';
	
	/* Remove directory path if present */
	slash_pos = strrchr(base_name, '/');
	if (slash_pos != NULL) {
		memmove(base_name, slash_pos + 1, strlen(slash_pos + 1) + 1);
	}
	
	/* Remove extension if present */
	dot_pos = strrchr(base_name, '.');
	if (dot_pos != NULL) {
		*dot_pos = '\0';
	}
	
	/* Set source file name for compiler */
	source_name_len = strlen(mumps_filename);
	if (source_name_len > MAX_FN_LEN) {
		snprintf(error_out, error_len, "Filename too long: %s", mumps_filename);
		return -1;
	}
	memcpy(source_file_name, mumps_filename, source_name_len + 1);
	
	/* Set routine and module names from base_name */
	routine_name.len = MIN(MAX_MIDENT_LEN, strlen(base_name));
	memcpy(routine_name.addr, base_name, routine_name.len);
	memcpy(module_name.addr, routine_name.addr, routine_name.len);
	if ('_' == *routine_name.addr)
		routine_name.addr[0] = '%';
	module_name.len = int_module_name.len = routine_name.len;
	memcpy(int_module_name.addr, routine_name.addr, routine_name.len);
	
	/* Save current command qualifiers */
	saved_cmd_qlf = cmd_qlf;
	
	/* Enable AST JSON dump and disable object file generation */
	cmd_qlf.qlf |= CQ_DUMP_AST_JSON;
	cmd_qlf.qlf &= ~CQ_OBJECT;
	
	/* Call the compiler - it returns TRUE on error, FALSE on success */
	compile_success = compiler_startup();
	
	/* Check if compilation succeeded (FALSE means success) */
	if (compile_success) {
		/* TRUE means there were errors */
		snprintf(error_out, error_len, "Compilation failed for file: %s", mumps_filename);
		status = -2;
	} else {
		/* FALSE means success - construct full path to JSON file */
		char *dir_end = strrchr(mumps_filename, '/');
		if (dir_end != NULL) {
			/* File is in a directory - prepend the directory path */
			int dir_len = dir_end - mumps_filename + 1;
			snprintf(last_json_filename, sizeof(last_json_filename), "%.*s%s_ast.json", dir_len, mumps_filename, base_name);
		} else {
			/* File is in current directory */
			snprintf(last_json_filename, sizeof(last_json_filename), "%s_ast.json", base_name);
		}
		strncpy(json_filename_out, last_json_filename, filename_len - 1);
		json_filename_out[filename_len - 1] = '\0';
		status = 0;
	}
	
	/* Restore command qualifiers */
	cmd_qlf = saved_cmd_qlf;
	
	return status;
}
