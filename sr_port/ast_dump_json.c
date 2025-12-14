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
#include "mvalconv.h"
#include "opcode.h"
#include "cmd_qlf.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ast_dump_json.h"

GBLREF triple		t_orig;
GBLREF command_qualifier	cmd_qlf;
GBLREF unsigned char	source_file_name[];
GBLREF unsigned short	source_name_len;

LITREF char *oc_tab_graphic[];

static FILE *ast_json_file = NULL;
static int indent_level = 0;

/* Triple ID mapping for removing memory addresses */
typedef struct triple_map_entry {
	triple *trip_ptr;
	int triple_id;
} triple_map_entry;

static triple_map_entry *triple_id_map = NULL;
static int triple_count = 0;
static int next_triple_id = 1;

/* Pattern source string mapping - associates OC_LIT triples with their original pattern source */
typedef struct pattern_source_entry {
	triple *lit_triple;
	char *pattern_src;
	int pattern_len;
} pattern_source_entry;

#define MAX_PATTERN_SOURCES 256
static pattern_source_entry pattern_source_map[MAX_PATTERN_SOURCES];
static int pattern_source_count = 0;

/* Forward declarations */
static void dump_triple_json(triple *trip, boolean_t is_last, int triple_id);
static void dump_operand_json(oprtype *opr, boolean_t is_last);
static void write_indent(void);
static const char* opcode_to_string(opctype opcode);
static const char* oprclass_to_string(operclass class);
static void build_triple_id_map(void);
static int get_triple_id(triple *trip);
static void cleanup_triple_id_map(void);
static void cleanup_pattern_source_map(void);

/* Initialize JSON AST dumping */
void ast_dump_json_init(void)
{
	if (!(cmd_qlf.qlf & CQ_DUMP_AST_JSON)) {
		return;
	}
	
	/* Just mark that AST dumping was requested */
	ast_json_file = NULL;
}void ast_dump_json_complete(void)
{
	triple *trip;
	int count = 0;
	char json_filename[1024];
	char base_name_copy[256];
	char *dot_pos;
	
	if (!(cmd_qlf.qlf & CQ_DUMP_AST_JSON)) {
		return;
	}

	/* Create JSON filename based on source filename */
	if (source_name_len > 0 && source_name_len < (sizeof(base_name_copy) - 10)) {
		/* Make a copy to avoid modifying the original */
		strncpy(base_name_copy, (char*)source_file_name, sizeof(base_name_copy)-1);
		base_name_copy[sizeof(base_name_copy)-1] = '\0';
		
		dot_pos = strrchr(base_name_copy, '.');
		if (dot_pos != NULL) {
			*dot_pos = '\0';  /* Null terminate at the dot */
		}
		snprintf(json_filename, sizeof(json_filename), "%s_ast.json", base_name_copy);
	} else {
		strcpy(json_filename, "mumps_ast.json");
	}
	
	/* Open the file for writing */
	ast_json_file = fopen(json_filename, "w");
	if (!ast_json_file) {
		printf("Warning: Could not create AST JSON file %s\n", json_filename);
		return;
	}
	
	/* Safety check for t_orig */
	if (!t_orig.exorder.fl || !t_orig.exorder.bl) {
		if (fprintf(ast_json_file, "{\n  \"error\": \"Invalid triple chain\",\n") < 0 ||
		    fprintf(ast_json_file, "  \"ast_type\": \"MUMPS\",\n") < 0 ||
		    fprintf(ast_json_file, "  \"triples\": []\n}\n") < 0) {
			printf("Warning: Failed to write to AST JSON file\n");
		}
		fclose(ast_json_file);
		ast_json_file = NULL;
		return;
	}

	/* Build the triple ID mapping first */
	build_triple_id_map();

	if (fprintf(ast_json_file, "{\n") < 0) {
		printf("Warning: Failed to write to AST JSON file\n");
		fclose(ast_json_file);
		ast_json_file = NULL;
		return;
	}
	
	indent_level = 1;
	
	write_indent();
	if (fprintf(ast_json_file, "\"ast_type\": \"MUMPS\",\n") < 0) {
		printf("Warning: Failed to write to AST JSON file\n");
		fclose(ast_json_file);
		ast_json_file = NULL;
		return;
	}
	
	write_indent();
	if (fprintf(ast_json_file, "\"source_file\": \"%.*s\",\n", source_name_len, source_file_name) < 0) {
		printf("Warning: Failed to write to AST JSON file\n");
		fclose(ast_json_file);
		ast_json_file = NULL;
		return;
	}
	
	write_indent();
	if (fprintf(ast_json_file, "\"triples\": [\n") < 0) {
		printf("Warning: Failed to write to AST JSON file\n");
		fclose(ast_json_file);
		ast_json_file = NULL;
		return;
	}
	
	indent_level = 2;
	
	/* Count triples first to know when we hit the last one */
	dqloop(&t_orig, exorder, trip) {
		if (trip) count++;
	}
	
	/* Now dump all triples */
	int current = 0;
	dqloop(&t_orig, exorder, trip) {
		if (trip) {
			current++;
			/* Add safety check for the triple itself */
			if ((void*)trip < (void*)0x1000) {
				fprintf(ast_json_file, "    {\n");
				fprintf(ast_json_file, "      \"error\": \"Invalid triple pointer\",\n");
				fprintf(ast_json_file, "      \"triple_id\": %d\n", current);
				fprintf(ast_json_file, "    }%s\n", (current == count) ? "" : ",");
				continue;
			}
			dump_triple_json(trip, (current == count), get_triple_id(trip));
		}
	}
	
	indent_level = 1;
	write_indent();
	fprintf(ast_json_file, "]\n");
	fprintf(ast_json_file, "}\n");
	
	/* Clean up */
	cleanup_triple_id_map();
	cleanup_pattern_source_map();
	fclose(ast_json_file);
	ast_json_file = NULL;
}

