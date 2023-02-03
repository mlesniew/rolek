#include "utils.h"

unsigned long hex_to_dec(const char * s) {
    unsigned long ret = 0;
    for (; *s; ++s) {
        ret <<= 4;
        char c = *s;
        if ((c >= 'A') && (c <= 'F')) {
            ret += 10 + c - 'A';
        } else if ((c >= 'a') && (c <= 'f')) {
            ret += 10 + c - 'a';
        } else if ((c >= '0') && (c <= '9')) {
            ret += c - '0';
        }
    }
    return ret;
}

std::string uri_unquote(const std::string & s) {
    char ret[s.length()];

    char * w = ret;
    const char * r = s.c_str();

    while (*r) {
        if (*r == '%') {
            char c1, c2;
            if (!(c1 = *++r)) {
                break;
            }
            if (!(c2 = *++r)) {
                break;
            }
            char x[3] = {c1, c2, '\0'};
            *w = (char) hex_to_dec(x);
        } else {
            *w = *r;
        }

        ++r;
        ++w;
    }

    *w = '\0';
    return ret;
}
