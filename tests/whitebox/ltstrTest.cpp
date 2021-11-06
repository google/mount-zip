////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2015-2021 by Alexander Galanin                          //
//  al@galanin.nnov.ru                                                    //
//  http://galanin.nnov.ru/~al                                            //
//                                                                        //
//  This program is free software: you can redistribute it and/or modify  //
//  it under the terms of the GNU General Public License as published by  //
//  the Free Software Foundation, either version 3 of the License, or     //
//  (at your option) any later version.                                   //
//                                                                        //
//  This program is distributed in the hope that it will be useful,       //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//  GNU General Public License for more details.                          //
//                                                                        //
//  You should have received a copy of the GNU General Public License     //
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.//
////////////////////////////////////////////////////////////////////////////

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

