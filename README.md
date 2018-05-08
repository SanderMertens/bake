# bake
Bake is a buildtool (like `make`) and buildsystem in one that dramatically reduces the complexity of building code, in exchange for enforcing a rigid structure upon projects. This is great for two reasons:

1. All bake projects look the same which makes it easy to share and read code.
2. Bake just needs a simple, declarative JSON file to build a project

## Installation
Install bake using the following command:

```demo
curl https://corto.io/install-bake | sh
```

This installs the bake executable to your home directory. Additionally the installer creates a script in `/usr/local/bin` that lets you run bake from anywhere. To be able to write to `/usr/local/bin`, **the installer may ask for your password**.

The C language plugin is installed by default. For C-specific configuration parameters, see the README file in the plugin repository: https://github.com/cortoproject/driver-bake-c.

## Getting started

### Your first bake project
To create a new bake project called `my_app`, run the following commands:

```demo
mkdir my_app
cd my_app
bake init
```

This will have created an simple executable project with the following `project.json` configuation file:

```json
{
    "id":"my_app",
    "type":"executable"
}
```

You'll find a `my_app.c` and `my_app.h` in the newly created `src` and `include` directories. To build the project, run bake with this command:

```demo
bake
```

You can now run the project with this command:

```demo
bin/<platform-config>/my_app
```

### Useful commands

#### Export bake environment
When bake builds executables, binaries are installed to a special location that can be accessed from any location, as long as the bake environment is exported. You can easily do this by adding this line to your `.bashrc` file:

```demo
export `bake env`
```

#### List public projects for environment
When building public projects (see: "Bake Projects"), you can use the following command to get a quick overview of the projects that are installed to your current bake environment:

```demo
corto list
```

#### Find paths for a project
If you want to quickly check where a project stores its include files, binary or miscellaneous files, you can use this command:

```demo
corto locate my_project --all
```

#### Do a release build
By default, bake will build projects in debug mode, which disables optimizations and enables symbols. If you want to build projects in release build, specifiy the `release` configuration, like this:

```demo
bake --cfg release
```

Note that you can add/modify/remove bake configurations in the bake configuration file.

#### Do a standalone build
By default, bake generates binaries that cannot be shared between machines, because they use hard-coded paths and are stored in your home directory. To generate binary distributions that can be shared across machines, use this command:

```demo
bake --standalone --standalone-path my_distro
```

#### Debug bake
Can't figure out why bake is doing something (or not doing something)? By enabling tracing, you can get much more information out of bake. Try this command:

```demo
bake --trace
```

Or if that is not enough, try this one:

```demo
bake --debug
```

## Bake Manual

### Introduction
The goal of bake is to bring a level of abstraction to building software that is comparable with `npm`. Tools like `make`, `cmake` and `premake` abstract away from writing your own compiler commands by hand, but still require users to create their own buildsystem, with proprietary mechanisms for specifying dependencies, build configurations etc.

This makes it difficult to share code between different people and organizations, and is arguably one of the reasons why ecosystems like `npm` are thriving, while ecosystems for native code are very fragmented.

Bake is therefore not just a buildtool like `make` that can automatically generate compiler commands. It is also a buildsystem that clearly specifies how projects are organized and configured. When a project relies on bake, a user does, for example, not need to worry about how to link with it, where to find its include files or whether binaries have been built with incompatible compiler flags.

A secondary goal is to create a zero-dependency buildtool that can be easily ported to other platforms. Whereas other buildtools exist, like `make`, `premake`, `rake` and `gradle`, they all rely on their respective ecosystems (`unix`, `lua`, `ruby`, `java`) which complicates writing platform-independent build configurations.

### Feature Overview
Bake also addresses a number of issues commonly found during building. Here is a (non-exhaustive) overview of bake features:

- Automatically discover projects in a directory tree
- Build discovered projects in correct dependency order
- Manage custom build configurations (`debug`, `release`, ...) on a global or project-specific level
- Manage environment variables
- Install project binaries to a common location so they can be located by their logical name
- Install miscellaneous files to a common location (like configuration, HTML/CSS/JS) that can be accessed regardless from where the code is ran
- Integration with corto package management tools

