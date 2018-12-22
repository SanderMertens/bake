# driver-bake-c
This is the C language plugin for the bake buildtool.

## Installation
This plugin is installed by default when installing bake. To install bake, run
the following command:

```
curl https://corto.io/install-bake | sh
```

## Project configuration
The C plugin supports properties specific to C projects. These properties are specified in the "value" section of the `project.json` file. The following properties are made available by the C plugin:

Property | Type | Description
---------|------|------------
cflags   | list[string] | List of arguments to pass to the C compiler
cxxflags | list[string] | List of arguments to pass to the C++ compiler
ldflags | list[string] | List of arguments to pass to the linker
lib | list[string] | List of library names to link with
static_lib | list[string] | List of static libraries to link with.
libpath | list[string] |
link | list[string] | List of objects and (static) library files to provide to the linker.
include | list[string] | List of paths to look for include files
dylib | bool | Link binary as a dylib instead of an .so (ignored if not OSX)
