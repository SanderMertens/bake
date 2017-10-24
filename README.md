# bake
Existing buildtools are either not portable, not flexible or too rigid for what we envision: a cross-platform, cross-language buildtool that imposes a well defined structure on software projects. Our initial focus is embedded languages (C/C++) since the pain is felt most accutely there.

Bake is an opinionated tool that builds packages in a certain way to ensure packages are securely linked with (hardcoded paths), don't cause name collisions (hierarchical organization) and can be automatically installed (depdendency/version management).

The initial usecase for bake is to build the corto project (https://www.corto.io) and it shares some of its codebase (located in the https://www.github.com/cortoproject/base repository), but the tool will be generic and can be utilized outside of corto as well.

## Getting Started
Bake is currently under development. If you have ideas, feature requests or want to collaborate create an issue or contact me at sander@corto.io

## Authors
- Sander Mertens - Initial work

## Legal stuff
Bake is licensed under the MIT license.

