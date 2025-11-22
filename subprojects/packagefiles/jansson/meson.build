project('jansson', 'c',
    version: '2.14.1',
    default_options: [
        'c_std=c11',
        'warning_level=2',
    ]
)

cc = meson.get_compiler('c')
# Get C++ compiler if available (for export compile args)
has_cpp = add_languages('cpp', required: false, native: false)
if has_cpp
    cxx = meson.get_compiler('cpp')
endif

# Configuration data for jansson_config.h
conf_data = configuration_data()

# Version information
conf_data.set_quoted('JANSSON_VERSION', '2.14.1')
conf_data.set('JANSSON_VERSION_MAJOR', 2)
conf_data.set('JANSSON_VERSION_MINOR', 14)
conf_data.set('JANSSON_VERSION_MICRO', 1)

# Check for headers
headers = [
    'endian.h',
    'fcntl.h',
    'locale.h',
    'sys/stat.h',
    'sys/time.h',
    'sys/types.h',
    'unistd.h',
]

foreach h : headers
    if cc.has_header(h)
        conf_data.set('HAVE_@0@'.format(h.underscorify().to_upper()), 1)
    endif
endforeach

# Check for functions
functions = [
    'close',
    'getpid',
    'gettimeofday',
    'localeconv',
    'open',
    'read',
    'setlocale',
    'strtoll',
]

foreach f : functions
    if cc.has_function(f)
        conf_data.set('HAVE_@0@'.format(f.to_upper()), 1)
    endif
endforeach

# Check for integer types
int32_type = 'int32_t'
uint32_type = 'uint32_t'
uint16_type = 'uint16_t'
uint8_type = 'uint8_t'

if cc.sizeof('int32_t', prefix: '#include <stdint.h>') == 4
    conf_data.set('JSON_INT32', 'int32_t')
else
    conf_data.set('JSON_INT32', 'int')
endif

if cc.sizeof('uint32_t', prefix: '#include <stdint.h>') == 4
    conf_data.set('JSON_UINT32', 'uint32_t')
else
    conf_data.set('JSON_UINT32', 'unsigned int')
endif

if cc.sizeof('uint16_t', prefix: '#include <stdint.h>') == 2
    conf_data.set('JSON_UINT16', 'uint16_t')
else
    conf_data.set('JSON_UINT16', 'unsigned short')
endif

if cc.sizeof('uint8_t', prefix: '#include <stdint.h>') == 1
    conf_data.set('JSON_UINT8', 'uint8_t')
else
    conf_data.set('JSON_UINT8', 'unsigned char')
endif

# Check for 64-bit integer support
if cc.sizeof('long long') == 8
    conf_data.set('JSON_INT_T', 'long long')
    conf_data.set('JSON_INTEGER_IS_LONG_LONG', 1)
    conf_data.set_quoted('JSON_INTEGER_FORMAT', '"lld"')
    conf_data.set('JSON_STRTOINT', 'strtoll')
elif cc.sizeof('int64_t', prefix: '#include <stdint.h>') == 8
    conf_data.set('JSON_INT_T', 'int64_t')
    conf_data.set_quoted('JSON_INTEGER_FORMAT', '"' + 'PRId64' + '"')
    conf_data.set('JSON_STRTOINT', 'strtoll')
else
    conf_data.set('JSON_INT_T', 'long')
    conf_data.set_quoted('JSON_INTEGER_FORMAT', '"ld"')
    conf_data.set('JSON_STRTOINT', 'strtol')
endif

# Check for ssize_t
if cc.has_type('ssize_t', prefix: '#include <sys/types.h>')
    conf_data.set('JSON_SSIZE', 'ssize_t')
else
    conf_data.set('JSON_SSIZE', 'long')
endif

# Check for inline keyword (for jansson_config.h.in)
if cc.compiles('static inline int foo(void) { return 0; }')
    conf_data.set('json_inline', 'inline')
elif cc.compiles('static __inline int foo(void) { return 0; }')
    conf_data.set('json_inline', '__inline')
elif cc.compiles('static __inline__ int foo(void) { return 0; }')
    conf_data.set('json_inline', '__inline__')
else
    conf_data.set('json_inline', '')
endif

# Check for long long support (for jansson_config.h.in)
if cc.sizeof('long long') == 8
    conf_data.set('json_have_long_long', 1)
else
    conf_data.set('json_have_long_long', 0)
endif

# Check for atomic builtins (for jansson_config.h.in)
if cc.has_function('__sync_val_compare_and_swap', prefix: '#include <stdint.h>')
    conf_data.set('json_have_sync_builtins', 1)
else
    conf_data.set('json_have_sync_builtins', 0)
endif

if cc.has_function('__atomic_load', prefix: '#include <stdint.h>')
    conf_data.set('json_have_atomic_builtins', 1)
else
    conf_data.set('json_have_atomic_builtins', 0)
endif

# Check endianness
if host_machine.endian() == 'big'
    conf_data.set('WORDS_BIGENDIAN', 1)
endif

# Hash seed source (prefer /dev/urandom on Unix-like systems)
if host_machine.system() != 'windows'
    conf_data.set('USE_URANDOM', 1)
else
    conf_data.set('USE_WINDOWS_CRYPTOAPI', 1)
endif

# Generate jansson_config.h
jansson_config_h = configure_file(
    input: 'src/jansson_config.h.in',
    output: 'jansson_config.h',
    configuration: conf_data
)

# Configuration for jansson_private_config.h
private_conf = configuration_data()

# Set all HAVE_* defines based on our checks
foreach h : headers
    if cc.has_header(h)
        private_conf.set('HAVE_@0@'.format(h.underscorify().to_upper()), 1)
    endif