/* Clean up if needed */
void ast_dump_json_cleanup(void)
{
	cleanup_triple_id_map();
	cleanup_pattern_source_map();
	if (ast_json_file) {
		fclose(ast_json_file);
		ast_json_file = NULL;
	}
}

/* Dump a single triple as JSON */
static void dump_triple_json(triple *trip, boolean_t is_last, int triple_id)
{
	const char *pattern_src;
	int pattern_len;
	
	if (!ast_json_file || !trip)
		return;
		
	write_indent();
	fprintf(ast_json_file, "{\n");
	indent_level++;
	
	write_indent();
	fprintf(ast_json_file, "\"triple_id\": %d,\n", triple_id);
	
	write_indent();
	fprintf(ast_json_file, "\"opcode\": \"%s\",\n", opcode_to_string(trip->opcode));
	
	write_indent();
	fprintf(ast_json_file, "\"opcode_value\": %d,\n", (int)trip->opcode);
	
	write_indent();
	fprintf(ast_json_file, "\"source_line\": %u,\n", trip->src.line);
	
	write_indent();
	fprintf(ast_json_file, "\"source_column\": %u,\n", trip->src.column);
	
	write_indent();
	fprintf(ast_json_file, "\"rtaddr\": %d,\n", trip->rtaddr);
	
	/* Check if this OC_LIT triple has a pattern source string registered */
	pattern_src = ast_dump_get_pattern_source(trip, &pattern_len);
	if (pattern_src && pattern_len > 0) {
		write_indent();
		fprintf(ast_json_file, "\"pattern_string\": \"");
		/* Escape JSON special characters in the pattern string */
		for (int i = 0; i < pattern_len; i++) {
			char c = pattern_src[i];
			switch (c) {
				case '"':  fprintf(ast_json_file, "\\\""); break;
				case '\\': fprintf(ast_json_file, "\\\\"); break;
				case '\n': fprintf(ast_json_file, "\\n"); break;
				case '\r': fprintf(ast_json_file, "\\r"); break;
				case '\t': fprintf(ast_json_file, "\\t"); break;
				default:
					if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) {
						fprintf(ast_json_file, "\\u%04x", (unsigned char)c);
					} else {
						fprintf(ast_json_file, "%c", c);
					}
					break;
			}
		}
		fprintf(ast_json_file, "\",\n");
	}
	
	/* Operands */
	write_indent();
	fprintf(ast_json_file, "\"operands\": [\n");
	indent_level++;
	
	dump_operand_json(&trip->operand[0], FALSE);
	fprintf(ast_json_file, ",\n");
	dump_operand_json(&trip->operand[1], TRUE);
	fprintf(ast_json_file, "\n");
	
	indent_level--;
	write_indent();
	fprintf(ast_json_file, "],\n");
	
	/* Destination */
	write_indent();
	if (fprintf(ast_json_file, "\"destination\": ") < 0) return;
	fflush(ast_json_file);
	dump_operand_json(&trip->destination, TRUE);
	if (fprintf(ast_json_file, "\n") < 0) return;
	fflush(ast_json_file);
	
	indent_level--;
	write_indent();
	if (fprintf(ast_json_file, "}%s\n", is_last ? "" : ",") < 0) return;
	fflush(ast_json_file);
}

