# Using YDB MUMPS Parser with Docker

This guide shows how to use the YDB MUMPS Parser Python bindings via the Docker container.

## Docker Images

Two Docker images are available:

1. **Base Image** (`ghcr.io/grugnog/ydb:ast-dump`): Full YDB installation with Python support
2. **Python-Optimized Image** (`ghcr.io/grugnog/ydb:ast-dump-python`): Python-first interface with automatic YDB environment setup

The Python-optimized image is recommended for most Python use cases as it:
- Pre-sources YDB environment variables automatically
- Uses Python as the default entrypoint (no need to specify `python3`)
- Enables drop-in replacement for the `python` command

## Quick Start

### Using the Python-Optimized Image (Recommended)

#### 1. Pull the Docker Image

```bash
docker pull ghcr.io/grugnog/ydb:ast-dump-python
```

#### 2. Run Python Code Directly

```bash
# Run inline Python code
docker run --rm ghcr.io/grugnog/ydb:ast-dump-python -c "
from ydb_parser import YDBParser
parser = YDBParser()
result = parser.parse('HELLO W \"Hello, World!\",! Q')
print(f'Parsed {len(result[\"triples\"])} triples')
"

# Start interactive Python shell
docker run -it --rm ghcr.io/grugnog/ydb:ast-dump-python
```

#### 3. Run a Python Script

Create `my_parser.py`:

```python
#!/usr/bin/env python3
from ydb_parser import YDBParser
import json

parser = YDBParser()

# Multi-line code (will be written to temp file automatically)
code = """fibonacci(n) ; Calculate Fibonacci number
 if n<2 quit n
 quit $$fibonacci(n-1)+$$fibonacci(n-2)"""

ast = parser.parse(code)
print(json.dumps(ast, indent=2))
```

Run it:

```bash
# Simply pass the script name - Python is the default entrypoint
docker run --rm -v "$(pwd):/data" ghcr.io/grugnog/ydb:ast-dump-python my_parser.py
```

### Using the Base Image

If you need the base YDB environment or want to use the full shell:

#### 1. Pull the Docker Image

```bash
docker pull ghcr.io/grugnog/ydb:ast-dump
```

#### 2. Interactive Python Session

```bash
docker run -it --rm ghcr.io/grugnog/ydb:ast-dump python3
```

Note: The YDB environment is automatically sourced by the entrypoint.

Then in Python:

```python
from ydb_parser import YDBParser

parser = YDBParser()

# Parse a simple MUMPS statement
code = ' write "Hello, World!",!'
ast = parser.parse(code)

print(f"AST Type: {ast['ast_type']}")
print(f"Number of triples: {len(ast['triples'])}")

# Pretty print the full AST
import json
print(json.dumps(ast, indent=2))
```

#### 3. Run a Python Script

```bash
docker run --rm -v "$(pwd):/data" ghcr.io/grugnog/ydb:ast-dump python3 my_parser.py
```

### 4. Run Examples from the Repository

```bash
# Clone the repository
git clone https://github.com/grugnog/YDB.git
cd YDB

# With Python-optimized image (simpler)
docker run --rm -v "$(pwd):/data" ghcr.io/grugnog/ydb:ast-dump-python python/examples/parse_example.py

# With base image
docker run --rm -v "$(pwd):/data" ghcr.io/grugnog/ydb:ast-dump python3 python/examples/parse_example.py
```

## Development Workflow

### Mount Your Project Directory

```bash
docker run -it --rm \
  -v "$(pwd):/workspace" \
  -w /workspace \
  ghcr.io/grugnog/ydb:ast-dump \
  bash
```

Then you can run Python scripts, edit files, etc. from within the container.

**Note:** If you override the entrypoint with `--entrypoint=/bin/bash`, you'll need to source the YDB environment first:

```bash
source /opt/yottadb/current/ydb_env_set
python3 your_script.py
```

### Use as a Base Image

Create your own Dockerfile:

```dockerfile
FROM ghcr.io/grugnog/ydb:ast-dump

# Copy your application
COPY . /app
WORKDIR /app

# Install additional Python packages if needed
RUN pip3 install --break-system-packages -r requirements.txt

CMD ["python3", "your_app.py"]
```

## API Examples

See the full documentation in `python/README.md` and examples in `python/examples/`.

### Basic Parsing

```python
from ydb_parser import YDBParser

parser = YDBParser()

# Single-line statement (fastest - uses op_fnzycompile())
ast = parser.parse('TEST S X=1 W X Q')
print(f"Triples: {len(ast['triples'])}")  # 13 triples

# Multi-line code (automatic temp file handling)
code = """TEST
 S X=1
 W X
 Q"""
ast = parser.parse(code)
print(f"Triples: {len(ast['triples'])}")  # 20 triples - all lines parsed
```

### Performance Characteristics

The parser uses different underlying mechanisms depending on the input:

1. **Single-line code** (fastest): Uses YDB's `op_fnzycompile()` function directly in memory
   - Best for: Simple expressions, single statements, performance-critical parsing
   - Example: `parser.parse('TEST S X=1 W X Q')`

2. **Existing .m files** (fast): Parses files directly using `compiler_startup()`
   - Best for: Pre-existing MUMPS routine files
   - Example: `parser.parse_file('/path/to/routine.m')`

3. **Multi-line strings** (slower): Automatically writes to temporary file, then parses
   - Best for: Complete routines passed as strings, multi-line code generation
   - Note: Creates temp files in system temp directory, cleaned up automatically
   - Example: `parser.parse('ROUTINE\\n S X=1\\n Q')`

**Recommendation:** For best performance when parsing many snippets, keep code single-line when possible, or save to a file and use `parse_file()` for repeated parsing.

### Error Handling

```python
from ydb_parser import YDBParser, YDBParserError

parser = YDBParser()

try:
    # Invalid MUMPS code
    ast = parser.parse(' set ($TEST)=1')
except YDBParserError as e:
    print(f"Parse error: {e}")
```

### Parse Files Directly

```python
from ydb_parser import YDBParser

parser = YDBParser()

# Parse an existing .m file (no temp file needed)
ast = parser.parse_file('/path/to/your/routine.m')
print(f"Parsed {len(ast['triples'])} triples")
```

### Advanced Usage

```python
from ydb_parser import YDBParser
import json

parser = YDBParser()

# Parse and pretty-print
ast = parser.parse('TEST S X=1 W X Q')
print(json.dumps(ast, indent=2))

# Control cleanup behavior (defaults to True)
ast = parser.parse(code, cleanup=True)

# Parse file without cleanup
ast = parser.parse_file('/tmp/test.m', cleanup=False)
# JSON file remains at /tmp/test_ast.json
```

## Troubleshooting

### "Cannot find libyottadb.so"

This error shouldn't occur in the Docker container. If you see it, ensure you're using the correct image:

```bash
docker pull ghcr.io/grugnog/ydb:ast-dump
```

### Permission Issues with Mounted Volumes

If you encounter permission issues when mounting volumes, you may need to adjust ownership:

```bash
docker run -it --rm \
  -v "$(pwd):/workspace" \
  -w /workspace \
  --user $(id -u):$(id -g) \
  ghcr.io/grugnog/ydb:ast-dump \
  python3 script.py
```

## Building from Source

If you want to build the Docker image yourself:

```bash
git clone https://github.com/grugnog/YDB.git
cd YDB
git checkout ast-dump
docker build -t ydb-ast .
```

Then use `ydb-ast` instead of `ghcr.io/grugnog/ydb:ast-dump` in the commands above.
