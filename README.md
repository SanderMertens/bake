# bake
Existing buildtools are either not portable, not flexible or too rigid for what we envision: a cross-platform, cross-language buildtool that "just works" by imposing a well defined structure on projects. Our initial focus is embedded languages (C/C++) since the pain is felt most accutely there.

Bake is an opinionated tool that builds packages in a certain way to ensure packages are securely linked with (hardcoded paths), don't cause name collisions (hierarchical organization) and can be automatically installed (depdendency/version management).

The initial usecase for bake is to build the corto project (https://www.corto.io) and it shares some of its codebase (located in the https://www.github.com/cortoproject/base repository), but the tool will be generic and can be utilized outside of corto as well.

Bake is an evolution of the rake-based corto buildsystem (located in the https://github.com/cortoproject/corto repository). In practice rake appears to be harder to port to older platforms than expected alongside with a couple of other hard-to-solve issues, which triggered a rebuild in native code.

## Getting Started
Bake is currently under development. If you have ideas, feature requests or want to collaborate create an issue or contact me at sander@corto.io

## Features
- **hierarchical package repository** - bake organizes packages in a hierarchically organized local repository. Packages have identifiers that look like `hello/world`, and specifying dependencies can be done using these identifiers.
- **package development** - bake supports mixed environments where some packages are installed and some packages are under development. Using a simple algorithm bake detetcs which version of a package (development / installed) to link with.
- **multiple environments** - bake supports multiple isolated environments to allow for running isolated functionality (useful for test frameworks) or to ensure that potentially conflicting environments can coexist.
- **install misc resources** - A bake project can contain more than just code. Miscallaneous files (images, web resources, data, binaries) are installed to well defined locations alongside the package so that a package always has access to it
- **embeddable binaries** - Bake supports generating binaries that can be embedded into other projects without imposing the package hierarchy.
- **code generation** - Bake has built-in support for code generation steps
- **plugin-based design** - Implementations for different languages are loaded as plugins (packages), making it easy to add support for new languages
- **project discovery** - Bake automatically discovers projects in a directory tree and builds them in the right order using information from their dependency configuration.
- **fast execution engine** - Bake is 100% native code and leverages multi-threaded builds which drastically speeds up execution.

## Authors
- Sander Mertens - Initial work

## Legal stuff
Bake is licensed under the MIT license.

