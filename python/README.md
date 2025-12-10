# YDB MUMPS Parser - Python Bindings

Python bindings for parsing MUMPS code using YottaDB's compiler and generating Abstract Syntax Tree (AST) representations in JSON format.

## Features

- Parse MUMPS code from strings or files
- Generate detailed AST representations
- Access to full YottaDB compiler functionality
- Simple ctypes-based interface (no compilation needed)
- Clean Python API

## Prerequisites

- YottaDB installed with `libyottadb.so` available
- Python 3.7 or higher

## Installation

```bash
cd python
pip install .
```

## Quick Start

### Parse MUMPS code from a string

```python
from ydb_parser import parse_mumps

code = """
hello() ; Hello World example
 write "Hello, World!",!
 quit
"""

ast = parse_mumps(code)
print(f"Found {len(ast['triples'])} AST nodes")

# Access individual triples (AST nodes)
for triple in ast['triples'][:5]:
    print(f"{triple['opcode']} at line {triple['source_line']}")
```

### Parse MUMPS code from a file

```python
from ydb_parser import parse_mumps_file

ast = parse_mumps_file('example.m')
print(ast['ast_type'])  # "MUMPS"
print(ast['source_file'])
```

### Using the Parser class directly

```python
from ydb_parser import YDBParser

# Create parser with explicit library path
parser = YDBParser('/usr/local/lib/libyottadb.so')

# Parse code
ast = parser.parse('write "Test",!')

# Keep temporary JSON files for debugging
ast = parser.parse('set x=1', cleanup=False)
```

## AST Structure

The returned AST is a Python dictionary with the following structure:

```python
{
    "ast_type": "MUMPS",
    "source_file": "mumps_ast.json",
    "triples": [
        {
            "triple_id": 1,
            "opcode": "OC_LINEFETCH",
            "opcode_value": 42,
            "source_line": 1,
            "source_column": 0,
            "rtaddr": 0,
            "operands": [
                {
                    "class": "ILIT_REF",
                    "class_value": 3,
                    "value": 1
                },
                # ... second operand
            ],
            "destination": {
                "class": "NO_REF",
                "class_value": 0,
                "value": null
            }
        },
        # ... more triples
    ]
}
```

### Triple Structure

Each triple (AST node) contains:
- `triple_id`: Sequential ID for the node
- `opcode`: String representation of the operation (e.g., "OC_WRITE", "OC_RET")
- `opcode_value`: Numeric opcode value
- `source_line`: Line number in source code
- `source_column`: Column number in source code
- `rtaddr`: Runtime address information
- `operands`: Array of 2 operands
- `destination`: Destination operand

### Operand Types

Operands have different types indicated by their `class`:
- `NO_REF`: No reference (empty)
- `TRIP_REF`: Reference to another triple
- `ILIT_REF`: Integer literal
- `MLIT_REF`: MUMPS literal (string or numeric)
- `MVAR_REF`: Variable reference (local, global, or intrinsic)
- `TEMP_REF`: Temporary variable

## Error Handling

```python
from ydb_parser import parse_mumps, YDBParserError

try:
    ast = parse_mumps('invalid mumps code ;;;')
except YDBParserError as e:
    print(f"Parse error: {e}")
```

## Environment Variables

The parser will look for `libyottadb.so` in these locations:
1. `$ydb_dist/libyottadb.so`
2. `/usr/local/lib/libyottadb.so`
3. `/usr/lib/libyottadb.so`
4. Other standard library paths

Set `ydb_dist` if YottaDB is installed in a non-standard location:

```bash
export ydb_dist=/path/to/yottadb
```

## Examples

See the `examples/` directory for more detailed usage examples.

## Development

To install in development mode:

```bash
cd python
pip install -e .[dev]
```

Run tests:

```bash
pytest
```

## License

Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.
Licensed under the GNU Affero General Public License v3.

## Support

- Documentation: https://docs.yottadb.com
- Issues: https://gitlab.com/YottaDB/DB/YDB/-/issues
- Community: https://yottadb.com/resources/community/
