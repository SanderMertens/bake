#include <include/pkg_dependee.h>
#include <stdio.h>
#include <math.h>

void pkg_dependee() {
    printf("pkg_dependee cos(1.0) = %f\n", cos(1.0));
    pkg_w_dependee();
    pkg_helloworld();
}
