# bake
Bake is a buildtool (like `make`) and buildsystem in one that dramatically reduces the complexity of building code, in exchange for enforcing a rigid structure upon projects. This is great for two reasons:

1. All bake projects look the same which makes it easy to share and read code.
2. Bake just needs a simple, declarative JSON file to build a project

## Installation
[instructions on how to install bake]

Install bake using the following command:

```demo
curl https://corto.io/install-bake | sh
```

This installs the bake executable to your home directory. Additionally the installer creates a script in `/usr/local/bin` that lets you run bake from anywhere. To be able to write to `/usr/local/bin`, **the installer may ask for your password**.

The C language plugin is installed by default. For C-specific configuration parameters, see the README file in the plugin repository: https://github.com/cortoproject/driver-bake-c.

## Platform support
[platforms on which bake is supported]

Bake has been verified on the following platforms:

### Ubuntu
- Xenial
- Trusty
- Precise

### MacOS
- 10.13
- 10.12

## Getting started
[useful tips for new bake users]

### Bake Hello World
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

#### Clean a project
This command removes all the temporary files and build artefacts from the project directory. This does not remove files from the bake environment.

```demo
bake clean
```

#### Rebuild a project
This command first cleans, then builds the project again.

```demo
bake rebuild
```

#### Build a directory
Build all bake projects in the `my_app` directory.

```demo
bake my_app
```

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

```note
You can add/modify/remove bake configurations in the bake configuration file.
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

## Manual
[everything there is to know about bake]

### Introduction
The goal of bake is to bring a level of abstraction to building software that is comparable with `npm`. Tools like `make`, `cmake` and `premake` abstract away from writing your own compiler commands by hand, but still require users to create their own buildsystem, with proprietary mechanisms for specifying dependencies, build configurations etc.

This makes it difficult to share code between different people and organizations, and is arguably one of the reasons why ecosystems like `npm` are thriving, while ecosystems for native code are very fragmented.

Bake is therefore not just a buildtool like `make` that can automatically generate compiler commands. It is also a buildsystem that clearly specifies how projects are organized and configured. When a project relies on bake, a user does, for example, not need to worry about how to link with it, where to find its include files or whether binaries have been built with incompatible compiler flags.

A secondary goal is to create a zero-dependency buildtool that can be easily ported to other platforms. Whereas other buildtools exist, like `make`, `premake`, `rake` and `gradle`, they all rely on their respective ecosystems (`unix`, `lua`, `ruby`, `java`) which complicates writing platform-independent build configurations. Bake's only dependencies are a C standard library, and the corto platform abstraction API (https://github.com/cortoproject/platform).

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
Bake supports different project kinds which are configured in the `type` property of a `project.json` file. The project kind determines whether a project is a library or executable, whether a project is installed to a bake environment and whether a project is managed or not. The following table shows an overview of the different project kinds:

Project Kind | Description | Public | Managed
-------------|----------|------------|---------
executable   | Executable binary | No by default | No by default
library      | Shared object | No by default | No by default
application  | Managed executable binary | Yes by default | Yes by default
package      | Managed shared object | Yes by default | Yes by default
tool         | Executable binary installed to OS `bin` path | No | Yes

#### Public vs private projects
A public project is a project that is installed to the bake environment. In this environment, bake knows where to find include files, binaries and other project resources. This allows other projects to refer to these resources by the logical project name, and makes specifying dependencies between projects a lot easier.

Private projects are projects that are not installed to a bake environment. Because of this, these projects cannot be located by other projects. Private projects may depend on public projects, but public projects cannot depend on private projects. Binaries of private objects are only stored in the bin folder in the project root.

Other than this, private and public projects are exactly the same. Therefore, when a private project depends on a public project, its binary cannot be distributed to other machines.

An example of where private projects are used extensively is in the corto test framework (https://cortoproject/test). Each test suite is compiled to a private shared object, which is then loaded by a tool that loads and runs the tests it discovers in the binary.

#### Managed vs. unmanaged projects
A managed project is a project for which code is automatically generated by the corto code generator. The generated code automatically includes include files of dependencies. Additionally, code can be generated for a **corto model file**, when available in the project directory.

A model file should be called `model`, followed by an extension that indicates the format of the model. Currently JSON, XML and cortoscript are supported. When bake is installed standalone (without corto), only JSON support is installed.

Here is an example JSON model file for an application called `my_app`:

```json
[{
    "id": "my_app",
    "type": "application",
    "scope": [{
        "id": "Point",
        "type": "struct",
        "scope": [{
            "id": "x",
            "value": {
                "type": "int32"
            }
        }, {
            "id": "y",
            "value": {
                "type": "int32"
            }
        }]
    }]
}]
```

In this simple model, a type `Point` is described, with two members, `x` and `y`, both of type `int32`. When building the project, the corto code generator will be automatically invoked by bake, and a C/C++ type definition (amongst others) for this type will be generated. For the exact semantics and features of this modeling language, see the corto documentation. Managed projects also **automatically link with the corto runtime**.

Unmanaged projects do not invoke code generators, and are like regular C projects.

### Project Layout
Each bake project uses the same layout. This makes it very easy to build bake projects, as bake always knows where to find project configuration, include files, source files and so on. Bake will look for the following directories and files in a project:

Directory / File | Description
-----------------|------------
project.json | Contains build configuration for the project
model.* | Contains a model for managed projects (optional)
src | Contains the project source files
include | Contains the project header files
etc | Contains miscellaneous files (optional)
install | Contains files that are installed to environment (optional)
bin | Contains binary build artefacts (created by bake)
.bake_cache | Contains temporary build artefacts, such as object files and generated code

Bake will by default build any source file that is in the `src` directory. If the project is public, files in the `include`, `etc` and `install` folders will be soft-linked to the bake environment. By creating softlink, you can update files in these folders in your project directory without having to re-run bake to make the changes public.

### Project Configuration
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

Function | Description
---------|------------
locate | Locate various project paths in the bake environment

The next sections are detailed description of the supported functions:

#### locate
The locate function allows a project configuration to use any of the project paths in the bake environment. The values returned by this function are equal to those returned by the `corto locate` command and `corto_locate` function.

Parameter | Description
----------|-------------
package | The package directory (lib)
include | The package include directory
etc | The package etc directory
lib | The package library (empty if an executable)
app | The package executable (empty if a library)
bin | The package binary
env | The package environment

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
$BAKE_TARGET/2.0/platform-config/etc/my_app/index.html
$BAKE_TARGET/2.0/platform-config/etc/my_app/style.html
$BAKE_TARGET/2.0/platform-config/etc/image.jpg
$BAKE_TARGET/2.0/platform-config/etc/manual.pdf
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

To see the exact matching of the platform string, see the implementation of `corto_os_match` in the https://github.com/cortoproject/platform repository.

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
        "dependee": {
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
        "language": "none",
        "dependee": {
            "include": ["${locate include}"],
        }
    }
}
```