/* Dump an operand as JSON */
static void dump_operand_json(oprtype *opr, boolean_t is_last)
{
	if (!ast_json_file || !opr)
		return;
		
	write_indent();
	if (fprintf(ast_json_file, "{\n") < 0) return;
	fflush(ast_json_file);
	indent_level++;
	
	write_indent();
	if (fprintf(ast_json_file, "\"class\": \"%s\",\n", oprclass_to_string(opr->oprclass)) < 0) return;
	fflush(ast_json_file);
	
	write_indent();
	if (fprintf(ast_json_file, "\"class_value\": %d,\n", (int)opr->oprclass) < 0) return;
	fflush(ast_json_file);
	
	write_indent();
	/* Use different field names for different operand types for clarity */
	switch (opr->oprclass) {
		case TRIP_REF:
		case TNXT_REF:
		case TJMP_REF:
			if (fprintf(ast_json_file, "\"target_triple_id\": ") < 0) return;
			break;
		case MVAR_REF:
			if (fprintf(ast_json_file, "\"variable_name\": ") < 0) return;
			break;
		case MLIT_REF:
			if (fprintf(ast_json_file, "\"literal_value\": ") < 0) return;
			break;
		case MNXL_REF:
			if (fprintf(ast_json_file, "\"line_ref\": ") < 0) return;
			break;
		case MFUN_REF:
			if (fprintf(ast_json_file, "\"func_ref\": ") < 0) return;
			break;
		case CDIDX_REF:
			if (fprintf(ast_json_file, "\"routine_or_label\": ") < 0) return;
			break;
		default:
			if (fprintf(ast_json_file, "\"value\": ") < 0) return;
			break;
	}
	fflush(ast_json_file);
	
	switch (opr->oprclass) {
		case NO_REF:
			if (fprintf(ast_json_file, "null") < 0) return;
			break;
		case TRIP_REF:
		case TNXT_REF:
		case TJMP_REF:
			{
				int target_id = get_triple_id(opr->oprval.tref);
				if (target_id > 0) {
					if (fprintf(ast_json_file, "%d", target_id) < 0) return;
				} else {
					if (fprintf(ast_json_file, "null") < 0) return;
				}
			}
			break;
		case ILIT_REF:
			if (fprintf(ast_json_file, "%d", opr->oprval.ilit) < 0) return;
			break;
		case MLIT_REF:
			{
				/* 
				 * MLIT_REF contains a pointer to an mliteral structure. However, during AST dump
				 * the literal memory may not be safely accessible (the literal chain may have been
				 * freed or reused). To avoid segfaults, we output a placeholder with the pointer
				 * address rather than trying to dereference the literal value.
				 */
				mliteral *mlit = opr->oprval.mlit;
				if (mlit) {
					if (fprintf(ast_json_file, "\"<literal@%p>\"", (void *)mlit) < 0) return;
				} else {
					if (fprintf(ast_json_file, "null") < 0) return;
				}
			}
			break;
		case MNXL_REF:
			/* MNXL_REF refers to a mline - we don't have a name for it */
			if (fprintf(ast_json_file, "\"<line_ref>\"") < 0) return;
			break;
		case MFUN_REF:
			/* MFUN_REF uses oprval.lab which may not be valid during AST dump */
			/* For now, use a placeholder - extracting the label name needs more investigation */
			if (fprintf(ast_json_file, "\"<func_ref>\"") < 0) return;
			break;
		case CDIDX_REF:
			{
				/* CDIDX_REF uses oprval.cdidx which is an mstr* containing the routine/label name */
				mstr *cdidx = opr->oprval.cdidx;
				if (cdidx && cdidx->addr && cdidx->len > 0) {
					if (fprintf(ast_json_file, "\"") < 0) return;
					for (int i = 0; i < cdidx->len; i++) {
						char c = cdidx->addr[i];
						switch (c) {
							case '"':  if (fprintf(ast_json_file, "\\\"") < 0) return; break;
							case '\\': if (fprintf(ast_json_file, "\\\\") < 0) return; break;
							default:   if (fprintf(ast_json_file, "%c", c) < 0) return; break;
						}
					}
					if (fprintf(ast_json_file, "\"") < 0) return;
				} else {
					if (fprintf(ast_json_file, "\"\"") < 0) return;
				}
			}
			break;
		case MVAR_REF:
			{
				mvar *mv = opr->oprval.vref;
				if (mv && mv->mvname.addr && mv->mvname.len > 0) {
					const char *scope;
					
					/* Determine scope based on variable name prefix */
					if (mv->mvname.len > 0) {
						char first_char = mv->mvname.addr[0];
						if (first_char == '^') {
							scope = "global";
						} else if (first_char == '$') {
							scope = "intrinsic"; 
						} else {
							scope = "local";
						}
					} else {
						scope = "unknown";
					}
					
					/* Output variable name */
					if (fprintf(ast_json_file, "\"") < 0) return;
					/* Escape JSON special characters in the variable name */
					for (int i = 0; i < mv->mvname.len; i++) {
						char c = mv->mvname.addr[i];
						switch (c) {
							case '"':  if (fprintf(ast_json_file, "\\\"") < 0) return; break;
							case '\\': if (fprintf(ast_json_file, "\\\\") < 0) return; break;
							case '\n': if (fprintf(ast_json_file, "\\n") < 0) return; break;
							case '\r': if (fprintf(ast_json_file, "\\r") < 0) return; break;
							case '\t': if (fprintf(ast_json_file, "\\t") < 0) return; break;
                                                        default:
                                                            if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) {
                                                                if (fprintf(ast_json_file, "\\u%04x", (unsigned char)c) < 0) return;
                                                            } else {
                                                                if (fprintf(ast_json_file, "%c", c) < 0) return;
                                                            }
                                                            break;
						}
					}
					if (fprintf(ast_json_file, "\",\n") < 0) return;
					fflush(ast_json_file);
					write_indent();
					if (fprintf(ast_json_file, "\"scope\": \"%s\"", scope) < 0) return;
				} else {
					if (fprintf(ast_json_file, "\"<unknown>\",\n") < 0) return;
					fflush(ast_json_file);
					write_indent();
					if (fprintf(ast_json_file, "\"scope\": \"unknown\"") < 0) return;
				}
			}
			break;
		case MLAB_REF:
			{
				mlabel *mlab = opr->oprval.lab;
				if (mlab && mlab->mvname.addr && mlab->mvname.len > 0) {
					/* Output label name */
					if (fprintf(ast_json_file, "\"") < 0) return;
					/* Escape JSON special characters in the label name */
					for (int i = 0; i < mlab->mvname.len; i++) {
						char c = mlab->mvname.addr[i];
						switch (c) {
							case '"':  if (fprintf(ast_json_file, "\\\"") < 0) return; break;
							case '\\': if (fprintf(ast_json_file, "\\\\") < 0) return; break;
							case '\n': if (fprintf(ast_json_file, "\\n") < 0) return; break;
							case '\r': if (fprintf(ast_json_file, "\\r") < 0) return; break;
							case '\t': if (fprintf(ast_json_file, "\\t") < 0) return; break;
							default:
								if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F) {
									if (fprintf(ast_json_file, "\\u%04x", (unsigned char)c) < 0) return;
								} else {
									if (fprintf(ast_json_file, "%c", c) < 0) return;
								}
								break;
						}
					}
					if (fprintf(ast_json_file, "\"") < 0) return;
				} else {
					/* Fallback for NULL or empty label */
					if (fprintf(ast_json_file, "\"<label>\"") < 0) return;
				}
			}
			break;
		case INDR_REF:
			/* For now, use a placeholder for indirect references */
			if (fprintf(ast_json_file, "\"<indirect>\"") < 0) return;
			break;
		case CDLT_REF:
			/* For now, use a placeholder for code literal references */
			if (fprintf(ast_json_file, "\"<code_literal>\"") < 0) return;
			break;
		case TEMP_REF:
		case TVAR_REF:
		case TVAD_REF:
		case TCAD_REF:
		case TVAL_REF:
		case TSIZ_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
		case OCNT_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.offset) < 0) return;
			break;
		default:
			if (fprintf(ast_json_file, "\"unknown_type\"") < 0) return;
			break;
	}
	
	fflush(ast_json_file);
	if (fprintf(ast_json_file, "\n") < 0) return;
	fflush(ast_json_file);
	indent_level--;
	write_indent();
	if (fprintf(ast_json_file, "}") < 0) return;
	fflush(ast_json_file);
}