endforeach

# Additional headers for private config
if cc.has_header('sched.h')
    private_conf.set('HAVE_SCHED_H', 1)
endif
if cc.has_header('sys/param.h')
    private_conf.set('HAVE_SYS_PARAM_H', 1)
endif
if cc.has_header('stdint.h')
    private_conf.set('HAVE_STDINT_H', 1)
endif

# Function checks
foreach f : functions
    if cc.has_function(f)
        private_conf.set('HAVE_@0@'.format(f.to_upper()), 1)
    endif
endforeach

# Additional functions
if cc.has_function('sched_yield', prefix: '#include <sched.h>')
    private_conf.set('HAVE_SCHED_YIELD', 1)
endif

# Atomic builtins
if cc.has_function('__sync_val_compare_and_swap', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_SYNC_BUILTINS', 1)
endif
if cc.has_function('__atomic_load', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_ATOMIC_BUILTINS', 1)
endif

# Integer type checks
if cc.has_type('int32_t', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_INT32_T', 1)
endif
if cc.has_type('uint32_t', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_UINT32_T', 1)
endif
if cc.has_type('uint16_t', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_UINT16_T', 1)
endif
if cc.has_type('uint8_t', prefix: '#include <stdint.h>')
    private_conf.set('HAVE_UINT8_T', 1)
endif
if cc.has_type('ssize_t', prefix: '#include <sys/types.h>')
    private_conf.set('HAVE_SSIZE_T', 1)
endif

# Fallback type definitions
private_conf.set('JSON_INT32', conf_data.get('JSON_INT32'))
private_conf.set('JSON_UINT32', conf_data.get('JSON_UINT32'))
private_conf.set('JSON_UINT16', conf_data.get('JSON_UINT16'))
private_conf.set('JSON_UINT8', conf_data.get('JSON_UINT8'))
private_conf.set('JSON_SSIZE', conf_data.get('JSON_SSIZE'))

# Endianness
if host_machine.endian() == 'big'
    private_conf.set('WORDS_BIGENDIAN', 1)
endif

# Random source
if host_machine.system() != 'windows'
    private_conf.set('USE_URANDOM', 1)
else
    private_conf.set('USE_WINDOWS_CRYPTOAPI', 1)
endif

# Enable dtoa for better float conversion
private_conf.set('USE_DTOA', 1)

# Initial hashtable order
private_conf.set('JANSSON_INITIAL_HASHTABLE_ORDER', 3)

# Generate jansson_private_config.h
jansson_private_config_h = configure_file(
    input: 'jansson_private_config.h.in',
    output: 'jansson_private_config.h',
    configuration: private_conf
)

# Source files
jansson_sources = [
    jansson_config_h,
    jansson_private_config_h,
]
jansson_sources += files(
    'src/dump.c',
    'src/error.c',
    'src/hashtable.c',
    'src/hashtable_seed.c',
    'src/load.c',
    'src/memory.c',
    'src/pack_unpack.c',
    'src/strbuffer.c',
    'src/strconv.c',
    'src/utf.c',
    'src/value.c',
    'src/version.c',
)

# Add dtoa.c conditionally (for better floating-point conversion)
jansson_sources += files('src/dtoa.c')

# Include directories
jansson_inc = include_directories('src', '.')

# Compiler arguments
jansson_c_args = []

# Define HAVE_CONFIG_H so that source files include jansson_private_config.h
jansson_c_args += ['-DHAVE_CONFIG_H']

# Add build directory to include path for generated config headers
jansson_c_args += ['-I' + meson.current_build_dir()]

# Suppress format-truncation warnings for GCC
if cc.get_id() == 'gcc'
    if cc.has_argument('-Wno-format-truncation')
        jansson_c_args += ['-Wno-format-truncation']
    endif
endif

# Suppress unused function warnings
if cc.has_argument('-Wno-unused-function')
    jansson_c_args += ['-Wno-unused-function']
endif

# Suppress volatile warnings (deprecated in newer GCC/Clang)
if cc.has_argument('-Wno-volatile')
    jansson_c_args += ['-Wno-volatile']
endif

# Build static library
jansson_lib = static_library('jansson',
    jansson_sources,
    include_directories: jansson_inc,
    c_args: jansson_c_args,
    install: false
)

# Compile arguments to export to users of this library
# This suppresses warnings from jansson.h when included in C++ code
jansson_export_args = []

# Suppress volatile warnings (deprecated increment/decrement of volatile)
if cc.has_argument('-Wno-error=volatile')
    jansson_export_args += ['-Wno-error=volatile']
elif cc.has_argument('-Wno-volatile')
    jansson_export_args += ['-Wno-volatile']
endif

# Check C++ compiler for additional warnings
if has_cpp
    # Suppress deprecated volatile warnings
    if cxx.has_argument('-Wno-error=deprecated-volatile')
        if '-Wno-error=deprecated-volatile' not in jansson_export_args
            jansson_export_args += ['-Wno-error=deprecated-volatile']
        endif
    elif cxx.has_argument('-Wno-deprecated-volatile')
        if '-Wno-deprecated-volatile' not in jansson_export_args
            jansson_export_args += ['-Wno-deprecated-volatile']
        endif
    endif

    # Suppress old-style cast warnings (C library used in C++ code)
    if cxx.has_argument('-Wno-old-style-cast')
        jansson_export_args += ['-Wno-old-style-cast']
    endif
endif

# Declare dependency
jansson_dep = declare_dependency(
    link_with: jansson_lib,
    include_directories: jansson_inc,
    compile_args: jansson_export_args
)
