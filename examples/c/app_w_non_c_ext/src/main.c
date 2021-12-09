#include <examples_c_app_w_non_c_ext.h>
#include <stdio.h>
#include "foo.inl"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    print_helloworld();
    return 0;
}
