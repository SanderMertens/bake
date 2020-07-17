# bake-lang-c
This is the C language plugin for the bake buildtool.

## Project configuration
The C plugin supports properties specific to C projects. These properties are specified in the "lang.c" section of a `project.json` file. The following properties are made available by the C plugin:

Property | Type | Description
---------|------|------------
cflags   | list[string] | List of arguments to pass to the C compiler
cxxflags | list[string] | List of arguments to pass to the C++ compiler
defines  | list[string] | List of preprocessor defines to add to the compiler
ldflags | list[string] | List of arguments to pass to the linker
lib | list[string] | List of library names to link with
static-lib | list[string] | List of static libraries to link with.
libpath | list[string] | Linker library path
link | list[string] | List of objects and (static) library files to provide to the linker.
include | list[string] | List of paths to look for include files
static | bool | Create static library (packages only, default=false)
c-standard | string | Specify C standard (default=c99)
cpp-standard | string | Specify C++ standard (default=c++0x)
export-symbols | bool | Export all library symbols (default=false)
precompile-header | bool | Precompile main project header (default=true)

## Example

```json
{
    "id": "my_lib",
    "type": "package",
    "lang.c": {
        "export-symbols": true
    }
}
```
