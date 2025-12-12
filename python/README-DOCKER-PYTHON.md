# YDB Python Docker Image

A Python-optimized Docker image for YottaDB MUMPS Parser with simplified usage.

## Features

- **Python-first interface**: Python is the default entrypoint - no need to specify `python3`
- **Pre-configured environment**: YDB environment variables automatically sourced
- **Drop-in replacement**: Use like the `python` command with Docker
- **Multi-line support**: Automatically handles multi-line MUMPS code via temporary files
- **Globally installed module**: `ydb_parser` available in all Python sessions

## Quick Examples

### Parse MUMPS code inline

```bash
docker run --rm ghcr.io/grugnog/ydb:ast-dump-python -c "
from ydb_parser import YDBParser
parser = YDBParser()
result = parser.parse('HELLO W \"Hello!\",! Q')
print(f'Parsed {len(result[\"triples\"])} triples')
"
```

### Run a Python script

```bash
# Create your script
cat > parse_mumps.py << 'EOF'
from ydb_parser import YDBParser
parser = YDBParser()
code = """TEST
 S X=1
 W X
 Q"""
ast = parser.parse(code)
print(f"Parsed {len(ast['triples'])} triples from {len(code.splitlines())} lines")
EOF

# Run it (Python is the default entrypoint)
docker run --rm -v "$(pwd):/data" ghcr.io/grugnog/ydb:ast-dump-python parse_mumps.py
```

### Interactive Python shell

```bash
docker run -it --rm ghcr.io/grugnog/ydb:ast-dump-python
```

Then in the Python shell:
```python
from ydb_parser import YDBParser
parser = YDBParser()
result = parser.parse('TEST S X=1 Q')
print(result)
```

## Comparison with Base Image

| Feature | Base Image | Python Image |
|---------|------------|--------------|
| Image name | `ghcr.io/grugnog/ydb:ast-dump` | `ghcr.io/grugnog/ydb:ast-dump-python` |
| Default entrypoint | YDB shell | Python 3 |
| YDB environment | Sourced automatically | Sourced automatically |
| Python command | Requires `python3` | Direct execution |
| Use case | Full YDB development | Python-focused parsing |

## Building Locally

```bash
# Build base image first
docker build -t ydb-test -f Dockerfile .

# Build Python image on top
docker build -t ydb-python -f Dockerfile-python --build-arg BASE_IMAGE=ydb-test .
```

## Performance Characteristics

The Python image uses the same parsing logic as the base image:

- **Single-line code**: Fastest - uses `op_fnzycompile()` directly in memory
- **Existing .m files**: Fast - uses `compiler_startup()` to parse files directly  
- **Multi-line strings**: Slower - automatically creates temporary files for parsing

See the [Python Usage Guide](../PYTHON_USAGE_GUIDE.md) for more details.

## License

Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.  
Licensed under the same terms as YottaDB.
