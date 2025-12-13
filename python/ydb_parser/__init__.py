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
import tempfile
import threading
import uuid
from pathlib import Path
from typing import Dict, List, Optional, Union

# Global lock for thread safety - the underlying C library is not thread-safe
# as it uses global state (cmd_qlf, source_file_name, routine_name, etc.)
_parser_lock = threading.Lock()


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
        
        # int ydb_parse_mumps_file_to_json(
        #     const char *mumps_filename,
        #     char *json_filename_out,
        #     size_t filename_len,
        #     char *error_out,
        #     size_t error_len
        # )
        self.lib.ydb_parse_mumps_file_to_json.argtypes = [
            ctypes.c_char_p,  # mumps_filename
            ctypes.c_char_p,  # json_filename_out
            ctypes.c_size_t,  # filename_len
            ctypes.c_char_p,  # error_out
            ctypes.c_size_t,  # error_len
        ]
        self.lib.ydb_parse_mumps_file_to_json.restype = ctypes.c_int
    
    def parse(self, code: str, cleanup: bool = True) -> Dict:
        """
        Parse MUMPS code and return the AST as a Python dictionary.
        
        For multi-line code, this method automatically writes to a temporary
        file and uses the full compiler pipeline for accurate parsing.
        
        This method is thread-safe - concurrent calls from multiple threads
        will be serialized using a global lock.
        
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
        
        # For multi-line code, use the file-based approach for better accuracy
        if '\n' in code:
            # Write code to a temporary file with unique name using UUID
            unique_id = uuid.uuid4().hex[:12]
            temp_dir = tempfile.gettempdir()
            temp_file = os.path.join(temp_dir, f"ydb_parse_{unique_id}.m")
            
            try:
                with open(temp_file, 'w', encoding='utf-8') as tf:
                    tf.write(code)
                
                # Parse using the file-based method
                ast = self.parse_file(temp_file, cleanup=cleanup)
                return ast
            finally:
                # Clean up temporary MUMPS file
                try:
                    if os.path.exists(temp_file):
                        os.unlink(temp_file)
                except OSError:
                    pass
        
        # Single-line code: use the string-based API
        # Acquire lock because the C library has global state
        with _parser_lock:
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
            
            # Read and parse the JSON file while still holding the lock
            try:
                with open(json_filename, 'r', encoding='utf-8') as f:
                    ast = json.load(f)
            except FileNotFoundError:
                raise YDBParserError(f"JSON file not found: {json_filename}")
            except json.JSONDecodeError as e:
                raise YDBParserError(f"Invalid JSON in {json_filename}: {e}")
            finally:
                # Clean up the temporary JSON file
                if cleanup and os.path.exists(json_filename):
                    try:
                        os.unlink(json_filename)
                    except OSError:
                        pass  # Ignore cleanup errors
        
        return ast
    
    def parse_file(self, mumps_file: Union[str, Path], cleanup: bool = True) -> Dict:
        """
        Parse a MUMPS source file and return the AST as a Python dictionary.
        
        This method uses the full compiler pipeline and properly handles
        multi-line MUMPS routines.
        
        This method is thread-safe - concurrent calls from multiple threads
        will be serialized using a global lock.
        
        Args:
            mumps_file: Path to MUMPS source file (.m file)
            cleanup: If True, delete the temporary JSON file after parsing
        
        Returns:
            Dictionary containing the AST with keys:
                - ast_type: Always "MUMPS"
                - source_file: Name of the MUMPS source file
                - triples: List of AST nodes (triples)
        
        Raises:
            YDBParserError: If parsing fails
            FileNotFoundError: If the MUMPS file doesn't exist
            
        Example:
            >>> parser = YDBParser()
            >>> ast = parser.parse_file('routine.m')
            >>> print(len(ast['triples']))
            25
        """
        mumps_file = str(mumps_file)
        
        if not mumps_file.strip():
            raise ValueError("mumps_file cannot be empty")
        
        # Check if file exists
        if not os.path.exists(mumps_file):
            raise FileNotFoundError(f"MUMPS file not found: {mumps_file}")
        
        # Acquire lock because the C library has global state
        with _parser_lock:
            # Prepare buffers for output
            filename_buf = ctypes.create_string_buffer(1024)
            error_buf = ctypes.create_string_buffer(4096)
            
            # Call the C function
            result = self.lib.ydb_parse_mumps_file_to_json(
                mumps_file.encode('utf-8'),
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
            
            # Read and parse the JSON file while still holding the lock
            try:
                with open(json_filename, 'r', encoding='utf-8') as f:
                    ast = json.load(f)
            except FileNotFoundError:
                raise YDBParserError(f"JSON file not found: {json_filename}")
            except json.JSONDecodeError as e:
                raise YDBParserError(f"Invalid JSON in {json_filename}: {e}")
            finally:
                # Clean up the temporary JSON file
                if cleanup and os.path.exists(json_filename):
                    try:
                        os.unlink(json_filename)
                    except OSError:
                        pass  # Ignore cleanup errors
        
        return ast


# Module-level convenience functions

_default_parser: Optional[YDBParser] = None
_default_parser_lock = threading.Lock()


def get_parser() -> YDBParser:
    """
    Get or create the default YDBParser instance.
    
    This function is thread-safe - concurrent calls will not create
    multiple parser instances.
    
    Returns:
        The default parser instance
    """
    global _default_parser
    # Double-checked locking pattern
    if _default_parser is None:
        with _default_parser_lock:
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
