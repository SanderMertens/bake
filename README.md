# bake
Bake is a build tool, build system, package manager and environment manager in one. The goal of bake is to be able to download, build and run applications that rely on many packages, with a single command. Other than package managers like `brew` and `conan`, bake is also a build system (like `make` and `cmake`), so building projects is seamlessly integrated with using remote projects. For now, bake focusses on building C/C++ projects.

Some of its features are:

- Single command to create a new project
- Minimal JSON file for project configuration
- Recursively discover projects in a directory & build them in correct order, based on dependencies
- Automatically clone bake projects from git, including their dependencies
- Automatically generate include statements for header files of dependencies
- Manage and export sets of environment variables with a single command
- Manage different build configurations (like 'debug' and 'release')
- Store packages in $HOME (or custom location), so no root privileges required
- Hierarchical package identifiers
- Use logical package names ('hello.world') instead of filenames and paths
- Include dependency headers by logical package name (`#include <hello.world>`)
- Publish new package versions with a single command
- Include miscellaneous files, like HTML and JS to deploy with your project
- Modular architecture, where projects can specify which build drivers they require
- Reuse complex build configurations by storing them in special configuration packages
- Apply filters to configurations like operating system, language and architecture
- Simple redistribution of self-contained bake environments
- Programmable C API for interacting with package management

Bake development has focussed so far on improving user experience on commodity operating systems, like macOS and Linux (Windows is coming soon). Future releases will add more features, like cross-compilation, code coverage, static code analysis and CI integration.

Bake only uses git, and does _not_ depend on its own server infrastructure for package management. **We do not collect any information when you clone, build or publish projects**.

## Installation
[instructions on how to install bake]

Install bake using the following commands:

On macOS:
```demo
git clone https://github.com/SanderMertens/bake
make -C bake/build-darwin
bake/bake setup
```

On Linux:
```demo
git clone https://github.com/SanderMertens/bake
make -C bake/build-linux
bake/bake setup
```

After you've installed bake once, you can upgrade to the latest version with:

```demo
bake upgrade
```

We will add support for package managers like brew in the future.

## Platform support
[platforms on which bake is supported]

Bake is currently only supported on Linux and macOS.

## Getting started
[useful tips for new bake users]

The following commands are useful for getting started with bake. Also, check out the `bake --help` command, which lists all the options and commands available in the bake tool.

### Basic configuration
This example shows the simplest possible bake configuration file that builds an executable called `my_app`. Bake project configuration is stored in a file called `project.json`.

```json
{
    "id": "my_app",
    "type": "application"
}
```

