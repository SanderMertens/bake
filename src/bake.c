
#include <include/bake.h>

#define PARSE_OPTION(short, long, action)\
    if (argv[i][0] == '-') {\
        if (argv[i][1] == '-') {\
            if (long && !strcmp(&argv[i][2], long ? long : "")) {\
                action;\
                parsed = true;\
            }\
        } else {\
            if (short && argv[i][1] == short) {\
                action;\
                parsed = true;\
            }\
        }\
    }

int main(int argc, char* argv[]) {

    base_init(argv[0]);

    return 0;
}
