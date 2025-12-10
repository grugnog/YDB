"""
YDB MUMPS Parser - Python Bindings

This module provides Python bindings for parsing MUMPS code and generating
Abstract Syntax Tree (AST) representations in JSON format.

The module uses ctypes to interface with the YottaDB C library (libyottadb.so)
and wraps the ydb_parse_mumps_to_json C function.

Example usage:
    from ydb_parser import parse_mumps, parse_mumps_file
    
    # Parse MUMPS code from a string
    code = '''
    hello() ; Hello World example
     write "Hello, World!",!
     quit
    '''
    
    ast = parse_mumps(code)
    print(f"Found {len(ast['triples'])} AST nodes")
    
    # Parse MUMPS code from a file
    ast = parse_mumps_file('example.m')
"""

import ctypes
import json
import os
from pathlib import Path
from typing import Dict, List, Optional, Union


class YDBParserError(Exception):
    """Exception raised when MUMPS parsing fails."""
    pass


class YDBParser:
    """
    Python interface to the YottaDB MUMPS parser.
    
    This class provides methods to parse MUMPS code and return the Abstract
    Syntax Tree (AST) as a Python dictionary.
    
    Attributes:
        lib: The loaded libyottadb shared library
        libydb_path: Path to the libyottadb.so file
    """
    
    def __init__(self, libydb_path: Optional[str] = None):
        """
        Initialize the YDB parser with a specific libyottadb.so path.
        
        Args:
            libydb_path: Optional path to libyottadb.so. If not provided,
                        will search common locations.
        
        Raises:
            RuntimeError: If libyottadb.so cannot be found
        """
        if libydb_path is None:
            libydb_path = self._find_libyottadb()
        
        self.libydb_path = libydb_path
        self.lib = ctypes.CDLL(libydb_path)
        self._setup_signatures()
    
    def _find_libyottadb(self) -> str:
        """
        Search for libyottadb.so in common locations.
        
        Returns:
            Path to libyottadb.so
        
        Raises:
            RuntimeError: If library cannot be found
        """
        # Check bundled library first (for pip-installed packages)
        bundled_lib = Path(__file__).parent / 'lib' / 'libyottadb.so'
        if bundled_lib.exists():
            return str(bundled_lib)
        
        # Common installation paths
        search_paths = [
            # Environment variable
            os.path.join(os.environ.get('ydb_dist', ''), 'libyottadb.so'),
            # Standard Linux paths
            '/usr/local/lib/libyottadb.so',
            '/usr/lib/libyottadb.so',
            '/usr/lib64/libyottadb.so',
            '/usr/local/lib/yottadb/r*/libyottadb.so',
            # macOS paths
            '/usr/local/lib/libyottadb.dylib',
            '/opt/homebrew/lib/libyottadb.dylib',
        ]
        
        for path_pattern in search_paths:
            # Handle glob patterns
            if '*' in path_pattern:
                from glob import glob
                matches = glob(path_pattern)
                if matches:
                    # Use the most recent version if multiple matches
                    matches.sort(reverse=True)
                    path = matches[0]
                    if os.path.exists(path):
                        return path
            elif os.path.exists(path_pattern):
                return path_pattern
        
        raise RuntimeError(
            "Could not find libyottadb.so. Please set the ydb_dist "
            "environment variable or provide the path explicitly."
        )
    
    def _setup_signatures(self):
        """Configure ctypes function signatures for C library calls."""
        # int ydb_parse_mumps_to_json(
        #     const char *code,
        #     char *json_filename_out,
        #     size_t filename_len,
        #     char *error_out,
        #     size_t error_len
        # )
        self.lib.ydb_parse_mumps_to_json.argtypes = [
            ctypes.c_char_p,  # code
            ctypes.c_char_p,  # json_filename_out
            ctypes.c_size_t,  # filename_len
            ctypes.c_char_p,  # error_out
            ctypes.c_size_t,  # error_len
        ]
        self.lib.ydb_parse_mumps_to_json.restype = ctypes.c_int
    
    def parse(self, code: str, cleanup: bool = True) -> Dict:
        """
        Parse MUMPS code and return the AST as a Python dictionary.
        
        Args:
            code: String containing MUMPS code to parse
            cleanup: If True, delete the temporary JSON file after parsing
        
        Returns:
            Dictionary containing the AST with keys:
                - ast_type: Always "MUMPS"
                - source_file: Name of source (usually "mumps_ast.json")
                - triples: List of AST nodes (triples)
        
        Raises:
            YDBParserError: If parsing fails
            
        Example:
            >>> parser = YDBParser()
            >>> ast = parser.parse('write "Hello",!')
            >>> print(len(ast['triples']))
            3
        """
        if not isinstance(code, str):
            raise TypeError("code must be a string")
        
        if not code.strip():
            raise ValueError("code cannot be empty")
        
        # Prepare buffers for output
        filename_buf = ctypes.create_string_buffer(1024)
        error_buf = ctypes.create_string_buffer(4096)
        
        # Call the C function
        result = self.lib.ydb_parse_mumps_to_json(
            code.encode('utf-8'),
            filename_buf,
            len(filename_buf),
            error_buf,
            len(error_buf)
        )
        
        # Check for errors
        if result != 0:
            error_msg = error_buf.value.decode('utf-8', errors='replace')
            raise YDBParserError(f"Parse failed (code {result}): {error_msg}")
        
        # Get the JSON filename
        json_filename = filename_buf.value.decode('utf-8')
        
        if not json_filename:
            raise YDBParserError("No JSON filename returned from parser")
        
        # Read and parse the JSON file
        try:
            with open(json_filename, 'r', encoding='utf-8') as f:
                ast = json.load(f)
        except FileNotFoundError:
            raise YDBParserError(f"JSON file not found: {json_filename}")
        except json.JSONDecodeError as e:
            raise YDBParserError(f"Invalid JSON in {json_filename}: {e}")
        finally:
            # Optionally clean up the temporary file
            if cleanup and os.path.exists(json_filename):
                try:
                    os.unlink(json_filename)
                except OSError:
                    pass  # Ignore cleanup errors
        
        return ast
    
    def parse_file(self, filename: Union[str, Path], cleanup: bool = True) -> Dict:
        """
        Parse MUMPS code from a file and return the AST.
        
        Args:
            filename: Path to MUMPS source file
            cleanup: If True, delete the temporary JSON file after parsing
        
        Returns:
            Dictionary containing the AST
        
        Raises:
            FileNotFoundError: If the input file doesn't exist
            YDBParserError: If parsing fails
        """
        filepath = Path(filename)
        
        if not filepath.exists():
            raise FileNotFoundError(f"MUMPS source file not found: {filename}")
        
        # Read the file content
        with open(filepath, 'r', encoding='utf-8') as f:
            code = f.read()
        
        # Parse the code
        return self.parse(code, cleanup=cleanup)


