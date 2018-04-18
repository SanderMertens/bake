# bake
Bake is an opinionated buildtool that dramatically reduces the complexity of a build configuration by imposing a well-defined structure upon projects. This is great for two reasons:
1. Bake projects look the same which makes it easy to share and read code.
2. Bake configuration can be reduced to a simple declarative JSON file.

## Overview
The motivation behind bake was to create a buildtool that is optimized for codebases that consist of many projects with complex interdependencies. A secondary goal was to create a zero-dependency buildtool that can be easily ported to other platforms.

To simplify working with many projects, bake automatically discovers projects in the current folder and its subfolders. It then parses the dependencies for all discovered projects, and build the projects in the correct dependency order.

Besides building code, bake also can:

- Install project binaries to a common location so they can be located by their logical name
- Install miscellaneous files to a common location (like configuration, HTML/CSS/JS) that can be accessed regardless from where the code is ran
- Automatically discover projects in a directory tree
- Build discovered projects in correct dependency order
- Specify configurations on a global, project-specific or any level in between
- Automatically set environment variables specified in a configuration file

## Usage
The following examples show how to use the bake command:

```
bake [flags]
bake [flags] <directory>
bake [flags] <command>
bake [flags] <command> <directory>
```

The following commands are valid for bake:

Command | Description
--------|------------
build   | Build the project (default)
rebuild | Rebuild the project
clean   | Clean the project
env     | Return bake environment
foreach | Run command for every discovered project
install | Install project files to $BAKE_TARGET

The following flags can be used to specify parameters for a project on the command line, when no `project.json` is available:

Flag | Description
-----|--------------
--id | Manually set project id
-k,--kind | Specify project kind
-i,--includes | Specify include paths, separated by comma
-s,--sources | Specify source paths, separated by comma
-l,--lang | Specify programming language
-a,--artefact | Specify artefact name
-u,--use | Specify dependencies
--managed | If `true`, project is managed. Managed projects integrate with corto code generation.
--local | If `true`, project is not installed to $BAKE_TARGET

The following flags control the logging framework:

Flag | Description
-----|--------------
--debug | DEBUG level tracing
--trace | TRACE level tracing
--ok | OK level tracing
--warning | WARNING level tracing
--error | ERROR level tracing
--profile | Show all corto_log_push and corto_log_pop transitions. Combine with `--show-delta` to profile a build.
--dont-mute-foreach | Don't mute output when using the foreach command.
--show-time | Show time for each trace
--show-delta | Show time delta for each trace
--show-proc | Show process id for each trace
--exit-on-exception | Exit when an exception occurs
--abort-on-exception | Abort when an exception occurs

The following flags control configuration and environment:

Flag | Description
-----|--------------
--env | Specify environment to load (default = 'default')
--cfg | Specify configuration to load (default = 'default')
--no-symbols | Disable symbols
--no-debug | Disable debug build
--optimize | Enable optimizations
--coverage | Enable code coverage
--strict | Enable strict mode (strictest compiler flags)

Miscellaneous flags:

Flag | Description
-----|--------------
-p,--path | Specify search paths, separated by comma
--skip-preinstall | Skip install step
--skip-postintall | Skip copying binary to $BAKE_TARGET
--do | Specify action when using `bake foreach`

## Project files
Bake project files are located in the root of a project, and are called `project.json`. This is a minimal example of a bake project file that builds an shared object. With this configuration, the project will be built with all values set to their defaults.

```json
{
    "id": "my_library",
    "type": "library"
}
```

This example shows how to specify dependencies and specify additional flags:

```json
{
    "id": "my_application",
    "type": "application",
    "value": {
        "use": ["my_library"],
        "cflags": ["-DHELLO_WORLD"]
    }
}
```

In this example, if `my_library` is a project that is discovered by bake, it will be built before `my_application`.

The following list of properties can be used for all bake-based projects. Plugins may support additional properties. See the plugin documentation for more details.