The `${locate include}` part of the include path will be substituted by the project-specific include folder when the `project.json` is parsed. This function is equivalent to `$BAKE_TARGET/include/corto/$BAKE_VERSION/helloworld`.

#### Link external files from global environment
When a project needs to link with an external binary, one option is to install ot to a global location, like `/usr/local/lib`. The bake equivalent is to install it to the `lib` root directory. That way, the library will be installed to `$BAKE_TARGET/lib`. Thus when `$BAKE_TARGET` is set to `/usr/local`, the library will be installed to `/usr/local/lib`.

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
        "language": "none",
        "dependee": {
            "lib": ["helloworld"]
        }
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
        "language": "none",
        "dependee": {
            "link": ["${locate lib}/helloworld"]
        }
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
        "language": "none",
        "dependee": {
            "include": ["${locate include}"],
            "link": ["${locate lib}/helloworld"]
        }
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

The `$BAKE_TARGET` environment variable specifies the location where new projects are built to. The `$BAKE_HOME` environment variable specifies where the bake environment is located. Typically these variables are set to the same location. Dependencies are looked up in both `$BAKE_TARGET` as well as `$BAKE_HOME`.

By default, both `$BAKE_TARGET` and `$BAKE_HOME` are set to `~/corto`, as bake tightly integrates with (but does not depend on) corto package management. Inside this directory, a number of directories are commonly found. The following table describes their function:

Directory | Description
----------|-------------
bin | Contains executable binaries
lib | Contains shared libraries
include | Contains include files of projects
java | Contains Java JARs
etc | Contains miscellaneous files used by projects at runtime
meta | Contains project metadata
src | Contains downloaded sources (when using web installer)

The contents of these folders are split up by project, to prevent name-clashes between projects. For the two aforementioned example projects, miscellaneous files would be stored in the following directories:

```
$BAKE_TARGET/2.0/platform-config/etc/my_app
$BAKE_TARGET/2.0/platform-config/etc/my_lib
```
Platform and config here are replaced respectively with the platform that bake is running on, and the current configuration (for example `debug` or `release`).

When `$BAKE_HOME` and `$BAKE_TARGET` are set to different locations, it can happen that a project appears twice in the environment. This typically happens when a package is both installed to a public location (`/usr/local`) and a local location (`~/corto`). In that case, bake will link with the latest version of the project, which is determined by comparing timestamps of the binaries.

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
            "BAKE_HOME":"~/.corto",
            "BAKE_TARGET":"~/.corto",
            "BAKE_VERSION":"2.0",
        }
    }
}
```

With the `--cfg` and `--env` flags the respective configuration or environment can be selected.

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
-i,--includes | Specify paths that contain include files, separated by comma. Include files will be copied to the project include directory in the bake environment. This is not the same as specifiying an include path!
-s,--sources | Specify paths that contain source files, separated by comma. Source files fill be built as part of the project.
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
--mono | Disable colors in output
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

From the above output we can see that compiling the file `my_app.c` took approximately 0.038 seconds, while the whole compilation took around 0.070 seconds.

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

The corto runtime mounts the bake environment in the corto object store, which gives applications API access to installed packages and their meta information. The following code is an example that lists all packages in the bake environment:

```c
corto_iter it;
corto_select("//").type("package").iter(&it);
while (corto_iter_hasNext(&it)) {
    corto_result *r = corto_iter_next(&it);
    printf("package '%s/%s' found\n", r->parent, r->id);
}
```

## Authors
[who built bake]

- Sander Mertens - Initial work

## Legal stuff
[bake licensing]

Bake is licensed under the GPL3.0 license.
