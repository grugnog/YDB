#!/usr/bin/env python3
"""
Example: Basic MUMPS Parser Usage

This example demonstrates how to use the ydb_parser module to parse
MUMPS code and examine the resulting Abstract Syntax Tree (AST).
"""

from ydb_parser import parse_mumps, parse_mumps_file, YDBParserError
import json
import sys


def example_1_simple_parse():
    """Example 1: Parse a simple MUMPS statement"""
    print("=" * 60)
    print("Example 1: Simple MUMPS Statement")
    print("=" * 60)
    
    code = ' write "Hello, World!",!'
    
    print(f"Code:{code}")
    print()
    
    try:
        ast = parse_mumps(code)
        print(f"AST Type: {ast['ast_type']}")
        print(f"Number of triples: {len(ast['triples'])}")
        print()
        
        # Show first few triples
        print("First 3 triples:")
        for i, triple in enumerate(ast['triples'][:3]):
            print(f"  [{i}] {triple['opcode']:20s} (line {triple['source_line']}, col {triple['source_column']})")
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_2_function_with_locals():
    """Example 2: Parse a function with local variables"""
    print("=" * 60)
    print("Example 2: Function with Local Variables")
    print("=" * 60)
    
    code = """
add(a,b) ; Add two numbers
 new result
 set result=a+b
 quit result
"""
    
    print("Code:")
    print(code)
    
    try:
        ast = parse_mumps(code)
        
        print(f"Total triples: {len(ast['triples'])}")
        print()
        
        # Find variable references
        print("Variable references found:")
        for triple in ast['triples']:
            for operand in triple['operands']:
                if operand['class'] == 'MVAR_REF':
                    var_name = operand.get('variable_name', 'unknown')
                    scope = operand.get('scope', 'unknown')
                    print(f"  - {var_name} (scope: {scope})")
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_3_global_access():
    """Example 3: Parse code with global variable access"""
    print("=" * 60)
    print("Example 3: Global Variable Access")
    print("=" * 60)
    
    code = """
save(id,name) ; Save to global
 set ^Person(id,"name")=name
 quit
"""
    
    print("Code:")
    print(code)
    
    try:
        ast = parse_mumps(code)
        
        # Find global variable references
        print("Global variables:")
        for triple in ast['triples']:
            for operand in triple['operands']:
                if operand['class'] == 'MVAR_REF':
                    var_name = operand.get('variable_name', '')
                    if var_name.startswith('^'):
                        print(f"  - {var_name}")
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_4_literals():
    """Example 4: Examine different types of literals"""
    print("=" * 60)
    print("Example 4: Literal Values")
    print("=" * 60)
    
    code = """
demo()
 set str="Hello"
 set num=42
 set float=3.14
 quit
"""
    
    print("Code:")
    print(code)
    
    try:
        ast = parse_mumps(code)
        
        # Find literals
        print("Literals found:")
        for triple in ast['triples']:
            for operand in triple['operands']:
                if operand['class'] == 'MLIT_REF':
                    lit_value = operand.get('literal_value', 'null')
                    lit_type = operand.get('literal_type', 'unknown')
                    print(f"  - {lit_value} (type: {lit_type})")
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_5_control_flow():
    """Example 5: Parse control flow statements"""
    print("=" * 60)
    print("Example 5: Control Flow")
    print("=" * 60)
    
    code = """
check(x) ; Check value
 if x>10 do
 . write "Large",!
 else  do
 . write "Small",!
 quit
"""
    
    print("Code:")
    print(code)
    
    try:
        ast = parse_mumps(code)
        
        print(f"Total triples: {len(ast['triples'])}")
        print()
        
        # Show opcodes for control flow
        print("Opcodes (first 15):")
        for i, triple in enumerate(ast['triples'][:15]):
            print(f"  [{i:2d}] {triple['opcode']}")
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_6_full_ast_dump():
    """Example 6: Dump complete AST as formatted JSON"""
    print("=" * 60)
    print("Example 6: Complete AST Dump")
    print("=" * 60)
    
    code = ' set x=1 write x,!'
    
    print(f"Code: {code}")
    print()
    
    try:
        ast = parse_mumps(code)
        
        print("Full AST (formatted JSON):")
        print(json.dumps(ast, indent=2))
        
    except YDBParserError as e:
        print(f"Error: {e}")
    
    print()


def example_7_error_handling():
    """Example 7: Handle parsing errors"""
    print("=" * 60)
    print("Example 7: Error Handling")
    print("=" * 60)
    
    invalid_codes = [
        ' set (x,$TEST)=1',  # Invalid syntax
        ' write "Unclosed string',  # Unclosed string
        ' for i=1:1 do',  # Incomplete for loop
    ]
    
    for code in invalid_codes:
        print(f"Trying to parse: {code}")
        try:
            ast = parse_mumps(code)
            print("  ✓ Parsed successfully")
        except YDBParserError as e:
            print(f"  ✗ Error: {e}")
        print()


def main():
    """Run all examples"""
    print("\n" + "=" * 60)
    print("YDB MUMPS Parser - Python Examples")
    print("=" * 60 + "\n")
    
    examples = [
        example_1_simple_parse,
        example_2_function_with_locals,
        example_3_global_access,
        example_4_literals,
        example_5_control_flow,
        example_6_full_ast_dump,
        example_7_error_handling,
    ]
    
    for example in examples:
        try:
            example()
        except Exception as e:
            print(f"Example failed with unexpected error: {e}")
            import traceback
            traceback.print_exc()
            print()
    
    print("=" * 60)
    print("All examples completed!")
    print("=" * 60)


if __name__ == '__main__':
    main()
