"""
Setup script for ydb_parser Python package.

This package provides Python bindings for parsing MUMPS code using YottaDB's
compiler and returning Abstract Syntax Tree (AST) representations.

Installation:
    pip install .

Usage:
    from ydb_parser import parse_mumps
    
    ast = parse_mumps('write "Hello, World!",!')
"""

from setuptools import setup, find_packages
from setuptools.command.build_py import build_py
from pathlib import Path
import shutil
import os

# Read the README file
this_directory = Path(__file__).parent
long_description = ""
readme_path = this_directory / "README.md"
if readme_path.exists():
    long_description = readme_path.read_text(encoding='utf-8')


class BuildWithLibrary(build_py):
    """Custom build command to bundle libyottadb.so"""
    
    def run(self):
        # Run standard build first
        build_py.run(self)
        
        # Find libyottadb.so
        lib_locations = [
            os.environ.get('ydb_dist'),
            '/opt/yottadb/current',
            str(Path(__file__).parent.parent / 'build'),
        ]
        
        libyottadb = None
        for loc in lib_locations:
            if not loc:
                continue
            lib_path = Path(loc) / 'libyottadb.so'
            if lib_path.exists():
                libyottadb = lib_path
                break
        
        if libyottadb:
            # Copy to build directory
            target_dir = Path(self.build_lib) / 'ydb_parser' / 'lib'
            target_dir.mkdir(parents=True, exist_ok=True)
            target_file = target_dir / 'libyottadb.so'
            shutil.copy2(libyottadb, target_file)
            print(f"Bundled {libyottadb} -> {target_file}")
        else:
            print("Warning: libyottadb.so not found. Package will require system installation.")


setup(
    name='ydb-parser',
    version='0.1.0',
    author='YottaDB LLC',
    author_email='info@yottadb.com',
    description='Python bindings for parsing MUMPS code with YottaDB',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://gitlab.com/YottaDB/DB/YDB',
    packages=find_packages(),
    package_data={
        'ydb_parser': ['lib/*.so', 'lib/*.dylib'],
    },
    cmdclass={
        'build_py': BuildWithLibrary,
    },
    classifiers=[
        'Development Status :: 3 - Alpha',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Compilers',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'License :: OSI Approved :: GNU Affero General Public License v3',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Operating System :: POSIX :: Linux',
        'Operating System :: MacOS',
    ],
    python_requires='>=3.7',
    install_requires=[
        # No additional dependencies - uses ctypes from stdlib
    ],
    extras_require={
        'dev': [
            'pytest>=7.0',
            'pytest-cov>=3.0',
            'black>=22.0',
            'mypy>=0.950',
        ],
    },
    keywords='mumps parser yottadb ast compiler',
    project_urls={
        'Bug Reports': 'https://gitlab.com/YottaDB/DB/YDB/-/issues',
        'Source': 'https://gitlab.com/YottaDB/DB/YDB',
        'Documentation': 'https://docs.yottadb.com',
    },
    entry_points={
        'console_scripts': [
            # Add CLI tools if needed in the future
        ],
    },
)
