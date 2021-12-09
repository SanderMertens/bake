#include <examples_c_pkg_dependency_private.h>
#include <stdio.h>

static
void pkg_dependency_private(void) {
    pkg_helloworld();
    printf("pkg_dependency_private\n");
}
