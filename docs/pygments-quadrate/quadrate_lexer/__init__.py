"""Pygments lexer for the Quadrate programming language."""

from pygments.lexer import RegexLexer, bygroups, words
from pygments.token import (
    Comment, Keyword, Name, Number, Operator, Punctuation, String, Text, Whitespace
)

__all__ = ['QuadrateLexer']


class QuadrateLexer(RegexLexer):
    """Pygments lexer for Quadrate, a stack-based programming language."""

    name = 'Quadrate'
    aliases = ['quadrate', 'qd']
    filenames = ['*.qd']
    mimetypes = ['text/x-quadrate']

    # Keywords
    keywords = (
        'fn', 'pub', 'const', 'struct', 'use',
        'if', 'else', 'for', 'loop', 'while', 'break', 'continue', 'return',
        'defer', 'switch', 'default', 'ctx', 'with', 'unless',
        'true', 'false', 'iter',
    )

    # Built-in stack operations
    builtins_stack = (
        'dup', 'dup2', 'dupd', 'drop', 'drop2',
        'swap', 'swap2', 'swapd',
        'over', 'over2', 'overd',
        'rot', 'nip', 'nipd', 'tuck',
        'pick', 'roll', 'clear', 'depth',
    )

    # Built-in arithmetic
    builtins_arith = (
        'add', 'sub', 'mul', 'div', 'mod',
        'neg', 'inc', 'dec', 'abs',
    )

    # Built-in comparison
    builtins_cmp = (
        'eq', 'neq', 'lt', 'lte', 'gt', 'gte', 'within',
    )

    # Built-in bitwise
    builtins_bit = (
        'and', 'or', 'xor', 'not', 'shl', 'shr',
    )

    # Built-in array operations
    builtins_array = (
        'makei', 'makef', 'makes', 'makep', 'make',
        'len', 'nth', 'set', 'append', 'free',
    )

    # Built-in type casting
    builtins_cast = (
        'casti', 'castf', 'casts',
    )

    # Built-in I/O
    builtins_io = (
        'print', 'printv', 'prints', 'printsv', 'nl', 'read',
    )

    # Built-in misc
    builtins_misc = (
        'error', 'spawn', 'wait', 'detach', 'call', 'abort',
    )

    # Math functions (often used)
    builtins_math = (
        'sqrt', 'sin', 'cos', 'tan', 'asin', 'acos', 'atan', 'atan2',
        'exp', 'log', 'log10', 'pow', 'floor', 'ceil', 'round',
    )

    # Types
    types = (
        'i64', 'f64', 'str', 'ptr', 'bool',
    )

    tokens = {
        'root': [
            # Whitespace
            (r'\s+', Whitespace),

            # Comments
            (r'//.*$', Comment.Single),
            (r'///.*$', Comment.Doc),

            # Strings
            (r'"', String.Double, 'string'),

            # Character literals
            (r"'\\.'", String.Char),
            (r"'.'", String.Char),

            # Numbers
            (r'0b[01_]+', Number.Bin),
            (r'0x[0-9a-fA-F_]+', Number.Hex),
            (r'-?\d+\.\d+', Number.Float),
            (r'-?\d+', Number.Integer),

            # Function definition
            (r'(fn)(\s+)(\w+)', bygroups(Keyword, Whitespace, Name.Function)),

            # Struct definition
            (r'(struct)(\s+)(\w+)', bygroups(Keyword, Whitespace, Name.Class)),

            # Module usage
            (r'(use)(\s+)(\w+)', bygroups(Keyword.Namespace, Whitespace, Name.Namespace)),

            # Module function call (module::function)
            (r'(\w+)(::)(\w+)', bygroups(Name.Namespace, Punctuation, Name.Function)),

            # Function pointer
            (r'&\w+', Name.Function),

            # Variable binding arrow
            (r'->', Keyword),

            # Case arrow in switch
            (r'=>', Punctuation),

            # Operators
            (r'[+\-*/%]', Operator),
            (r'[<>=!]=?', Operator),

            # Field access/set
            (r'@\w+', Name.Attribute),
            (r'!\w+', Name.Attribute),
            (r'@\[\]', Operator),
            (r'!\[\]', Operator),

            # Array type syntax
            (r'\w+\[\]', Keyword.Type),

            # Keywords
            (words(keywords, suffix=r'\b'), Keyword),

            # Types
            (words(types, suffix=r'\b'), Keyword.Type),

            # Builtins
            (words(builtins_stack, suffix=r'\b'), Name.Builtin),
            (words(builtins_arith, suffix=r'\b'), Name.Builtin),
            (words(builtins_cmp, suffix=r'\b'), Name.Builtin),
            (words(builtins_bit, suffix=r'\b'), Name.Builtin),
            (words(builtins_array, suffix=r'\b'), Name.Builtin),
            (words(builtins_cast, suffix=r'\b'), Name.Builtin),
            (words(builtins_io, suffix=r'\b'), Name.Builtin),
            (words(builtins_misc, suffix=r'\b'), Name.Builtin),
            (words(builtins_math, suffix=r'\b'), Name.Builtin),

            # Punctuation
            (r'[{}()\[\],:]', Punctuation),

            # Identifiers (variables, function names)
            (r'\w+', Name),
        ],
        'string': [
            (r'\\[nrt\\"]', String.Escape),
            (r'[^"\\]+', String.Double),
            (r'"', String.Double, '#pop'),
        ],
    }