### Configuration with dependency
This example shows a simple configuration with a dependency on the `foo.bar` package.

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["foo.bar"]
    }
}
```

### Configuration with C driver
Bake projects can include configuration for drivers, which will be loaded during the build. This is an example that specifies settings specific for the driver responsible for building C code:

```json
{
    "id": "my_app",
    "type": "application",
    "lang.c": {
        "lib": ["pthread"]
    }
}
```

### Create new project
To create a new bake project called `my_app`, run the following commands:

```demo
bake init my_app
```

This creates a folder called `my_app` with a new `project.json` containing a basic configuration, a `src` and `include` folder for your source and include files.

### Clean a project
This command removes all temporary files and binaries from the project:

```demo
bake clean
```

### Rebuild a project
This command first cleans, then builds the project:

```demo
bake rebuild
```

### Build a directory
This command builds the project (or projects) in the `my_project` directory:

```demo
bake my_project
```

### Build a remote project
This command builds a project and its dependencies directly from a git repository:

```demo
bake clone https://github.com/SanderMertens/example
```

### Export an environment variable to the bake environment
Bake can manage environment variables that must be set during the build. To export an environment variable to the bake environment, do:

```demo
bake export VAR=value
```
Alternatively, if you want to add a path to an environment variable like `PATH` or `LD_LIBRARY_PATH`, do:

```demo
bake export PATH+=/my/path
```

These variables are stored in a configuration file called `bake.json` in the root of the bake environment, which by default is `$HOME/bake`.

### View and export bake environment variables
To see the environment variables and their values in the default environment, do:

```demo
bake env
```

To get the environment variables and values of a custom environment, do:

```demo
bake env --env custom
```

To export the environment variables to the shell, do:

```demo
export `bake env`
```

### Do a release build
By default, bake builds projects in debug mode, which disables optimizations and enables symbols. If you want to build projects in release build, specify the `release` configuration, like this:

```demo
bake --cfg release
```

```note
You can add/modify/remove bake configurations in the bake configuration file.
```

### Debug bake
Can't figure out why bake is doing something (or not doing something)? By enabling tracing, you can get much more information out of bake. Try this command:

```demo
bake --trace
```

Or if that is not enough, try this command:

```demo
bake -v debug
```

## Manual
[everything there is to know about bake]

### Introduction
The goal of bake is to bring a level of abstraction to building software that is comparable with `npm`. Tools like `make`, `cmake` and `premake` abstract away from writing your own compiler commands by hand, but still require users to create their own build system, with proprietary mechanisms for specifying dependencies, build configurations etc.

This makes it difficult to share code between different people and organizations, and is arguably one of the reasons why ecosystems like `npm` are thriving, while ecosystems for native code are fragmented.

Bake is therefore not just a build tool like `make` that can automatically generate compiler commands. It is also a build system that specifies how projects are organized and configured. When a project relies on bake, a user does, for example, not need to worry about how to link with it, where to find its include files or whether binaries have been built with incompatible compiler flags.

A secondary goal is to create a zero-dependency build tool that can be easily ported to other platforms. Whereas other build tools exist, like `make`, `premake`, `rake` and `gradle`, they all rely on their respective ecosystems (`unix`, `lua`, `ruby`, `java`) which complicates writing platform-independent build configurations. Bake's only dependency is the C runtime.

### Project Kinds
Bake supports different project kinds which are configured in the `type` property of a `project.json` file. The project kind determines whether a project is a library or executable, whether a project is installed to a bake environment and whether a project is managed or not. The following table shows an overview of the different project kinds:

Project Kind | Description
-------------|----------
application  | Executable
package      | Shared object

#### Public vs private projects
A public project is a project that is installed to the bake environment. In this environment, bake knows where to find include files, binaries and other project resources. This allows other projects to refer to these resources by the logical project name, and makes specifying dependencies between projects a lot easier.

Private projects are projects that are not installed to a bake environment. Because of this, these projects cannot be located by other projects. Private projects may depend on public projects, but public projects cannot depend on private projects. Binaries of private objects are only stored in the bin folder in the project root.

### Project Layout
Each bake project uses the same layout. This makes it very easy to build bake projects, as bake always knows where to find project configuration, include files, source files and so on. Bake will look for the following directories and files in a project:

Directory / File | Description
-----------------|------------
project.json | Contains build configuration for the project
src | Contains the project source files
include | Contains the project header files
etc | Contains miscellaneous files (optional)
install | Contains files that are installed to environment (optional)
bin | Contains binary build artefacts (created by bake)
.bake_cache | Contains temporary build artefacts, such as object files and generated code

Bake will by default build any source file that is in the `src` directory. If the project is public, files in the `include`, `etc` and `install` folders will be soft-linked to the bake environment.

### Project Configuration
A bake project file is located in the root of a project, and must be called `project.json`. This file contains of an `id` describing the logical project name, a `type` describing the kind of project, and a `value` property which contains properties that customize how the project should be built.

This is a minimal example of a bake project file that builds an shared object. With this configuration, the project will be built with all values set to their defaults.

```json
{
    "id": "my_library",
    "type": "package"
}
```

This example shows how to specify dependencies and specify additional flags:

```json
{
    "id": "my_application",
    "type": "application",
    "value": {
        "use": ["my_library"]
    },
    "lang.c": {
        "cflags": ["-DHELLO_WORLD"]
    }
}
```

In this example, if `my_library` is a project that is discovered by bake, it will be built *before* `my_application`.

The following properties are available from the bake configuration and are specified inside the `value` property:

Property | Type | Description
---------|------|------------
language | string | Language of the project. Is used to load a bake language driver. May be `null`.
version | string | Version of the project (use semantic versioning)
public | bool | If `true`, project is installed to `$BAKE_TARGET`
use | list(string) | List of dependencies using logical project ids. Dependencies must be located in either `$BAKE_HOME` or `$BAKE_TARGET`.
use_private | list(string) | Same as "use", but dependencies are private, which means that header files will not be exposed to dependees of this project.
sources | list(string) | List of paths that contain source files. Default is `src`. The `$SOURCES` rule is substituted with this value.
includes | list(string) | List of paths that contain include files.
keep_binary | bool | Do not clean binary files when doing bake clean. When a binary for the target platform is present, bake will skip the project. To force a rebuild, a user has to explicitly use the `bake rebuild` command.

The `cflags` atrribute is specified inside the `lang.c` property. This is because `cflags` is a property specific to the C driver. For documentation on which properties are valid for which drivers, see the driver documentation.

### Template Functions
Bake property values may contain calls to template functions, which in many cases allows project configuration files to be more generic or less complex. Template functions take the following form:

```
${function_name parameter}
```

They are used like this:

```json
{
    "id": "my_project",
    "type": "package",
    "value": {
        "include": ["${locate include}"]
    }
}
```

The following functions are currently supported:

Function | Type | Description
---------|------------
locate | string | Locate various project paths in the bake environment
target | bool | Match a target platform

The next sections are detailed description of the supported functions:

#### locate
The locate function allows a project configuration to use any of the project paths in the bake environment. This functionality can also be used programmatically, through the `ut_locate` function in the `bake.util` package.

Parameter | Description
----------|-------------
package | The package directory (lib)
include | The package include directory
etc | The package etc directory
lib | The package library (empty if an executable)
app | The package executable (empty if a library)
bin | The package binary
env | The package environment

#### target
The target function can be used to build configurations that use different settings for different platforms. The following example demonstrates how it can be used:

```json
{
    "${target linux}": {
        "include": ["includes/linux"]
    }
}
```

The function can match both operating system and architecture. The following expressions are all valid:

- x86-linux
- darwin
- x86_64
- x86_64-darwin
- i386

For a full description of the expressions that are supported, see the documentation of `ut_os_match`.

The target function may be nested:

```json
{
    "${target linux}": {
        "include": ["includes/linux"],
        "${target x86_64}": {
            "lib": ["mylib64"]
        },
        "${target x86}": {
            "lib": ["mylib32"]
        }
    }
}
```

### Installing Miscellaneous Files
Files in the `install` and `etc` directories are automatically copied to the project-specific locations in the bake environment, so they can be accessed from anywhere (see below). The `install` folder installs files directly to the bake environment, whereas files in `etc` install to the project-specific location in the bake environment. For example, the following files:

```
my_app
 |
 |-- etc
 |    | index.html
 |    + style.css
 |
 +-- install/etc
      | image.jpg
      + manual.pdf
