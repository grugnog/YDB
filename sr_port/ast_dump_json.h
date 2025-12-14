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

#ifndef AST_DUMP_JSON_H
#define AST_DUMP_JSON_H

#include "compiler.h"

/* Function prototypes for AST JSON dumping */
void ast_dump_json_init(void);
void ast_dump_json_complete(void);
void ast_dump_json_cleanup(void);

/* Pattern source string storage - maps OC_LIT triples to their pattern source strings */
void ast_dump_register_pattern_source(triple *lit_triple, const char *pattern_src, int pattern_len);
const char *ast_dump_get_pattern_source(triple *lit_triple, int *len_out);

#endif /* AST_DUMP_JSON_H */
