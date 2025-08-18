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
#include "opcode.h"
#include "cmd_qlf.h"
#include <stdio.h>
#include <string.h>
#include "ast_dump_json.h"

GBLREF triple		t_orig;
GBLREF command_qualifier	cmd_qlf;
GBLREF unsigned char	source_file_name[];
GBLREF unsigned short	source_name_len;

LITREF char *oc_tab_graphic[];

static FILE *ast_json_file = NULL;
static int indent_level = 0;

/* Forward declarations */
static void dump_triple_json(triple *trip, boolean_t is_last);
static void dump_operand_json(oprtype *opr, boolean_t is_last);
static void write_indent(void);
static const char* opcode_to_string(opctype opcode);
static const char* oprclass_to_string(operclass class);

/* Initialize JSON AST dumping */
void ast_dump_json_init(void)
{
	FILE *debug_file = fopen("/workspace/debug_init.log", "w");
	if (debug_file) {
		fprintf(debug_file, "ast_dump_json_init() called\n");
		fflush(debug_file);
		fclose(debug_file);
	}
	
	fprintf(stderr, "[DEBUG] ast_dump_json_init() called\n");
	fflush(stderr);
	
	if (!(cmd_qlf.qlf & CQ_DUMP_AST_JSON)) {
		fprintf(stderr, "[DEBUG] ast_dump_json_init() - flag not set, returning\n");
		fflush(stderr);
		debug_file = fopen("/workspace/debug_init.log", "a");
		if (debug_file) {
			fprintf(debug_file, "flag not set, returning\n");
			fclose(debug_file);
		}
		return;
	}

	fprintf(stderr, "[DEBUG] ast_dump_json_init() - flag is set, initializing\n");
	fflush(stderr);
	
	debug_file = fopen("/workspace/debug_init.log", "a");
	if (debug_file) {
		fprintf(debug_file, "flag is set, initializing\n");
		fclose(debug_file);
	}
	
	// Just mark that AST dumping was requested
	// Don't do ANYTHING else that might interfere with compilation
	ast_json_file = NULL;
	
	fprintf(stderr, "[DEBUG] ast_dump_json_init() completed\n");
	fflush(stderr);
	debug_file = fopen("/workspace/debug_init.log", "a");
	if (debug_file) {
		fprintf(debug_file, "completed\n");
		fclose(debug_file);
	}
}/* Dump the entire AST as JSON */
void ast_dump_json_complete(void)
{
	triple *trip;
	int count = 0;
	char json_filename[256];
	char base_name_copy[256];
	char *dot_pos;
	
	// Check if we should dump AST
	{
		FILE *debug_file = fopen("/workspace/debug_complete.log", "w");
		if (debug_file) {
			fprintf(debug_file, "ast_dump_json_complete() called\n");
			fclose(debug_file);
		}
	}
	
	fprintf(stderr, "[DEBUG] ast_dump_json_complete() called\n");
	fflush(stderr);
	
	if (!(cmd_qlf.qlf & CQ_DUMP_AST_JSON)) {
		fprintf(stderr, "[DEBUG] ast_dump_json_complete() - flag not set, returning\n");
		fflush(stderr);
		{
			FILE *debug_file = fopen("/workspace/debug_complete.log", "a");
			if (debug_file) {
				fprintf(debug_file, "flag not set, returning\n");
				fclose(debug_file);
			}
		}
		return;
	}

	{
		FILE *debug_file = fopen("/workspace/debug_complete.log", "a");
		if (debug_file) {
			fprintf(debug_file, "flag is set, proceeding\n");
			fclose(debug_file);
		}
	}

	fprintf(stderr, "[DEBUG] ast_dump_json_complete() - flag is set, proceeding\n");
	fflush(stderr);
	
	// Create JSON filename based on source filename - do this NOW when it's safe
	if (source_name_len > 0 && source_name_len < sizeof(base_name_copy)) {
		/* Make a copy to avoid modifying the original */
		strncpy(base_name_copy, (char*)source_file_name, sizeof(base_name_copy)-1);
		base_name_copy[sizeof(base_name_copy)-1] = '\0';
		
		dot_pos = strrchr(base_name_copy, '.');
		if (dot_pos != NULL) {
			*dot_pos = '\0';  /* Null terminate at the dot */
			snprintf(json_filename, sizeof(json_filename), "%s_ast.json", base_name_copy);
		} else {
			snprintf(json_filename, sizeof(json_filename), "%s_ast.json", base_name_copy);
		}
	} else {
		strcpy(json_filename, "mumps_ast.json");
	}
	
	// NOW open the file - after all compilation is done
	ast_json_file = fopen(json_filename, "w");
	if (!ast_json_file) {
		printf("Warning: Could not create AST JSON file %s\n", json_filename);
		return;
	}
	
	printf("Dumping AST to: %s\n", json_filename);

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
				fprintf(ast_json_file, "      \"error\": \"Invalid triple pointer: %p\"\n", (void*)trip);
				fprintf(ast_json_file, "    }%s\n", (current == count) ? "" : ",");
				continue;
			}
			dump_triple_json(trip, (current == count));
		}
	}
	
	indent_level = 1;
	write_indent();
	fprintf(ast_json_file, "]\n");
	fprintf(ast_json_file, "}\n");
	
	fclose(ast_json_file);
	ast_json_file = NULL;
}

/* Clean up if needed */
void ast_dump_json_cleanup(void)
{
	if (ast_json_file) {
		fclose(ast_json_file);
		ast_json_file = NULL;
	}
}

/* Dump a single triple as JSON */
static void dump_triple_json(triple *trip, boolean_t is_last)
{
	if (!ast_json_file || !trip)
		return;
		
	write_indent();
	fprintf(ast_json_file, "{\n");
	indent_level++;
	
	write_indent();
	fprintf(ast_json_file, "\"address\": \"%p\",\n", (void*)trip);
	
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
	if (fprintf(ast_json_file, "\"value\": ") < 0) return;
	fflush(ast_json_file);
	
	switch (opr->oprclass) {
		case NO_REF:
			if (fprintf(ast_json_file, "null") < 0) return;
			break;
		case TRIP_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.tref) < 0) return;
			break;
		case TNXT_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.tref) < 0) return;
			break;
		case ILIT_REF:
			if (fprintf(ast_json_file, "%d", opr->oprval.ilit) < 0) return;
			break;
		case MLIT_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.mlit) < 0) return;
			break;
		case MVAR_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.vref) < 0) return;
			break;
		case MLAB_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.lab) < 0) return;
			break;
		case MNXL_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.mlit) < 0) return;
			break;
		case MFUN_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.mlit) < 0) return;
			break;
		case TJMP_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.tref) < 0) return;
			break;
		case INDR_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.indr) < 0) return;
			break;
		case CDIDX_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.mlit) < 0) return;
			break;
		case CDLT_REF:
			if (fprintf(ast_json_file, "\"%p\"", (void*)opr->oprval.cdlt) < 0) return;
			break;
		case TEMP_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
		case TVAR_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
		case TVAD_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
		case TCAD_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
		case TVAL_REF:
			if (fprintf(ast_json_file, "%u", opr->oprval.temp) < 0) return;
			break;
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
