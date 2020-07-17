[![Build Status](https://travis-ci.org/SanderMertens/bake.svg?branch=master)](https://travis-ci.org/SanderMertens/bake) [![Build status](https://ci.appveyor.com/api/projects/status/or9nqredss604c39/branch/master?svg=true)](https://ci.appveyor.com/project/SanderMertens/bake/branch/master)

# bake
The Dutch IRS has a catchy slogan, which goes like this: "Leuker kunnen we 't niet maken, wel makkelijker". Roughly translated, this means: "We can't make it more fun, but we can make it easier". Bake adopts a similar philosophy. Building code (especially C/C++) will never be fun, so let's try to make it as easy and painless as possible.

To that end, bake is a build tool, build system, package manager and environment manager in one. Bake automates building code, especially for highly interdependent projects. Currently, Bake's focus is C/C++.

Bake's main features are:
- discover all projects in current directory & build them in the correct order
- clone, build and run a project and its dependencies with a single command using bake bundles 
- automatically include header files from dependencies
- use logical (hierarchical) identifiers to specify dependencies on any project built on the machine
- programmable C API for interacting with package management
- manage and automatically export environment variables used for builds

Bake depends on git for its package management features, and does _not_ have a server infrastructure for hosting a package repository. **Bake does not collect any information when you clone, build or publish projects**.

Bake is supported on the following platforms:

- Linux
- MacOS
- Windows (validated on Windows 10)

## Contents
* [Installation](#installation)
* [Getting Started](#getting-started)
* [FAQ](#faq)
* [Manual](#manual)
  * [Introduction](#introduction)
  * [Building projects](#building-projects)
  * [Running projects](#running-projects)
  * [Project kinds](#project-kinds)
  * [Project layout](#project-layout)
  * [Project configuration](#project-configuration)
  * [Project bundles](#project-bundles)
  * [Template functions](#template-functions)
  * [Templates](#templates)
  * [Configuring Bake](#configuring-bake)
  * [Installing Miscellaneous Files](#installing-miscellaneous-files)
  * [Integrating Non-bake Projects](#integrating-non-bake-projects)
  * [The Bake Environment](#the-bake-environment)
  * [Environment Variables](#environment-variables)
  * [Command line interface](#command-line-interface)
  * [Writing drivers](#writing-drivers)
* [Authors](#authors)
* [Legal stuff](#legal-stuff)

## Installation

Install bake using the following commands:

On Linux/MacOS:
```demo
git clone https://github.com/SanderMertens/bake
make -C bake/build-$(uname)
bake/bake setup
```

On Windows:
```
git clone https://github.com/SanderMertens/bake
cd build-Windows
nmake
cd ..
bake setup
```
On Windows, make sure to open a **visual studio command prompt**, as you will need access to the visual studio build tools. After bake is installed, you can invoke bake from any command prompt. If you want to install bake for all users, open the command prompt as administrator.

Bake installs a script to a location that is accessible for all users (`C:\Windows\System32` on Windows or `/usr/local/bin` on Linux). This however often requires administrator or root privileges. If you do not want bake to install this script and you get a password prompt, just press Enter untill the setup resumes.
 
In case you did not install bake for all users, you need to manually add `$HOME/bake` (`%USERPROFILE%\bake` on Windows) to your `PATH` environment variable. You can do this on a command prompt by doing:

On Linux:
```
export PATH=$PATH:$HOME/bake
```

On Windows:
```
set PATH=%PATH%;%USERPROFILE%\bake
```
 
After you've installed bake once, you can upgrade to the latest version with:

```demo
bake upgrade
```

## Getting started

The following commands are useful for getting started with bake. Also, check out the `bake --help` command, which lists all the options and commands available in the bake tool.

### Create and run new project
To create and run a new bake application project called `my_app`, run the following commands:

```demo
bake new my_app
bake run my_app
```

You can also run projects in interactive mode. This will automatically rebuild and restart an application when a project file changes. To run in interactive mode, simply add `--interactive` to the bake command:

```demo
bake run my_app --interactive
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
Build a project and its dependencies directly from a git repository using this command:

```demo
bake clone https://github.com/SanderMertens/example
```

### Export an environment variable to the bake environment
Bake can manage environment variables that must be set during the build. To export an environment variable to the bake environment, use this command:

```demo
bake export VAR=value
```
Alternatively, if you want to add a path to an environment variable like `PATH` or `LD_LIBRARY_PATH`, use this:

```demo
bake export PATH+=/my/path
```

These variables are stored in a configuration file called `bake.json` in the root of the bake environment, which by default is `$HOME/bake`.

To export the bake environment to a terminal, use:
```
export `bake env`
```

## FAQ

### Bake is built under the GPL3.0 license. Does this mean I cannot use it for commercial projects?
No. As long as you do not distribute bake (either as source or binary) as part of your (closed source) deliverable, you can use bake for building your projects. This is no different than when you would use make for your projects, which is also GPL licensed.

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
Bake is different from package managers like conan, brew or apt-get. It is intended as a tool for developers to easily import and use code from other developers. Bake for example does not have an online package repository, does not distribute binaries and by default stores packages in the user $HOME directory. Its only dependency is git, so no data is collected by bake when you download or publish packages.

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

### I want to wrap a C library so I can use it as a bake dependency. How do I do this?
It would be nice if we could wrap `libm.so` from the previous example in a bake `math` package, aso we don't have to repeat this configuration for every project. Bake lets us do this with the `"dependee"` attribute:

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

We can now use the math package like this:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "use": ["math"]
    }
}
```

### Where can I find the configuration options for C and C++ projects?
You can find language-specific configuration options in the README of the language driver projects:

For C: https://github.com/SanderMertens/bake/tree/master/drivers/lang/c

For C++: https://github.com/SanderMertens/bake/tree/master/drivers/lang/cpp

### What is a driver?
All of the rules and instructions in bake that actually builds code is organized in bake "drivers". Drivers are shared libraries that bake loads when a project needs them. The most common used drivers are "language drivers", which contain all the build instructions for a specific language, like C or C++. Bake automatically loads the language drivers based on the `"language"` attribute in your `project.json`, as is specified here:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "language": "c"
    }
}
```
By default the language is set to "c", so if you do not specify a language, your project will build as a C project.

### What does "lang.c" mean? When do I need to specify it? 
In some cases you will want to provide configuration options that are specific to a language, like linking with C libraries on your system, or provide additional compiler flags. In that case, you have to tell bake that the configuration you are about to specify is for a specific driver. This is where "lang.c" comes in:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "language": "c"
    },
    "lang.c": {
        "lib": ["m"]
    }
}
```

The `"lang.c"` member uniquely identifies the bake driver responsible for building C code, and bake will make all of the attributes inside the object (`"lib"`) available to the driver.

If you want to build a C++ project, instead of using the `"lang.c"` attribute, you have to use the `"lang.cpp"` attribute, which identifies the C++ driver:

```json
{
    "id": "my_app",
    "type": "application",
    "value": {
        "language": "c++"
    },
    "lang.cpp": {
        "lib": ["m"]
    }
}
```

### For C++ projects, should I specify cpp or c++ for the language attribute?
You can use either, but for specifying driver-specific configuration you always have to use `lang.cpp`.

### How can I see a list of the available drivers?
The following command will show you a list of the available drivers:

```
bake list bake.*
```

Everything except for `bake.util` is a driver. If you just built bake for the first time, this will only show the `"lang.c"` and `"lang.cpp"` drivers.

### Can I load more than one driver?
Yes! You can load as many drivers as you want. If you want to add a driver, simply add it to your configuration like this:

```json
{
    "id": "my_app",
    "type": "application",
    "my_custom_driver": { }
}
```

### Are there any example drivers I can use as a template?
Driver documentation is a bit lacking at the moment, but we will eventually address that. In the meantime, you can take a look at the C driver to see what a fully fletched driver looks like:

https://github.com/SanderMertens/bake/tree/master/drivers/lang/c

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

A disadvantage of JSON is that while it is fine for trivial configurations, it can get a bit unwieldy once project configurations get more complex. In bake however, you can encapsulate complexity into a configuration-only project, and then include that project as a dependency in your project configuration ([example](https://github.com/SanderMertens/bake/tree/master/examples/c/pkg_w_dependee)).

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

### Where does bake store my binaries?
Bake always stores binaries in the `bin/arch-os-config` directory of your project. When your project is a public project (this is the default) binaries are also copied to the target bake environment, which by default is `$BAKE_HOME/arch-os/config/bin` or `$BAKE_HOME/bake/arch-os/config/lib`. By default, `$BAKE_HOME` is set to `~/bake`.

To prevent a project from being stored in the bake environment, add this to the `project.json`:

```json
"value": {
    "public": false
}
```

Usually you do not need to know where binaries are stored, as you can run applications with `bake run`, and specify dependencies by using their logical name.

### How do I do a release build?
By default, binaries are built with the default debug configuration. To build a release configuration, add `--cfg release` to your bake command. You can add/change configurations in the bake configuration file. See "Configuring Bake" for more details.

### How to use different versions of the same package?
Bake does not support having different versions of a package in the same environment. If you want to use different versions of the same package on a machine, you have to use different bake environments. You can do this by setting the `BAKE_HOME` environment variable. By default, this variable is set to `$HOME/bake`, but you can override it to any path you want. You can set `BAKE_HOME` in a new environment called `my_env` (for example) with this command:

```
bake export BAKE_HOME=/home/user/my_path --env my_env
```

To set the variables in this environment, add `--env my_env` to any bake command, like this:

```demo
bake --env my_env
```

## Manual

### Introduction
The goal of bake is to bring a level of abstraction to building software that is comparable with `npm`. Tools like `make`, `cmake` and `premake` abstract away from writing your own compiler commands by hand, but still require users to create their own build system, with proprietary mechanisms for specifying dependencies, build configurations etc.

This makes it difficult to share code between different people and organizations, and is arguably one of the reasons why ecosystems like `npm` are thriving, while ecosystems for native code are fragmented.

Bake is therefore not just a build tool like `make` that can automatically generate compiler commands. It is also a build system that specifies how projects are organized and configured. When a project relies on bake, a user does, for example, not need to worry about how to link with it, where to find its include files or whether binaries have been built with incompatible compiler flags.

A secondary goal is to create a zero-dependency build tool that can be easily ported to other platforms. Whereas other build tools exist, like `make`, `premake`, `rake` and `gradle`, they all rely on their respective ecosystems (`unix`, `lua`, `ruby`, `java`) which complicates writing platform-independent build configurations. Bake's only dependency is the C runtime.

### Creating a new Project
You can create a new bake project with the `bake new` command. This command has a few options, which lets you create different kinds of projects (see "Project Kinds"). By default, bake creates an "application" project, which is a standard executable. To create a new application project called `my_app`, run the following command:

```
bake new my_app
```

This will create a new directory called `my_app` with the contents of a basic bake application project. If you want to create a bake package (a shared library), you can simply add `--package` to the command:

```
bake new my_pkg --package
```

When a new project is created, its metadata is also stored in the bake environment. That means that the project is now discoverable by bake, and can be used as dependency of other projects. You can inspect the bake environment with this command:

```
bake list
```

Your new project should show up in the list of projects.

Bake lets you create projects with nested identifiers, like `foo.bar`. This lets you create hierarchies of projects. The `.` notation is used to denote different elements in the project identifier. To use nested identfiers, simply specify their name with bake new:

```
bake new foo.bar
```

This will create a new directory `foo-bar`. The project will appear as `foo.bar` when you do `bake list`.

### Building Projects
Bake's primary task is to build the code in your projects, and generate binaries in a reliable and reproducible way. You can simply build a bake project by invoking the `bake` command:

```
bake
```

This will recursively discover and build all bake projects in the current directory. The command is synonymous for running bake with the `build` action:

```
bake build
```

Alternatively you can also specify a directory to build, like so:

```
bake my_directory
bake build my_directory
```

Bake has a number of actions, of which the following are related to building your project:

```
bake build
bake rebuild
bake clean
```

The `build` action incrementally builds your project, and will reuse artefacts from previous builds, like object files and binaries. The `rebuild` action cleans artefacts from previous builds, and is then followed by a regular build. The `clean` action cleans all build artefacts for the project.

Bake allows you to build for multiple platforms and build configurations from the same source tree, as it stores build artefacts in locations that are platform/configuration specific. When you do a `bake rebuild`, only the artefacts for the current platform / configuration are cleaned, whereas `bake clean` cleans artefacts for all platforms / configurations.

#### Discovery
Bake automatically discovers projects in the provided path, or current directory if no path was specified. It will then order the discovered projects based on their dependencies, so that they are built in the correct order. This removes the need for building makefiles in which you explicitly have to maintain the build order for your projects. Bake uses the information in the `use` attribute of your project configuration (see [Project Configuration](#project-configuration)).

Bake will not attempt to discover projects in subdirectories of projects if those subdirectories have special meaning. The following directories are skipped, _only_ if they are found inside a bake project directory:

- src
- include
- config
- data
- test
- etc
- lib
- bin
- install
- examples
- .bake_cache

Additionally, bake will skip any directories that start with a `.`.

#### Build configurations
Bake lets you build projects with different build configurations, like `debug` and `release`. By default, bake has built-in settings for `debug` and `release` configurations. You can specify a build configuration with the `--cfg` flag:

```
bake my_project --cfg release
```

The default configuration is `debug`. The difference between `debug` and `release` is that `debug` disables optimizations and enables debugging code (release adds the `-DNDEBUG` flag). Furthermore, `debug` builds add compiler debugging information (like `-g` in gcc).

Bake never mixes binaries between build configurations. Therefore, if you build a project in `release` mode, but its dependencies haven't been built in `release` mode yet, the build will fail. 

#### Recursive builds
To make working across configurations easier, bake lets you do so called "recursive builds". These builds don't just build the current project, but also all dependencies for a project (and dependencies of dependencies, hence recursive builds). When building a project in `release` mode, but all dependencies have been built in `debug` mode, you can simply do:

```
bake my_project --cfg release -r
```

The `-r` flag enables recursive building, which will, in addition to the current project, rebuild all dependencies in release mode as well. Recursive builds work for any dependency that is available in the bake environment. Bake keeps track of where the source files of your projects are located on disk, which is how it can start a build for a dependency, even when it is not discoverable from the location where bake was invoked from.

### Running Projects
You can run bake projects by using `bake run`, followed by either a folder or a project id:

```
bake run foo.bar
bake run my_directory
```

This only works for application projects (see [Project Kinds](#project-kinds)). Bake will automatically start the executable and monitor its status. Before running the project, bake will first attempt to do a recursive build (see [Recursive builds](#recursive-builds)) so that the project and all its dependencies are built and are available for the right configuration. You can specify a configuration just like you would when building:

```
bake run foo.bar --cfg release
```

Additionally, bake lets you do interactive builds, which monitor changes from your project, and rebuild the project when a change occurs. To start an interactive build, add the `--interactive` flag:

```
bake run foo.bar --interactive
```

Currently bake does not monitor changes in the source code of dependencies, though it may do so in the future.

### Project Kinds
Bake supports different project kinds which are configured in the `type` property of a `project.json` file. The project kind determines whether a project is a library or executable, whether a project is installed to a bake environment and whether a project is managed or not. The following table shows an overview of the different project kinds:

Project Kind | Description
-------------|----------
application  | Executable
package      | Shared object
template     | Template for new bake projects

#### Public vs private projects
A public project is a project that is installed to the bake environment. In this environment, bake knows where to find include files, binaries and other project resources. This allows other projects to refer to these resources by the logical project name, and makes specifying dependencies between projects a lot easier.

Private projects are projects that are not installed to a bake environment. Because of this, these projects cannot be located by other projects. Private projects may depend on public projects, but public projects cannot depend on private projects. Binaries of private objects are only stored in the bin folder in the project root.

### Project Layout
Each bake project uses the same layout. This makes it very easy to build bake projects, as bake always knows where to find project configuration, include files, source files and so on. A bake project has at least three files:

Directory / File | Description
-----------------|------------
project.json | Contains build configuration for the project
src | Contains the project source files
include | Contains the project header files

In addition, a bake project can contain the following optional directories:

Directory / File | Description
-----------------|------------
etc | Miscellaneous project-specific files
install | Miscellaneous files that can be used by all projects in bake environment
templates | Template projects that are automatically installed when building the project

Bake will by default build any source file that is in the `src` directory. If the project is public, files in the `include`, `etc` and `install` folders will be soft-linked to the bake environment on Linux/MacOS, and copied when using Windows.

When bake builds a project, build artefacts are stored in these directories:

Directory / File | Description
-----------------|------------
bin | Contains project binaries
.bake_cache | Contains temporary files, like object files and precompiled headers

Bake stores temporary files in platform- and configuration specific directories, so that you can safely do debug/release builds, and builds for different operating systems from the same source directory.

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
use_runtime | list(string) | Specify dependencies that a project needs at runtime, but that are not used/linked with during build time.
use-bundle | list(string) | Bundles to be used by project. If the bundles are also specified as a repository, bake will be able to automatically download & find dependencies in the specified bundles.
amalgamate | bool | Experimental. Generate amalgamated header and source file for the project.
sources | list(string) | List of paths that contain source files. Default is `src`. The `$SOURCES` rule is substituted with this value.
includes | list(string) | List of paths that contain include files.
keep_binary | bool | Do not clean binary files when doing bake clean. When a binary for the target platform is present, bake will skip the project. To force a rebuild, a user has to explicitly use the `bake rebuild` command.

The `cflags` attribute is specified inside the `lang.c` property. This is because `cflags` is a property specific to the C driver. For documentation on which properties are valid for which drivers, see the driver documentation.

#### Private dependencies
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

### Template Functions
Bake property values may contain calls to template functions, which in many cases allows project configuration files to be more generic or less complex. Additionally, template functions can be used to parameterize bake template projects. Template functions take the following form:

```
${function_name argument}
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
---------|-------------
locate | Locate project paths in the bake environment
os | Match or return operating system
language | Match or return target language
id | Return project identifier

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

#### os
The `os` function can be used to specify platform-specific settings, or use the platform string in a path. The following example demonstrates how it can be used:

```json
{
    "${os linux}": {
        "include": ["includes/linux"]
    }
}
```

The `os` function can match both operating system and architecture. The following expressions are all valid:

- x86-linux
- darwin
- x86_64
- x86_64-darwin
- i386

For a full description of the expressions that are supported, see the documentation of `ut_os_match`.

The `os` function may be nested:

```json
{
    "${os linux}": {
        "include": ["includes/linux"],
        "${os x86_64}": {
            "lib": ["mylib64"]
        },
        "${os x86}": {
            "lib": ["mylib32"]
        }
    }
}
```

If no argument is provided to `os`, it will return the current architecture in the following format:

```
arch-os
```

This format is consistent with the platform-specific bin path under which bake stores project binaries (like `bin/x86-linux`).

#### language
The `language` function matches or returns the language of the project. 

When an argument is provided, it is matched against the current language:
```json
{
    "${language c}": { 
        "lib": ["my_c_lib"]
    },
    "${language cpp}": {
        "lib": ["my_cpp_lib"]
    }
}
```
The function accepts both `cpp` and `c++` for C++ projects.

When the argument is ommitted, the current language is returned. This is particularly effective in combination with the `dependee` attribute, when dependees can be implemented in different languages:

```json
{
    "dependee": {
        "lang.${language}": {

        }
    }
}
```

#### id
The `id` function returns the current project id in various formats. When the function is used without arguments, it returns the project id as it appears in the `project.json` file:

```
${id}
```

To obtain the id in other formats, the following arguments can be passed to the `id` functions:

Parameter | Description | Example
----------|-------------|---------
no parameter | | foo.bar
base | Last element of an id | bar
upper | Upper case, replace '.' with '\_'. Used for macro's | FOO_BAR
dash | Replace '.' with '-'. Used for repository names | foo-bar
underscore | Replace '.' with '\_'. Used for variable names | foo_bar

### Project bundles
Bake bundles let projects specify in which repositories their dependencies are located, and which revision of those dependencies they require. All repositories and their revisions in a bundle are expected to be compatible with each other. This makes it easier to work with projects that have many (open source) dependencies, where the dependencies have their own release cycles and rules for versioning.

If bundles are setup correctly, the following commands can all automatically download and build the correct set of dependencies:

Run directly from a repository:
```
bake run https://github.com/SanderMertens/example
```

Clone a bake project and its dependencies:
```
bake clone https://github.com/SanderMertens/example
```

Recursively build a project:
```
bake -r example
```

This is a simple example of a bundle configuration:

```json
{
    "id": "example",
    "type": "application",
    "value": {
        "use": ["example_library"]
    },
    "bundle": {
        "repositories": {
            "example_library": "https://github.com/SanderMertens/example_library"
        }
    }
}
```

This example only contains information about where the `example_library` project can be found. When a user clones, runs or recursively builds the project, bake will automatically clone the `example_library` project if it is not locally available. Alternatively, this information can be stored in a separate project:

```json
{
    "id": "example_bundle",
    "type": "package",
    "value": {
        "language": "none"
    },
    "bundle": {
        "default-host": "https://github.com/SanderMertens",
        "repositories": {
            "example_library": "SanderMertens/example_library",
            "foo.bar": "SanderMertens/foo-bar",
            "hello.world": "SanderMertens/hello-world"
        }
    }
}
```

This is a configuration for a project that only contains bundle information. Note how it uses the `default-host` attribute to shorten the URLs. A bake project can automatically load this bundle when it is built by specifying the bundle in the `use-bundle` attribute:

```json
{
    "id": "example",
    "type": "application",
    "value": {
        "use-bundle": ["example_bundle"]
    },
    "bundle": {
        "repositories": {
            "example_bundle": "https://github.com/SanderMertens/example_bundle"
        }
    }    
}
```

Note how the project also adds the bundle project to the `repositories` list in its `bundle` section. This is not required, but is recommended so that bake will be able to clone the bundle if `example` is built in an environment that does not contain `example_bundle`. If the repository configuration is ommitted and `example_bundle` cannot be found by bake the project will fail to build.

If a project wants to use a specific branch, commit and/or tag of a repository, it can do so by adding a `refs` property to the `bundle` section:

```json
{
    "id": "example_bundle",
    "type": "package",
    "value": {
        "language": "none"
    },
    "bundle": {
        "default-host": "https://github.com/SanderMertens",
        "repositories": {
            "example_library": "SanderMertens/example_library",
            "foo.bar": "SanderMertens/foo-bar",
            "hello.world": "SanderMertens/hello-world"
        },
        "refs": {
            "v1.0": {
                "example_library": {
                    "branch": "master",
                    "tag": "v1.0"
                },
                "foo.bar": {
                    "branch": "master",
                    "tag": "v1.0"
                },
                "hello.world": {
                    "branch": "master",
                    "commit": "52ba2e129a6359f06f3437e7f46b9f466464b495"
                }
            }
        }
    }
}
```

To use this specific set of revisions, a project should specify its id in the `use-bundle` property, like so: 

```json
{
    "use-bundle": ["example_bundle:v1.0"]
}
```

The above bundle configuration can be shortened with the `default-branch` and `default-tag` properties:

```json
{
    "id": "example_bundle",
    "type": "package",
    "value": {
        "language": "none"
    },
    "bundle": {
        "default-host": "https://github.com/SanderMertens",
        "repositories": {
            "example_library": "SanderMertens/example_library",
            "foo.bar": "SanderMertens/foo-bar",
            "hello.world": "SanderMertens/hello-world"
        },
        "refs": {
            "v1.0": {
                "default-branch": "master",
                "default-tag": "v1.0",
                "hello.world": {
                    "commit": "52ba2e129a6359f06f3437e7f46b9f466464b495"
                }
            }
        }
    }
}
```

In cases where the revision id is the same as the tag, the `tag` and `default-tag` properties can be ommitted, as bake will automatically use this id as the tag when a project has no tag and no commit specified. Additionally, if no branch is specified, master is assumed. Therefore the above configuration is equivalent to this:

```json
{
    "id": "example_bundle",
    "type": "package",
    "value": {
        "language": "none"
    },
    "bundle": {
        "default-host": "https://github.com",
        "repositories": {
            "example_library": "SanderMertens/example_library",
            "foo.bar": "SanderMertens/foo-bar",
            "hello.world": "SanderMertens/hello-world"
        },
        "refs": {
            "v1.0": {
                "hello.world": {
                    "commit": "52ba2e129a6359f06f3437e7f46b9f466464b495"
                }
            }
        }
    }
}
```

This allows for shorter bundle configurations, where the bundle only needs to capture deviations from what is considered good practice. The exception to this rule is if a set of revisions has the `default` id. `default` specifies the set of revisions that are loaded when no bundle id, or `default`, is specified. 

It is possible to add bundles to the bake configuration. This ensures that repositories in the environment are guaranteed to be of the revisions in the configured bundle. If a bundle is configured with bake, and a project is loaded that has a different configuration (repository, branch, tag, commit) for a project, it will fail to build. To add a bundle to bake's configuration, use the `bake use` command:

```
bake use example_bundle:v1.0
```

Attempting to load a project with different settings for any of the projects in the `example_bundle` bundle will result in an error. For example, if a project specifies project `hello.world` with tag `v1.0`, bake will reject it. Additionally, if a project specifies project `foo.bar` with tag `v2.0` bake will also reject the configuration. Even though `foo.bar` is not explicitly added to `v1.0`, bake adds it automatically when it loads the bundle configuration.

If a project specifies its repository URL in the `repository` property, it must match with bundle configurations, or bake will reject the project. For example, trying to load the following project configuration against the previous bundle will fail because the URL does not match:

```json
{
    "id": "foo.bar",
    "type": "library",
    "value": {
        "repository": "https://gitlab.com/foo-bar"
    }
}
```

Bake will not automatically download dependencies during a build. Dependencies are only automatically downloaded for recursive builds, which are enbled with the `-r` argument:

```
bake -r
```

### Templates
Bake lets you create template projects which contain boilerplate code for common types of applications. Template projects look like regular projects, with two exceptions:

- The project type is set to `template`
- Files may contain template functions that are resolved when instantiating a template

You can create a new template project by specifying the `--template` flag when using the `bake new` command:

```
bake new my_template --template
```

This creates a new template project which can be instantiated like this:

```
bake new my_app -t my_template
```

A template project can be parameterized using bake template functions (see previous chapter), like so:

#### Using template functions 
```c
int main(int argc, char *argv[]) {

    printf("Hello ${id}!");

    return 0;
}
```

Template functions may occur at any point in your files. Not all files in a project are parsed. Only files with the following extensions are considered by the template parser:

- c, cpp, h, hpp, html, js, css, json, md, sh, bat, lua, python, java, cs, make

Additionally, files with the following filename will be considered:

- Makefile

These lists may be extended with additional extensions and filenames.

Additionally, filenames may also be parameterized with bake template functions. The syntax for doing so is (`xxx` is a placeholder for parts of the filename):

```
xxx__<template function>.<file extension>
xxx__<template function>_<template argument>.<file extension>
xxx__<template function>__xxx.<file extension>
xxx__<template function>_<template argument>__xxx.<file extension>
```

For example, if you want a source file in your template project to have the base name of the instantiated project, you can name it like this:

```
__id_base.c
```

This is equivalent to the template function:
```
${id base}.c
```

#### Running templates
If you are developing a new template, you'll often find yourself wanting to instantiate it to test modifications to the template. To make this process easier, bake lets you instantiate templates directly. Simply do:

```
bake run my_template --template
```

where `my_template` is the template name. This will cause bake to automatically instantiate a new temporary project with the template.

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

### Installing Miscellaneous Files
Files in the `install` and `etc` directories are automatically copied to the project-specific locations in the bake environment, so they can be accessed from anywhere (see below). The `install` folder installs files directly to a location where other projects can also access it, whereas files in `etc` install to the project-specific location in the bake environment. For example, the following files:

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
$BAKE_HOME/platform/config/etc/my_app/index.html
$BAKE_HOME/platform/config/etc/my_app/style.html
$BAKE_HOME/platform/config/etc/image.jpg
$BAKE_HOME/platform/config/etc/manual.pdf
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
When a project needs to link with an external binary, one option is to install it to a global location. The bake equivalent is to install it to the `lib` directory in the bake environment. That way, the library will be installed to `$BAKE_TARGET/lib`.

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

### The Bake Environment
Bake installs projects to the "bake environment". The bake environment is located in a location specified by the `BAKE_HOME` environment variable, and contains all the metadata and binaries for public projects, miscellaneous files and templates. By default, the bake environment is located in `~/bake`. A different location can be specified by changing the value of the `BAKE_HOME` environment variable.

Projects in the bake environment can be automatically discovered and linked with by using their logical name. Here is an example of two public projects, one `application` and one `package`, where the `application` depends on the `package`:

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

To get an overview of the projects stored in the bake environment, you can do:

```
bake list
```

The bake environment stores platform-specific data (such as binaries) in a location that is specific to a platform and build configuration. For example, if you are doing a debug build on Windows, you will find a directory in `$BAKE_HOME` called:

```
x64-Windows/debug
```

During a build, this directory is accessible through the `BAKE_TARGET` environment variable. This will contain all binaries (in `bin` and `lib` directories) for projects built for this platform and build configuration. The `bake list` command shows which projects have been built for which build configuration.

### Environment Variables
Bake uses the following environment variables:

Variable | Description
---------|------------
BAKE_HOME | Location of the bake environment
BAKE_CONFIG | The current build configuration used by bake (`debug` by default)
BAKE_ENVIRONMENT | The current build environment used by bake (`default` by default)
BAKE_VERBOSITY | Specify the bake logging level (`INFO` by default)
BAKE_ARCHITECTURE | Specify the processor architecture (default is the host architecture)
BAKE_OS | Specify the operating system (default is the host operating system)

### Command line usage
The following is the output of `bake --help`

```
Usage: bake [options] <command> <path>

Options:
  -h,--help                    Display this usage information
  -v,--version                 Display version information

  --cfg <configuration>        Specify configuration id
  --env <environment>          Specify environment id
  --strict                     Manually enable strict compiler options
  --optimize                   Manually enable compiler optimizations

  --package                    Set the project type to package
  --template                   Set the project type to template
  --test                       Create a test project
  --to-env                     Clone projects to the bake environment source path (use with clone)
  --always-clone               Clone dependencies even if found in the bake environment (use with clone)

  --id <project id>            Specify a project id
  --type <package|template>    Specify a project type (default = "application")
  --language <language>        Specify a language for project (default = "c")
  --artefact <binary>          Specify a binary file for project
  -i,--includes <include path> Specify an include path for project
  --private                    Specify a project to be private (not discoverable)

  -a,--args [arguments]        Pass arguments to application (use with run)
  --interactive                Rebuild project when files change (use with run)
  --run-prefix                 Specify prefix command for run
  --test-prefix                Specify prefix command for tests run by test
  -r,--recursive               Recursively build all dependencies of discovered projects
  -t [id]                      Specify template for new project
  -o [path]                    Specify output directory for new projects

  --show-repositories          List loaded repositories (use with list)

  -v,--verbosity <kind>        Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)
  --trace                      Set verbosity to TRACE
  --debug                      Set verbosity to DEBUG (highest verbosity)

Commands:
  new [path]                   Initialize new bake project
  run [path|project id]        Build & run project
  build [path]                 Build a project (default command)
  rebuild [path]               Clean and build a project
  clean [path]                 Clean a project
  test [path]                  Run tests of project
  coverage [path]              Run coverage analysis for project
  cleanup                      Cleanup bake environment by removing dead or invalid projects
  reset                        Resets bake environment to initial state, save for bake configuration
  publish <patch|minor|major>  Publish new project version
  install <project id>         Install project to bake environment (repository must be known)
  uninstall [project id]       Remove project from bake environment
  clone <git url>              Clone and build git repository and dependencies
  use <project:bundle>         Configure the environment to use specified bundle
  update [project id]          Update an installed package or application
  foreach <cmd>                Run command for each discovered project

  env                          Echo bake environment
  upgrade                      Upgrade to new bake version
  export <NAME>=|+=<VALUE>     Add variable to bake environment

  info <package id>            Display info on a project in the bake environment
  list [filter]                List packages in bake environment

Examples:
  bake                         Build all projects discovered in current directory
  bake my_app                  Build all projects discovered in my_app directory
  bake new                     Create new application project in current directory
  bake new my_app              Create new application project in directory my_app
  bake new my_lib --package    Create new package project in directory my_lib
  bake new my_tmpl --template  Create new template project in directory my_tmpl
  bake new game -t sdl2.basic  Create new project from the sdl2.basic template
  bake run my_app -a hello     Run my_app project, pass 'hello' as argument
  bake publish major           Increase major project version, create git tag
  bake info foo.bar            Show information about package foo.bar
  bake list foo.*              List all packages that start with foo.
```

### Writing Drivers
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

- Sander Mertens - Initial work

## Legal stuff

Bake is licensed under the GPL3.0 license. The bake runtime (all code under the `util` directory) is licensed under the MIT license.