/* Write indentation */
static void write_indent(void)
{
	int i;
	for (i = 0; i < indent_level; i++) {
		fprintf(ast_json_file, "  ");
	}
}

/* Convert opcode to string */
static const char* opcode_to_string(opctype opcode)
{
	if (opcode >= 0 && opcode < OPCODE_COUNT && oc_tab_graphic[opcode]) {
		return oc_tab_graphic[opcode];
	}
	return "UNKNOWN_OPCODE";
}

/* Convert operand class to string */
static const char* oprclass_to_string(operclass class)
{
	switch (class) {
		case NO_REF: return "NO_REF";
		case TRIP_REF: return "TRIP_REF";
		case TNXT_REF: return "TNXT_REF";
		case ILIT_REF: return "ILIT_REF";
		case MLIT_REF: return "MLIT_REF";
		case MVAR_REF: return "MVAR_REF";
		case MLAB_REF: return "MLAB_REF";
		case MNXL_REF: return "MNXL_REF";
		case MFUN_REF: return "MFUN_REF";
		case TJMP_REF: return "TJMP_REF";
		case INDR_REF: return "INDR_REF";
		case CDIDX_REF: return "CDIDX_REF";
		case CDLT_REF: return "CDLT_REF";
		case TEMP_REF: return "TEMP_REF";
		case TVAR_REF: return "TVAR_REF";
		case TVAD_REF: return "TVAD_REF";
		case TCAD_REF: return "TCAD_REF";
		case TVAL_REF: return "TVAL_REF";
		case TSIZ_REF: return "TSIZ_REF";
		case OCNT_REF: return "OCNT_REF";
		default: return "UNKNOWN_CLASS";
	}
}