```

would be installed to the following locations:

```
$BAKE_TARGET/platform-config/etc/my_app/index.html
$BAKE_TARGET/platform-config/etc/my_app/style.html
$BAKE_TARGET/platform-config/etc/image.jpg
$BAKE_TARGET/platform-config/etc/manual.pdf
```

Bake allows projects to differentiate between different platforms when installing files from the `etc` and `install` directories. This can be useful when for example distributing binaries for different architectures and operating systems. By default, all files from these directories installed. However, bake will look for subdirectories that match the platform string. Files in those directories will only be installed to that platform. For example, consider the following tree:

```
my_app
 |
 +-- etc
      |-- Linux/linux_manual.html
      |
      |-- Darwin/darwin_manual.html
      |
      |-- Linux-i686/libmy_binary.so
      |
      |-- Linux-x86_64/libmy_binary.so
      |
      |-- Linux-armv7l/libmy_binary.so
      |
      +-- Darwin-x86_64/libmy_binary.so
```

Here, only the `libmy_binary.so` that is in the directory that matches the platform string will be installed.

The platform string is case independent. It allows for a number of different notations. For example, both `x86-linux` and `linux-x86` are allowed. In addition, projects can also just specify the operating system name, in which case the file will be installed to all architectures, as long as the operating system matches the directory name.

To see the exact matching of the platform string, see the implementation of `ut_os_match` in `bake.util`.

### Integrating Non-Bake Projects
It is not uncommon that a project needs to include or link with a project that itself was not built with bake. Often such projects require that you specify custom include paths, library paths, and link with specific libraries. When you have many projects that depend on such an external project, it can become tedious having to repeat these properties in every `project.json`.

#### Wrapping external projects
Bake allows you to create a project that "wraps around" the external project, in which you describe this build configuration once. Once done, projects can simply add this project as a dependency, and the properties will be automatically added.

Consider a project that requires dependees to add `/usr/local/include/foobar` to their include path, that need to link with `libfoobar.so`, which is a library located in `/usr/local/lib/foobar`. For such a project, this is what the `project.json` could look like:

```json
{
    "id": "foobar",
    "type": "package",
    "value": {
        "language": "none",
    },
    "dependee": {
        "lang.c": {
            "include": ["/usr/local/include/foobar"],
            "libpath": ["/usr/local/lib/foobar"],
            "lib": ["foobar"]
        }
    }
}
```

Lets go over each property. The first two specify that the project is a bake package with the id `foobar`. Because bake packages are public by default, we do not have to explicitly add `"public": true` to the `project.json` file to ensure that other packages can find this dependency.

The next property is `"language": "none"`. This ensures that when ran, bake does not try to build anything for the package. It also ensures that when specifying this package as a dependency, bake will not try to link with any binaries.

The `dependee` property is where the properties are specified for projects that depend on `foobar`. For every dependee project, bake will add the include, libpath and lib properties to those of the dependee configuration. Therefore, a project that depends on `foobar`, simply can do:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["foobar"]
    }
}
```