# Module-level convenience functions

_default_parser: Optional[YDBParser] = None


def get_parser() -> YDBParser:
    """
    Get or create the default YDBParser instance.
    
    Returns:
        The default parser instance
    """
    global _default_parser
    if _default_parser is None:
        _default_parser = YDBParser()
    return _default_parser


def parse_mumps(code: str, cleanup: bool = True) -> Dict:
    """
    Parse MUMPS code and return the AST (convenience function).
    
    Args:
        code: String containing MUMPS code to parse
        cleanup: If True, delete the temporary JSON file after parsing
    
    Returns:
        Dictionary containing the AST
    
    Raises:
        YDBParserError: If parsing fails
    
    Example:
        >>> ast = parse_mumps('write "Hello, World!",!')
        >>> print(ast['triples'][0]['opcode'])
        OC_LINEFETCH
    """
    parser = get_parser()
    return parser.parse(code, cleanup=cleanup)


def parse_mumps_file(filename: Union[str, Path], cleanup: bool = True) -> Dict:
    """
    Parse MUMPS code from a file (convenience function).
    
    Args:
        filename: Path to MUMPS source file
        cleanup: If True, delete the temporary JSON file after parsing
    
    Returns:
        Dictionary containing the AST
    
    Raises:
        FileNotFoundError: If the file doesn't exist
        YDBParserError: If parsing fails
    
    Example:
        >>> ast = parse_mumps_file('hello.m')
        >>> print(len(ast['triples']))
        5
    """
    parser = get_parser()
    return parser.parse_file(filename, cleanup=cleanup)


__all__ = [
    'YDBParser',
    'YDBParserError',
    'parse_mumps',
    'parse_mumps_file',
    'get_parser',
]
