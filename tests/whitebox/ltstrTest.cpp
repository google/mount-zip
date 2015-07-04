#include <cassert>

#include <cstdio>

#include "types.h"

int main(int, char **) {
    ltstr lt;

    assert(lt("a", "b"));
    assert(!lt("a", "a"));
    assert(!lt("", ""));
    assert(lt("ab", "abc"));
    assert(lt("abc", "abd"));

    // NOTE: not applicable because nullpath_ok=0
    // assert(!lt("","/"));

    assert(!lt("/","/"));
    assert(!lt("ab", "ab/"));
    assert(!lt("ab/", "ab"));
    assert(!lt("ab/", "ab/"));
    assert(!lt("ab////", "ab"));
    assert(lt("abc/", "abcd"));

    assert(lt("abc/", "abc/def"));
    assert(lt("abc", "abc/def"));

    return EXIT_SUCCESS;
}