... and bake will take care of the rest.

#### Include external files from bake environment
When include files or libraries are not installed to a common system location, you can use bake to make these files available to dependee projects as well. Suppose we have a project called `helloworld`, which is shipped as a library `libhelloworld.so`, and two header files called `helloworld.h` and `helloworld_types.h`. We could include these files in a bake project, like this:

```
helloworld
 |
 +-- include
      |-- helloworld.h
      +-- helloworld_types.h
```

That way, when running bake, they are installed to the bake environment and are available to other projects. However, there is a potential problem with this approach. The `helloworld.h` file might for example depend on `helloworld_types.h`, simply by doing:

```c
#include <helloworld_types.h>
```

Because bake does not automatically add project-specific include paths to the include path when compiling (to prevent name-clashes), that include file will not be found by the compiler. Therefore, an additional property is required that includes the correct path from the bake environment. Instead of hard-coding that path, bake provides a convenient way to do this:

```json
{
    "id": "helloworld",
    "type": "package",
    "value": {
        "language": "none"
    },
    "dependee": {
        "include": ["${locate include}"],
    }
}
```

The `${locate include}` part of the include path will be substituted by the project-specific include folder when the `project.json` is parsed.

#### Link external files from global environment
When a project needs to link with an external binary, one option is to install it to a global location, like `/usr/local/lib`. The bake equivalent is to install it to the `lib` root directory. That way, the library will be installed to `$BAKE_TARGET/lib`. Thus when `$BAKE_TARGET` is set to `/usr/local`, the library will be installed to `/usr/local/lib`.

To install the library to this location, it needs to be added to the project folder. Add the library to this location:

```
helloworld
 |
 +-- install/lib/libhelloworld.so
```

The project configuration now needs to be configured so that dependee projects link with the library:

```json
{
    "id": "helloworld",
    "type": "package",
    "value": {
        "language": "none"
    },
    "dependee": {
        "lib": ["helloworld"]
    }
}
```

#### Link external files from bake environment
In some cases it may be desirable to link with a library without copying it to a public location, like `/usr/local/lib`. In that case, the library can also be copied to the bake environment, in the same way we did for the the include file. First, the library needs to be installed to a project specific location. This can be accomplished by storing it in the `lib` directory in the project:

```
helloworld
 |
 +-- lib/libhelloworld.so
```

Now the `dependee` section in the `project.json` file needs to be adjusted so that projects depending on `helloworld` will link with the correct library. To avoid having to rely on `LD_LIBRARY_PATH`, or having to specify a full path in the project configuration, we can use the `link` property in combination with a bake template function. The full configuration now looks like this:

```json
{
    "id": "helloworld",
    "type": "package",
    "value": {
        "language": "none"
    },
    "dependee": {
        "link": ["${locate lib}/helloworld"]
    }
}
```

Bake will automatically expand the expression in the `link` path so that it contains the `lib` and `.so` prefixes. The `link` property will cause bake to link with the library using a hard-coded path, just like other project dependencies.

```warning
Some prebuilt libraries cannot be linked with using a hard-coded path. Typically libraries that have been compiled with "--soname" may cause problems, as the hardcoded path will be overwritten at link-time with the name provided to "--soname", which will cause the runtime linker to fail.
```

