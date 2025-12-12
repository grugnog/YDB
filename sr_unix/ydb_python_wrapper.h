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

#ifndef YDB_PYTHON_WRAPPER_H
#define YDB_PYTHON_WRAPPER_H

#include <stddef.h>

/* 
 * Python wrapper function to parse MUMPS code and generate AST JSON.
 * 
 * This function compiles MUMPS code from a string and generates a JSON
 * representation of the Abstract Syntax Tree (AST). The JSON file is
 * written to disk and the filename is returned to the caller.
 * 
 * Parameters:
 *   code             - NULL-terminated string containing MUMPS code to parse
 *   json_filename_out - Buffer to receive the generated JSON filename
 *   filename_len     - Size of json_filename_out buffer
 *   error_out        - Buffer to receive error message if compilation fails
 *   error_len        - Size of error_out buffer
 * 
 * Returns:
 *   0 on success (JSON file created, filename in json_filename_out)
 *   -1 if invalid parameters
 *   -2 if compilation failed (error message in error_out)
 * 
 * Example usage:
 *   char filename[1024];
 *   char error[4096];
 *   int result = ydb_parse_mumps_to_json(
 *       "hello() ; comment\n write \"Hello\",!\n quit\n",
 *       filename, sizeof(filename),
 *       error, sizeof(error)
 *   );
 *   if (result == 0) {
 *       // Success - read JSON from file in 'filename'
 *   } else {
 *       // Error - message in 'error'
 *   }
 */
int ydb_parse_mumps_to_json(
	const char *code,
	char *json_filename_out,
	size_t filename_len,
	char *error_out,
	size_t error_len
);

/*
 * Python wrapper function to parse a MUMPS file and generate AST JSON.
 * 
 * This function compiles a MUMPS source file (.m file) using the full
 * compiler pipeline and generates a JSON representation of the AST.
 * This properly handles multi-line MUMPS routines.
 * 
 * Parameters:
 *   mumps_filename   - Path to MUMPS source file (.m file)
 *   json_filename_out - Buffer to receive the generated JSON filename
 *   filename_len     - Size of json_filename_out buffer
 *   error_out        - Buffer to receive error message if compilation fails
 *   error_len        - Size of error_out buffer
 * 
 * Returns:
 *   0 on success (JSON file created, filename in json_filename_out)
 *   -1 if invalid parameters
 *   -2 if compilation failed (error message in error_out)
 * 
 * Example usage:
 *   char filename[1024];
 *   char error[4096];
 *   int result = ydb_parse_mumps_file_to_json(
 *       "/path/to/routine.m",
 *       filename, sizeof(filename),
 *       error, sizeof(error)
 *   );
 */
int ydb_parse_mumps_file_to_json(
	const char *mumps_filename,
	char *json_filename_out,
	size_t filename_len,
	char *error_out,
	size_t error_len
);

#endif /* YDB_PYTHON_WRAPPER_H */
