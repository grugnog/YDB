#!/usr/bin/env python3
"""
Example: Parse MUMPS File

This example shows how to parse a MUMPS source file and analyze its AST.
"""

from ydb_parser import parse_mumps_file, YDBParserError
import sys
from pathlib import Path


def analyze_mumps_file(filepath):
    """
    Parse and analyze a MUMPS source file.
    
    Args:
        filepath: Path to the MUMPS file
    """
    print(f"Parsing: {filepath}")
    print("-" * 60)
    
    try:
        ast = parse_mumps_file(filepath)
        
        print(f"✓ Successfully parsed!")
        print()
        
        # Basic statistics
        print("Statistics:")
        print(f"  Total triples: {len(ast['triples'])}")
        
        # Count opcodes
        opcodes = {}
        for triple in ast['triples']:
            opcode = triple['opcode']
            opcodes[opcode] = opcodes.get(opcode, 0) + 1
        
        print(f"  Unique opcodes: {len(opcodes)}")
        print()
        
        # Top 10 most common opcodes
        print("Top 10 most common opcodes:")
        sorted_opcodes = sorted(opcodes.items(), key=lambda x: x[1], reverse=True)
        for opcode, count in sorted_opcodes[:10]:
            print(f"  {opcode:25s} : {count:4d}")
        print()
        
        # Find variables
        variables = {'local': set(), 'global': set(), 'intrinsic': set()}
        for triple in ast['triples']:
            for operand in triple['operands'] + [triple['destination']]:
                if operand['class'] == 'MVAR_REF':
                    var_name = operand.get('variable_name', '')
                    scope = operand.get('scope', 'local')
                    if scope in variables:
                        variables[scope].add(var_name)
        
        print("Variables:")
        for scope, vars in variables.items():
            if vars:
                print(f"  {scope.capitalize()}: {', '.join(sorted(vars))}")
        
        if not any(variables.values()):
            print("  (none found)")
        print()
        
        # Source line range
        lines = [t['source_line'] for t in ast['triples'] if t['source_line'] > 0]
        if lines:
            print(f"Source line range: {min(lines)} - {max(lines)}")
        
    except FileNotFoundError:
        print(f"✗ Error: File not found: {filepath}")
    except YDBParserError as e:
        print(f"✗ Parse error: {e}")
    except Exception as e:
        print(f"✗ Unexpected error: {e}")
        import traceback
        traceback.print_exc()


def main():
    if len(sys.argv) < 2:
        print("Usage: python parse_file_example.py <mumps_file>")
        print()
        print("Example:")
        print("  python parse_file_example.py hello.m")
        sys.exit(1)
    
    filepath = sys.argv[1]
    
    if not Path(filepath).exists():
        print(f"Error: File does not exist: {filepath}")
        sys.exit(1)
    
    analyze_mumps_file(filepath)


if __name__ == '__main__':
    main()