#### Deploying to multiple operating systems
When deploying binaries, a project likely needs to include versions for multiple operating systems. This can be done by storing the binaries in a directory that matches the target operating system. The following tree shows the `helloworld` project with two binaries, for Linux and MacOS:

```
helloworld
 |
 +-- lib
      |-- Linux-i686/libhelloworld.so
      |-- Linux-x86_64/libhelloworld.so
      +-- Darwin-x86_64/libhelloworld.dylib
```

Bake will automatically install and link with the binary that corresponds with the target platform. Note that bake also automatically tries to find libraries that end in `dylib` on MacOS.

#### Putting it all together
The following tree and project file show a non-bake project where the include files and binary file are installed to the bake environment, and the project supports multiple operating systems.

Files:
```
helloworld
 |-- project.json
 |
 |-- include
 |    |-- helloworld.h
 |    +-- helloworld_types.h
 |    
 +-- lib
      |-- Linux-i686/libhelloworld.so
      |-- Linux-x86_64/libhelloworld.so
      +-- Darwin-x86_64/libhelloworld.dylib
```

project.json:

```json
{
    "id": "helloworld",
    "type": "package",
    "value": {
        "language": "none"
    },
    "dependee": {
        "include": ["${locate include}"],
        "link": ["${locate lib}/helloworld"]
    }
}
```

### Private dependencies
When projects depend on other projects that require additional library paths or include paths, it may not be desirable to require having these properties propagate to dependees. For example, `bar` depends on `foo`, and `foo` requires adding an `include` path to the build configuration. Now, `helloworld` depends on `bar`, but it does not need to know about `foo`.

To prevent the `foo` build settings from propagating to `helloworld`, `bar` will need to configure `foo` as a "private dependency". The following configuration shows how to do this:

```json
{
    "id": "bar",
    "type": "package",
    "value": {
        "use_private": ["foo"]
    }
}
```

This way, `foo` is still added as a dependency to `bar`, but `helloworld` will not be exposed to `foo`, nor inherit any of its build settings.

### The Bake Environment
Bake allows projects to be installed to a so called "bake environment". A bake environment is a location configured in the `$BAKE_HOME` and `$BAKE_TARGET` environment variables. Projects that are built to the bake environment are called "public" projects.

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

The `$BAKE_TARGET` environment variable specifies the location where new projects are built to. The `$BAKE_HOME` environment variable specifies where the bake environment is located. Typically these variables are set to the same location. Dependencies are looked up in both `$BAKE_TARGET` as well as `$BAKE_HOME`.

By default, both `$BAKE_TARGET` and `$BAKE_HOME` are set to `~/bake`. Inside this directory, a number of directories are commonly found. The following table describes their function:

Directory | Description
----------|-------------
bin | Contains executable binaries
lib | Contains shared libraries
include | Contains include files of projects
java | Contains Java JARs
etc | Contains miscellaneous files used by projects at runtime
meta | Contains project metadata
src | Contains downloaded sources (when using bake clone)

The contents of `$BAKE_TARGET` are split up by project, to prevent name-clashes between projects. For the two aforementioned example projects, miscellaneous files would be stored in the following directories:

```
$BAKE_TARGET/platform-config/etc/my_app
$BAKE_TARGET/platform-config/etc/my_lib
```
Platform and config here are replaced respectively with the platform that bake is running on, and the current configuration (for example `debug` or `release`).

When `$BAKE_HOME` and `$BAKE_TARGET` are set to different locations, it can happen that a project appears twice in the environment. This typically happens when a package is both installed to a public location (`/usr/local`) and a local location (`~/bake`). In that case, bake will link with the latest version of the project, which is determined by comparing timestamps of the binaries.

### Environment Variables
Bake uses the following environment variables:

Variable | Description
---------|------------
BAKE_HOME | Location where bake looks for projects specified in `use`.
BAKE_TARGET | Location where bake installs projects. Usually the same as `$BAKE_HOME`.
BAKE_CONFIG | The current build configuration used by bake (`debug` by default)
BAKE_ENVIRONMENT | The current build environment used by bake (`default` by default)