### Project Kinds
Bake supports different project kinds which are configured in the `type` property ofa bake `project.json` file. The project kind determines whether a project is a library or executable, whether a project is installed to a bake environment and whether a project is managed or not. The following table shows an overview of the different project kinds:

Project Kind | Description | Installed to bake environment | Managed
-------------|----------|------------|---------
executable   | Executable binary | No by default | No by default
library      | Shared object | No by default | No by default
application  | Managed executable binary | Yes by default | Yes by default
package      | Managed shared object | Yes by default | Yes by default
tool         | Executable binary installed to OS `bin` path | No | Yes

### Project configuration
A bake project file is located in the root of a project, and must be called `project.json`. This file contains of an `id` describing the logical project name, a `type` describing the kind of project, and a `value` property which contains properties that customize how the project should be built.

This is a minimal example of a bake project file that builds an shared object. With this configuration, the project will be built with all values set to their defaults.

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
---------|------|------------
language | string | Language of the project. Is used to select a bake plugin.
version | string | Version of the project (use semantic versioning)
managed | bool | Is project managed or unmanaged
public | bool | If `true`, project is installed to `$BAKE_TARGET`
use | list(string) | List of dependencies using logical project ids. Dependencies must be located in either `$BAKE_HOME` or `$BAKE_TARGET`.
use_private | list(string) | Same as "use", but dependencies are private, which means that header files will not be exposed to dependees of this project.
dependee | object | Properties inside the `dependee` object will be passed on to dependees of this project. This allows a project to specify, for example, include paths that its dependee should use.
sources | list(string) | List of paths that contain source files. Default is `src`. The `$SOURCES` rule is substituted with this value.
includes | list(string) | List of paths that contain include files.
use_generated_api | bool | For managed projects only. For each project in `use`, add a project with id `$project/$language`, if it exists.

### The Bake Environment
Bake allows projects to be installed to a so called "bake environment". A bake environment is a harddrive location configured in the `$BAKE_HOME` and `$BAKE_TARGET` environment variables. Projects that are built to the bake environment are called "public" projects.

Public projects can be automatically discovered, looked up, loaded and linked with by using their logical name. Here is an example of two public projects, one `application` and one `package`, where the `application` depends on the `package`:

```json
{
    "id": "my_lib",
    "type": "package"
}
```

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["my_lib"]
    }
}
```

Note that neither project configuration specifies where they are built to, or where to find the `my_lib` project. This is automatically managed by the bake environment.

**Bake uses hard-coded paths to link to libraries when building to an environment**. This ensures that at runtime, the right libraries can be found without having to set `LD_LIBRARY_PATH`. This does however have one big limitation: unless binaries are built to a location with the same name, they cannot be shared between machines! To get around this limitation, there are two options:
- Install binaries to the same location on different machines (`/usr/local` or `/opt`)
- Use **standalone builds** (see below)

Note that using hard-coded paths enhances security, as it makes it harder (if not impossible) to spoof a dynamic library, as long as dynamic libraries are stored in a write-protected location on a disk drive.

The `$BAKE_TARGET` environment variable specifies the location where new projects are built to. The `$BAKE_HOME` environment variable specifies where the bake environment is located. Typically these variables are set to the same location. Dependencies are looked up in both `$BAKE_TARGET` as well as `$BAKE_HOME`.

By default, both `$BAKE_TARGET` and `$BAKE_HOME` are set to `~/.corto`, as bake tightly integrates (but does not depend on) corto package management. Inside this directory, a number of directories are commonly found. The following table describes their function:

Directory | Description
----------|-------------
bin | Contains executable binaries
lib | Contains shared libraries and project metadata
include | Contains include files of projects
java | Contains Java JARs
etc | Contains miscellaneous files used by projects at runtime
src | Contains downloaded sources (when using web installer)

The contents of these folders are split up by project, to prevent name-clashes between projects. For the two aforementioned example projects, miscellaneous files would be stored in the following directories:

```
$BAKE_TARGET/etc/corto/2.0/my_app
$BAKE_TARGET/etc/corto/2.0/my_lib
```

The `corto` prefix is there to ensure that the bake environment does not clash with files outside a bake environment, when `$BAKE_TARGET` is set to `/usr` or `/usr/local`. The `2.0` prefix is the "framework version" and allows multiple environments based on different corto versions to coexist.

The structure of the bake environment mimics that of a the `/usr` or `/usr/local` directory on Linux systems. This makes it easy to install a bake environment to a globally available location while following default system conventions. By default, the environment is located in the users home directory.

When `$BAKE_HOME` and `$BAKE_TARGET` are set to different locations, it can happen that a project appears twice in the environment. This typically happens when a package is both installed to a public location (`/usr/local`) and a local location (`~/.corto`). In that case, bake will link with the latest version of the project, which is determined by comparing timestamps of the binaries.

### Private projects
Private projects are projects that are not installed to a bake environment. Because of this, these projects cannot be located by other projects. Private projects may depend on public projects, but public projects cannot depend on private projects. Binaries of private objects are only stored in the `bin` folder in the project root.

Other than this, private and public projects are exactly the same. Therefore, when a private project depends on a public project, its binary cannot be distributed to other machines.

An example of where private projects are used extensively is in the corto test framework (https://cortoproject/test). Each test suite is compiled to a private shared object, which is then loaded by a tool that loads and runs the tests it discovers in the binary.

### Standalone Distributions
By default, binaries built to bake environments cannot be shared between machines unless they are built to the same harddrive location, because bake uses **hard-coded paths to link with libraries**. This requires that libraries are installed to locations like `/usr/local` or `/opt` which may require administrator privileges, which can be inconvenient.

Standalone mode builds binaries that link with libraries in the traditional way, by only specifying the library name during linking (example: `-lfoo`). This allows binaries to be shared across machines.

To enable standalone mode, specify the `--standalone` flag to bake, or add `"standalone": true` to the bake configuration file. In addition, a path can be specified where bake stores the standalone libraries, using `--standalone-path /some/path`, or by adding `"standalone_path": "/some/path"` to the bake configuration file. If left unspecified, the standalone path will be set to `$BAKE_TARGET/standalone`.

The following command shows how to enable standalone builds from the command line:

```demo
bake --standalone --standalone-path ~/my_distro
```

This is an example bake configuration that enables standalone builds:

```json
{
    "configuration":{
        "standalone-dbg":{
            "symbols": true,
            "debug": true,
            "optimizations": false,
            "standalone": true,
            "standalone_path": "~/my_distro"
        }
    }
}
```

After a standalone build, the standalone path contains all the binaries, include files, miscellaneous files and metadata for all the projects that were part of the build. The directory will contain the following subdirectories:

Directory | Description
----------|-------------
bin       | Executable binaries
lib       | Shared objects
include   | Include files
etc       | Miscellaneous files
meta      | Project metadata (project.json files and LICENSE files)

The whole standalone directory must be distributed to ensure that standalone applications can run just like they would when installed to a bake environment.

Applications do not need to modify their code to run in standalone mode. Code that uses package management APIs can still refer to packages using their logical names, and files stored in the `etc` folder will still be accessed as before. The include file structure is also the same as in a bake environment, which means include statements do not have to be modified.

The package manager runtime detects it is running in standalone when `$BAKE_HOME` has not been set. Attempting to run an application in standalone mode with `$BAKE_HOME` set to a value will fail.

To make sure that standalone applications can correctly locate their libraries, `LD_LIBRARY_PATH` has to be set to the `<standalone_path>/lib` directory. To ensure that standalone applications can correctly locate their miscellaneous files, their working directory has to be the standalone directory root.

### Environment Variables
Bake uses the following environment variables:

Variable | Description
---------|------------
BAKE_HOME | Location where bake looks for projects specified in `use`.
BAKE_TARGET | Location where bake installs projects. Usually the same as `$BAKE_HOME`.
BAKE_VERSION | Framework version that allows for storing packages of multiple major.minor framework versions in the same `$BAKE_HOME`. Usually set to the corto version.

### Configuring Bake
Bake can be configured with configuration files that specify the environment in which bake should run and the build configuration that should be used. Bake locates a bake configuration file by traveling upwards from the current working directory, and looking for a `.bake`, `bake` or `bake/config.json` file. If multiple files are found, they are applied in reverse order, so that the file that is "closest" to the project takes precedence.

A bake configuration file consists out of an `environment` and a `configuration` section. The `configuration` section contains parameters that are not specific to a project, but influence how code is built. The `environment` section contains a list of environment variables and their values which are loaded when bake is started.

The `bake env` command prints the bake environment to the command line in a format that can be used with the `export` bash command, so that the bake environment can be easily exported to the current shell, like so:

```
export `bake env`
```

Note that bake automatically adds `$BAKE_HOME/bin` to the `PATH` environment variable. This ensures that even when applications (tools) are not installed to a global location, such as `/usr/local/bin`, they can still be directly accessed from a shell when the bake environment is exported.

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

### Profiling a build
The logging framework used by bake has builtin profiling capabilities that make it easy to profile the different steps in a build process. To see how long each step in a build takes, use the following command:

```demo
bake --profile --show-delta --ok
```

Example output:

```demo
-------- baking in <current directory>
-------- begin build application 'my_app' in '.'
-------- build
-------- ├> ARTEFACT
-------- |  ├> objects
00.00212 |  |  ├> SOURCES
00.00014 |  |  ├> gen-sources-2
-------- |  |  |  [100%] my_app.c
00.03832 |  |  +
-------- |  |  bin/x64-linux-debug/my_app
00.06921 |  +
00.07038 +
-------- √ build application 'my_app' in '.'
-------- done!
```

From the above output we can see that compiling the file `my_app.c` took approximately 0.038 seconds, while the whole compilation took around 9.07 seconds.

### Command line options
The following examples show how to use the bake command:

```demo
bake [flags]
bake [flags] <directory>
bake [flags] <command>
bake [flags] <command> <directory>
```

The following commands are valid for bake:

Command | Description
--------|------------
init    | Initialize a new bake project in the current directory
build   | Build the project (default)
rebuild | Rebuild the project
clean   | Clean the project
env     | Return bake environment
foreach | Run command for every discovered project
install | Install project files to $BAKE_TARGET

The following flags can be used to specify parameters for a project on the
command line, when no `project.json` is available:

Flag | Description
-----|--------------
--id | Manually set project id
-k,--kind | Specify project kind
-i,--includes | Specify include paths, separated by comma
-s,--sources | Specify source paths, separated by comma
-l,--lang | Specify programming language (default = 'c')
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
--cfg | Specify configuration to load (default = 'debug')
--no-symbols | Disable symbols
--no-debug | Disable debug build
--optimize | Enable optimizations
--coverage | Enable code coverage
--standalone | Enable standalone build
--standalone-path | Specify output path for standalone build
--strict | Enable strict mode (strictest compiler flags)

Miscellaneous flags:

Flag | Description
-----|--------------
-p,--path | Specify search paths, separated by comma
--skip-preinstall | Skip install step
--skip-postintall | Skip copying binary to $BAKE_TARGET
--do | Specify action when using `bake foreach`

### Writing Plugins
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

### Corto integration
Bake by default installs a minimal set of corto packages (https://corto.io) that allow you to easily locate and list projects. The following commands are installed with bake:

List all public projects:

```demo
corto list
```

Get the location on disk for a project:

```demo
corto locate <project id>
```

Bake is included in all corto installations. The following command installs a development build of the corto framework which (amongst others) includes the corto documentation framework and corto test framework:

```demo
curl https://corto.io/install-dev-src | sh
```

## Authors
- Sander Mertens - Initial work

## Legal stuff
Bake is licensed under the GPL3.0 license.
