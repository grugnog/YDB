# Using YDB MUMPS Parser with Docker

This guide shows how to use the YDB MUMPS Parser Python bindings via the Docker container.

## Quick Start

### 1. Pull the Docker Image

```bash
docker pull ghcr.io/grugnog/ydb:ast-dump
```

### 2. Interactive Python Session

```bash
docker run -it --rm ghcr.io/grugnog/ydb:ast-dump python3
```

Note: The default entrypoint automatically sources the YDB environment.

Then in Python:

```python
from ydb_parser import parse_mumps

# Parse a simple MUMPS statement
code = ' write "Hello, World!",!'
ast = parse_mumps(code)

print(f"AST Type: {ast['ast_type']}")
print(f"Number of triples: {len(ast['triples'])}")

# Pretty print the full AST
import json
print(json.dumps(ast, indent=2))
```

### 3. Run a Python Script

Create `my_parser.py`:

```python
#!/usr/bin/env python3
from ydb_parser import parse_mumps
import json

code = """
fibonacci(n) ; Calculate Fibonacci number
 if n<2 quit n
 quit $$fibonacci(n-1)+$$fibonacci(n-2)
"""

ast = parse_mumps(code)
print(json.dumps(ast, indent=2))
```

Run it:

```bash
docker run -it --rm -v "$(pwd):/workspace" -w /workspace \
  ghcr.io/grugnog/ydb:ast-dump \
  python3 my_parser.py
```

### 4. Run Examples from the Repository

```bash
# Clone the repository
git clone https://github.com/grugnog/YDB.git
cd YDB

# Run the example
docker run -it --rm -v "$(pwd):/workspace" -w /workspace \
  ghcr.io/grugnog/ydb:ast-dump \
  python3 python/examples/parse_example.py
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
from ydb_parser import parse_mumps

# Simple statement
ast = parse_mumps(' set x=1')

# Multiple lines
code = """
demo()
 set x=1
 write x,!
 quit
"""
ast = parse_mumps(code)
```

### Error Handling

```python
from ydb_parser import parse_mumps, YDBParserError

try:
    # Invalid MUMPS code
    ast = parse_mumps(' set ($TEST)=1')
except YDBParserError as e:
    print(f"Parse error: {e}")
```

### Parse Files

```python
from ydb_parser import parse_mumps_file

# Parse a .m file
ast = parse_mumps_file('/path/to/your/routine.m')
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