### Configuring Bake
Bake can be optionally configured with configuration files that specify the environment in which bake should run and the build configuration that should be used. Bake locates a bake configuration file by traveling upwards from the current working directory, and looking for a `bake.json` file. If multiple files are found, they are applied in reverse order, so that the file that is "closest" to the project takes precedence.

A bake configuration file consists out of an `environment` and a `configuration` section. The `configuration` section contains parameters that are not specific to a project, but influence how code is built. The `environment` section contains a list of environment variables and their values which are loaded when bake is started.

The `bake env` command prints the bake environment to the command line in a format that can be used with the `export` bash command, so that the bake environment can be easily exported to the current shell, like so:

```
export `bake env`
```

```note
Bake automatically adds `$BAKE_HOME/bin` to the `PATH` environment variable. This ensures that even when applications (tools) are not installed to a global location, such as `/usr/local/bin`, they can still be directly accessed from a shell when the bake environment is exported.
```

The following table is a list of the configuration parameters:

Parameter | Type | Description
----------|------|------------
symbols | bool | Enable or disable symbols in binaries
debug | bool | Enable or disable debugging (defines NDEBUG if `false`)
optimizations | bool | Enable or disable optimizations
coverage | bool | Enable or disable coverage
strict | bool | Enable or disable strict building

```note
It is up to plugins to provide implementations for the above parameters. Not all parameters may be implemented. Refer to the plugin documentation for specifics.
```

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
            "PATH": ["/my/path"],
            "FOO": "Some value"
        }
    }
}
```

Note that environment variables configured as a JSON array (as shown with the `PATH` variable), are appended to their current value. Elements in the array are separated by a `:` or `;`, depending on the platform.

With the `--cfg` and `--env` flags the respective configuration or environment can be selected.

### Command line usage
The following is the output of `bake --help`

Usage: bake [options] <command> <path>

Options:
  -h,--help                    Display this usage information
  -v,--version                 Display version information

  --cfg <configuration>        Specify configuration id
  --env <environment>          Specify environment id
  --build-to-home              Build to BAKE_HOME instead of BAKE_TARGET

  --id <project id>            Manually specify a project id
  --type <project type>        Manually specify a project type (default = "package")
  --language <language>        Manually specify a language for project (default = "c")
  --artefact <binary>          Manually specify a binary file for project
  --includes <include path>    Manually specify an include path for project

  --trace                      Set verbosity to TRACE
  -v,--verbosity <kind>        Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)

Commands:
  init [path]                  Initialize new bake project
  build [path]                 Build a project
  rebuild [path]               Clean and build a project
  clean [path]                 Clean a project
  publish <patch|minor|major>  Publish new project version
  install [path]               Install project to bake environment
  uninstall [project id]       Remove project from bake environment
  clone <git url>              Clone and build git repository and dependencies
  update [project id]          Update an installed package or application

  env                          Echo bake environment
  upgrade                      Upgrade to new bake version
  export <NAME>=|+=<VALUE>     Add variable to bake environment

### Writing Plugins
Bake has a plugin architecture, where a plugin describes how code should be built for a particular language. Bake plugins are essentially parameterized makefiles, with the only difference that they are written in C, and that they use the bake build engine. Plugins allow you to define how projects should be built once, and then reuse it for every project. Plugins can be created for any language.

The bake build engine has a design that is similar to other build engines in that it uses rules that depend on other rules. Rules have rule-actions, which get executed when a rule is outdated. Whether a rule is outdated or not is determined by comparing timestamps of the rule dependencies with the timestamps of the rule output.

Rules are written in their respective language plugins in C. A simple set of rules that builds a binary from a set of source files would look like this:

```c
driver->pattern("SOURCES", "//*.c|*.cpp|*.cxx");
driver->rule("objects", "$SOURCES", driver->target_map(src_to_obj), compile_src);
driver->rule("ARTEFACT", "$objects", driver->target_pattern(NULL), link_binary);
```

Patterns create a label for a pattern (using the `ut_expr` syntax). Rules are patterns that have dependencies and actions. The syntax for a rule is:

```c
driver->rule(<id>, <dependencies>, <function to map target to output>, <action>);
```

Each plugin must have a `bakemain` entrypoint. This function is called when the
plugin is loaded, and must specify the rules and patterns.

## Authors
[who built bake]

- Sander Mertens - Initial work

## Legal stuff
[bake licensing]

Bake is licensed under the GPL3.0 license.
