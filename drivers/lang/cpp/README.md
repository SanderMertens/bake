# bake-lang-cpp
This is the C++ language plugin for the bake buildtool.

## Project configuration
The C++ plugin supports properties specific to C++ projects. The C++ driver imports definitions from the C driver, and inherits its properties. For a list of the supported properties, [see the README of the C driver](https://github.com/SanderMertens/bake/blob/master/drivers/lang/c/README.md)

## Example

```json
{
    "id": "my_lib",
    "type": "package",
    "lang.cpp": {
        "export-symbols": true
    }
}
```
