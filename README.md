# bake
The Dutch IRS has a catchy slogan, which goes like this: "Leuker kunnen we 't niet maken, wel makkelijker". Roughly translated, this means: "We can't make it more fun, but we can make it easier". Bake adopts a similar philosophy. Building code (especially C/C++) will never be fun, so lets try to make it as easy and painless as possible.

To that end, bake is a build tool, build system, package manager and environment manager in one. Bake's goal is to automate the process of building code as much as possible, especially when having lots of projects that depend on each other. For now, bake focuses on building C/C++ code.

Bake's main features are:
- discover all projects in current directory & build them in right order
- automatically include header files from dependencies
- use logical (hierarchical) identifiers to specify dependencies on any project built on the machine
- programmable C API for interacting with package management
- manage and automatically export environment variables used for builds

Bake depends on git for its package management features, and does _not_ have a server infrastructure for hosting a package repository. **Bake does not collect any information when you clone, build or publish projects**.

## Contents
* [Installation](#installation)
* [Getting Started](#getting-started)
* [FAQ](#faq)
* [Manual](#manual)
* [Authors](#authors)
* [Legal stuff](#legal-stuff)

## Installation
[instructions on how to install bake]

Install bake using the following commands:

On MacOS:
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

We will add support for package managers like brew in the future. Bake is currently only supported on Linux and macOS.

## Getting started
[useful tips for new bake users]

The following commands are useful for getting started with bake. Also, check out the `bake --help` command, which lists all the options and commands available in the bake tool.

### Create new project
To create a new bake application project called `my_app`, run the following commands:

```demo
bake init my_app
```

### Basic configuration with dependency and configuration for C driver
This example shows a simple configuration with a dependency on the `foo.bar` package and links with `pthread`.

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["foo.bar"]
    },
    "lang.c": {
        "lib": ["pthread"]
    }
}
```

### Build, rebuild and clean a project

```demo
bake
bake rebuild
bake clean
```

Specify a build configuration:

```demo
bake --cfg release
```

### Clone & build a project from git
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

To export the bake environment to a terminal, do:
```
export `bake env`
```

## FAQ

### Bake is built under the GPL3.0 license. Does this mean I cannot use it for commercial projects?
No. As long as you do not distribute bake (either as source or binary) as part of your product, you can use bake for building your projects. This is no different than when you would use make for your projects, which is also GPL licensed.

### I want my customers to use bake. Does the license allow for this?
Yes. As long as your customers use the open source version of bake, and you do not distribute bake binaries or source files with your product, your customers can use bake.

### I noticed a premake file in the bake repository. Does bake need premake to be installed?
No. Bake uses premake to generate its makefiles (we would've used bake to build bake- but chicken & egg etc). The generated makefiles are included in the bake repository, so you won't need premake to use bake.

### Why yet another build tool?
Bake originally was a build tool developed for a framework (https://corto.io). It ended up simplifying building code a lot, and we decided to turn it into a separate project. So why did bake simplify building code that much?

Most build tools focus on the actual compilation process itself, and require project configurations to explicitly specify how source files get compiled to binaries. Since these rules are very similar for each C/C++ project, bake stores them into reusable drivers. As a result, bake project configurations can remain very simple and declarative.

In addition, bake is modular so that even when your build needs to do more than just compile C/C++ files, you can write a new driver that, for example, generates code. You can then simply reference that driver from your project configuration.

Having said that, bake is not perfect and there is still lots of work to do. It does not run on as many platforms as cmake does, and is not as flexible as make. Maybe someday it will be, maybe not. Bake's development is driven by its users, so if you are using it and you're missing a feature, let us know!

### Why yet another package manager?
Bake is different from package managers like conan, brew or apt-get. It is intended as a tool for developers to easily import and use code from other developers. Bake for example does not have an online package repository, does not distribute binaries and by default stores packages in the user $HOME directory. Its only dependency is git, so not data is collected by bake when you download or publish packages.

### How does bake compare to make?
GNU make is a tool for generating compiler commands. It has a custom language for specifying build rules, and allows for a lot of complexity and flexibility in the project-specific makefiles. In a makefile, you would ordinarily find all information that is required to build your project, from the names and locations of source files, to the compiler flags, to where your binary will be stored.

Bake also generates compiler commands, but instead of requiring a user to create build rules from scratch, bake uses "drivers" (configurable plugins) to do much of the heavy lifting. Driver implementations look very similar to makefiles, in that they also specify build rules with in & outputs. This moves the most complex part of a build to a reusable module, while keeping configuration simple.

Bake further differentiates itself when it comes to working with multiple projects at once. With make, users often rely on "super" makefiles, that specify the locations of projects and the order in which they must be built. In contrast, bake automatically discovers the projects to build, and computes the right build order based on the project dependencies. If a dependency is not discovered, bake will locate it in the bake environment (or throw an error).

Finally, bake has many features beyond generating compiler commands that address problems commonly found during building, like managing environment variables, git integration and package versioning.

### How does bake compare to CMake?
CMake and bake have similar goals in that both tools simplify the build process, but they do so in very different ways. To highlight the differences, lets take an example CMake project configuration, and then compare it to bake:

```cmake
cmake_minimum_required (VERSION 2.6)

include_directories ("bar")
add_subdirectory (bar)
set (EXTRA_LIBS ${EXTRA_LIBS} bar)

project (foo)
add_executable(foo foo.c)

target_link_libraries (foo ${EXTRA_LIBS})
```

This configuration builds an executable called `Foo` that depends on a library called `Bar` (configuration for `Bar` not shown). The `Bar` project is a subdirectory of the `Foo` project, and it is added to the configuration so CMake is able to find the `Bar` project. The equivalent bake project configuration looks like this:

```json
{
    "id": "foo",
    "type": "application",
    "value": {
        "use": ["bar"]
    }
}
```
A few things jump out. First of all, the bake configuration does not specify where to find `bar`. Bake will either automatically discover `bar` from where it is invoked, or find bar in the bake environment in `$HOME/bake` if it has been built before. 

Secondly, the bake configuration does not explicitly specify the source files of the project. Bake looks for source files in well-defined locations, which is the same for each project (source files in `src`, include files in `include`).

A more subtle difference is how in CMake, the configuration adds the `bar` subdirectory to the list of include paths. In bake, projects can use logical package identifiers to include their headers, like so:

```c
#include <bar>
#include <hello.world>  // Nested package
```

This is possible because bake copies header files of projects to the bake environment, and bake projects always are expected to have a header with the name of the project. This approach ensures that projects always can use the same include path, regardless of where packages are installed, and also prevents name collisions between header files of different projects.

There are of course many more differences, and this example covers only a small subset of the features of both CMake and bake, but hopefully it provides a bit more insight into how the two tools are different.

### Can I link with non-bake libraries?
Yes. You will have to add the library not as a bake dependency, but as a library for the C driver. This example shows how to link with the `m` (math) library:

```json
{
    "id": "my_app",
    "type": "application",
    "lang.c": {
        "lib": ["m"]
    }
}
```

This makes the project configuration platform-specific which is not ideal. To improve the above configuration, we should ensure that `m` is only added on Linux (MacOS doesn't have a `m` library):

```json
{
    "id": "my_app",
    "type": "application",
    "${os linux}": {
        "lang.c": {
            "lib": ["m"]
        }
    }
}
```

For big projects, all these different rules could complicate the `project.json` quite a bit. It would be even better to encapsulate this logic in a separate bake project:

```json
{
    "id": "math",
    "type": "package",
    "value": {
        "language": "none"
    },
    "dependee": {
        "${os linux}": {
            "lang.c": {
                "lib": ["m"]
            }
        }
    }
}
```

This creates a new "math" package that you can now specify as regular bake dependency. The `"language": "none"` attribute lets bake know that there is no code to build, and this is a configuration-only project. The `dependee` attribute tells bake to not apply the settings inside the JSON object to the `math` project, but to the projects that depend on `math`.

We can now change the configuration of `my_app` into this:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["math"]
    }
}
```

### How do I install bake packages?
Bake relies on git to store packages. To install a package, use the `bake clone` command with a GitHub repository identifier:

```
bake clone SanderMertens/example
```

If your git repository is not hosted on GitHub, simply provide the full git URL:

```
bake clone https://my_git_server.com/example
```

Any URL that is accepted by git is accepted by bake.

### How does bake find dependencies of cloned projects?
When bake clones a package with dependencies, it will try to also install those dependencies. It does this by taking the git URL specified to `bake clone`, and replacing the package name with the dependency name. For example, if the https://github.com/SanderMertens/example git repository depends on project `foobar`, bake would also look for https://github.com/SanderMertens/foobar.

Future versions of bake may provide more intelligent ways to locate packages.

### Why use JSON for project configuration?
A number of people have asked me why I used JSON for project configuration. There are two reasons: 
- It is a ubiquitous language that everyone understands, 
- It has a C parser that can be easily embedded into bake without adding dependencies

A disadvantage of JSON is that while it is fine for trivial configurations, it can get a bit unwieldy once project configurations get more complex. In bake however, you can encapsulate complexity into a configuration-only project, and then include that project as a dependency in your project configuration ([example](https://github.com/SanderMertens/bake/tree/master/examples/pkg_w_dependee)).

Additionally, bake is not like traditional build tools where you specify rules with inputs and outputs in your project configuration. If you want to, for example, add a code generation step to your build, you write a driver for it, and then include the driver in your project configuration.

### How can I specify a custom compiler?
The drivers for C & C++ projects by default use gcc/g++ (on Linux) and clang/clang++ (on MacOS). If you want to change the default compiler, you can set the `CC` (for C) and `CXX` (for C++) environment variables, as long as the command line options are compatible with gcc. Instead of setting the environment variables manually, you can make them part of a bake environment like this:

```demo
bake export CC=clang --env clang_env
```

To use the environment, and build with clang, you can then invoke bake like this:
```demo
bake --env clang_env
```

To export `CC` or `CXX` to the default environment, simply leave out the `--env` argument.

### What is the difference between BAKE_HOME and BAKE_TARGET?
`BAKE_HOME` is where all the installed projects are stored. These are projects that you did not build on your machine, but installed from a git repository. `BAKE_TARGET` is the location where all the projects you built are stored. By default, `BAKE_HOME` and `BAKE_TARGET` are set to the same location, which is `BAKE_HOME`. Whereas bake installs projects directly to `BAKE_HOME`, when building your own projects, they are stored in `$BAKE_TARGET/arch-os-config` (for example: `x64-darwin-debug`).

### Where does bake store my binaries?
Bake always stores binaries in the `bin/arch-os-config` directory of your project. When your project is a public project (this is the default) binaries are also copied to the target bake environment, which by default is `$BAKE_TARGET/arch-os-config/bin` or `$BAKE_TARGET/arch-os-config/lib`. By default, `$BAKE_TARGET` is set to `$HOME/bake`, just like `$BAKE_HOME`.

To prevent a project from being stored in `BAKE_TARGET`, add this to the `project.json`:

```json
"value": {
    "public": false
}
```

### How do I do a release build?
By default, binaries are built with the default debug configuration. To build a release configuration, add `--cfg release` to your bake command. You can add/change configurations in the bake configuration file. See "Configuring Bake" for more details.

### How to use different versions of the same package?
Bake does not support having different versions of a package in the same environment. If you want to use different versions of the same package on a machine, you have to use different bake environments. You can do this by setting the `BAKE_TARGET` environment variable. By default, this variable is set to `$HOME/bake`, but you can override it to any path you want. You can set `BAKE_TARGET` in a new environment called `my_env` (for example) with this command:

```
bake export BAKE_TARGET=/home/user/my_path --env my_env
```

To set the variables in this environment, add `--env my_env` to any bake command, like this:
```demo
bake --env my_env
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
---------|------|------
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
