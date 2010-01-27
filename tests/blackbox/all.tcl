#!/bin/sh
# \
exec tclsh "$0" "$@"

package require Tcl 8.5
package require tcltest 2.2

::tcltest::configure -testdir \
    [ file dirname [ file normalize [ info script ] ] ]
::tcltest::configure {*}$argv
::tcltest::runAllTests