Property | Type | Description
---------|------|-------
language | string | Language of the project. Is used to select a bake plugin.
version | string | Version of the project (use semantic versioning)
managed | bool | Is project managed or unmanaged
public | bool | If `true`, project is installed to `$BAKE_TARGET`
use | list(string) | List of dependencies using logical project ids. Dependencies must be located in either `$BAKE_HOME` or `$BAKE_TARGET`.
dependee | object | Properties inside the `dependee` object will be passed on to dependees of this project. This allows a project to specify, for example, include paths that its dependee should use.
sources | list(string) | List of paths that contain source files. Default is `src`. The `$SOURCES` rule is substituted with this value.
includes | list(string) | List of paths that contain include files.
use_generated_api | bool | For managed projects only. For each project in `use`, add a project with id `$project/$language`, if it exists.

## Plugins and rules
Bake has a plugin architecture, where a plugin describes how code should be built for a particular language. Bake plugins are essentially parameterized makefiles, with the only difference that they are written in C, and that they use the bake build engine. Plugins allow you to define how projects should be built once, and then reuse it for every project. Plugins can be created for any language.

The bake build engine has a design that is similar to other build engines in that it uses rules that depend on other rules. Rules have rule-actions, which get executed when a rule is outdated. Whether a rule is outdated or not is determined by comparing timestamps of the rule dependencies with the timestamps of the rule output.

Rules are written in their respective language plugins in C. A simple set of rules that builds a binary from a set of source files would look like this:

```c
l->pattern("SOURCES", "//*.c|*.cpp|*.cxx");
l->rule("objects", "$SOURCES", l->target_map(src_to_obj), compile_src);
l->rule("ARTEFACT", "$objects", l->target_pattern(NULL), link_binary);
```

Patterns create a label for a pattern (using the corto `idmatch` syntax). Rules are patterns that have dependencies and actions. The syntax for a rule is:

```c
l->rule(<id>, <dependencies>, <function to map target to output>, <action>);
```

Each plugin must have a `bakemain` entrypoint. This function is called when the
plugin is loaded, and must specify the rules and patterns.

## Environment variables
Bake uses the following environment variables:

Variable | Description
---------|------------
BAKE_HOME | Location where bake looks for projects specified in `use`.
BAKE_TARGET | Location where bake installs projects. Usually the same as `$BAKE_HOME`.
BAKE_VERSION | Framework version that allows for storing packages of multiple major.minor framework versions in the same `$BAKE_HOME`. Usually set to the corto version.

## Environment and Configuration
Bake can be configured with configuration files that specify the environment in which bake should run and the build configuration that should be used. Bake locates a bake configuration file by traveling upwards from the current working directory, and looking for a `.bake`, `bake` or `bake/config.json` file. If multiple files are found, they are applied in reverse order, so that the file that is "closest" to the project takes precedence.

A bake configuration file consists out of an `environment` and a `configuration` section. The `configuration` section contains parameters that are not specific to a project, but influence how code is built. The `environment` section contains a list of environment variables and their values which are loaded when bake is started.

The `bake env` command prints the bake environment to the command line in a format that can be used with the `export` bash command, so that the bake environment can be easily exported to the current shell, like so:

```
export `bake env`
```

The following table is a list of the configuration parameters:

Parameter | Type | Description
----------|------|------------
symbols | bool | Enable or disable symbols in binaries
debug | bool | Enable or disable debugging (defines NDEBUG if `false`)
optimizations | bool | Enable or disable optimizations
coverage | bool | Enable or disable coverage
strict | bool | Enable or disable strict building

Note that plugins may not support all parameters.

This is an example configuration file:

```json
{
    "configuration":{
        "debug":{
            "symbols":true,
            "debug":true,
            "optimizations":false,
            "coverage":false,
            "strict":false
        },
        "release":{
            "symbols":false,
            "debug":false,
            "optimizations":true,
            "coverage":false,
            "strict":false
        }
    },
    "environment":{
        "default":{
            "BAKE_HOME":"~/.corto",
            "BAKE_TARGET":"~/.corto",
            "BAKE_VERSION":"2.0",
        }
    }
}
```

With the `--cfg` and `--env` flags the respective configuration or environment can be selected.

## Authors
- Sander Mertens - Initial work

## Legal stuff
Bake is licensed under the MIT license.