/* Build a mapping from triple pointers to sequential IDs */
static void build_triple_id_map(void)
{
	triple *trip;
	int i = 0;
	
	/* First count the triples */
	triple_count = 0;
	dqloop(&t_orig, exorder, trip) {
		if (trip) triple_count++;
	}
	
	if (triple_count == 0) return;
	
	/* Allocate the mapping array */
	triple_id_map = (triple_map_entry*)malloc(triple_count * sizeof(triple_map_entry));
	if (!triple_id_map) return;
	
	/* Build the mapping */
	next_triple_id = 1;
	dqloop(&t_orig, exorder, trip) {
		if (trip && i < triple_count) {
			triple_id_map[i].trip_ptr = trip;
			triple_id_map[i].triple_id = next_triple_id++;
			i++;
		}
	}
}

/* Get the triple ID for a given triple pointer */
static int get_triple_id(triple *trip)
{
	int i;
	
	if (!trip || !triple_id_map) return -1;
	
	for (i = 0; i < triple_count; i++) {
		if (triple_id_map[i].trip_ptr == trip) {
			return triple_id_map[i].triple_id;
		}
	}
	return -1;
}

/* Clean up the triple ID mapping */
static void cleanup_triple_id_map(void)
{
	if (triple_id_map) {
		free(triple_id_map);
		triple_id_map = NULL;
	}
	triple_count = 0;
	next_triple_id = 1;
}

/* Register a pattern source string for an OC_LIT triple */
void ast_dump_register_pattern_source(triple *lit_triple, const char *pattern_src, int pattern_len)
{
	int i;
	
	if (!lit_triple || !pattern_src || pattern_len <= 0)
		return;
	
	if (pattern_source_count >= MAX_PATTERN_SOURCES)
		return;  /* Too many patterns - silently ignore */
	
	/* Check if already registered */
	for (i = 0; i < pattern_source_count; i++) {
		if (pattern_source_map[i].lit_triple == lit_triple)
			return;  /* Already registered */
	}
	
	/* Allocate and copy the pattern source string */
	pattern_source_map[pattern_source_count].lit_triple = lit_triple;
	pattern_source_map[pattern_source_count].pattern_src = (char *)malloc(pattern_len + 1);
	if (pattern_source_map[pattern_source_count].pattern_src) {
		memcpy(pattern_source_map[pattern_source_count].pattern_src, pattern_src, pattern_len);
		pattern_source_map[pattern_source_count].pattern_src[pattern_len] = '\0';
		pattern_source_map[pattern_source_count].pattern_len = pattern_len;
		pattern_source_count++;
	}
}

/* Get the pattern source string for an OC_LIT triple, or NULL if not a pattern */
const char *ast_dump_get_pattern_source(triple *lit_triple, int *len_out)
{
	int i;
	
	if (!lit_triple) {
		if (len_out) *len_out = 0;
		return NULL;
	}
	
	for (i = 0; i < pattern_source_count; i++) {
		if (pattern_source_map[i].lit_triple == lit_triple) {
			if (len_out) *len_out = pattern_source_map[i].pattern_len;
			return pattern_source_map[i].pattern_src;
		}
	}
	
	if (len_out) *len_out = 0;
	return NULL;
}

/* Clean up pattern source map (called from ast_dump_json_cleanup) */
static void cleanup_pattern_source_map(void)
{
	int i;
	
	for (i = 0; i < pattern_source_count; i++) {
		if (pattern_source_map[i].pattern_src) {
			free(pattern_source_map[i].pattern_src);
			pattern_source_map[i].pattern_src = NULL;
		}
	}
	pattern_source_count = 0;
}
