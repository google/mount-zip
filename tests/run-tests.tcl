#!/bin/sh

#restart usinf tclsh \
exec tclsh "$0" "$@"

package require Tcl 8.4

# source files
set linuxSources "linux-sources"
set cdImage "/home/share/Soft/Linux_dist/debian-40r1-amd64-CD-1.iso"

# utilities
set fuseZip "../fuse-zip"
set fusermount "fusermount"
set kioCopy "kio_copy/kio_copy"
set uzip "/usr/share/mc/extfs/uzip"
set zip "zip"

# print real user system, in seconds
set timeCmd "/usr/bin/time -f \"%e\t%U\t%S\" -o /dev/stdout"

# paths
set tmpDir "fusezip-tests-temp"
set mountPoint "$tmpDir/mount-point"
set extractDir "$tmpDir/extract-dir"
set archive "$tmpDir/archive.zip"

set participants {
    fuse-zip
    kio-zip
    mc-uzip
}

set tests {
    zip-random      "Compress files generated from /dev/urandom and /dev/zero"
    unzip-random    "Uncompress files generated from /dev/urandom and /dev/zero"
}

###############################################################################
# FUNCTIONS
###############################################################################

proc prepareRandomData {} {
    global extractDir

    set size 100

    puts "Generating random data... "
    puts -nonewline "Generating random data... "
    file mkdir $extractDir/data
    exec dd if=/dev/urandom of=$extractDir/data/random bs=1M count=$size > /dev/null 2> /dev/null
    exec dd if=/dev/zero of=$extractDir/data/zero bs=1M count=$size > /dev/null 2> /dev/null
    puts "OK"
}

proc cleanRandomData {} {
    global extractDir

    file delete -force $extractDir/data
}

proc prepareData {action} {
    switch -exact $action {
        zip-random {
            prepareRandomData
        }
    }
}

proc cleanData {action} {
    global archive

    switch -exact $action {
        zip-random {
            cleanRandomData
        }
        unzip-random {
            file delete $archive
        }
    }
}

###############################################################################
# TESTS
###############################################################################

proc timeExec {args} {
    global timeCmd
    return [ eval [ concat exec $timeCmd $args ] ]
}

proc fusezipExec {cmd} {
    global fuseZip archive mountPoint fusermount
    return [ timeExec sh -c "\
        $fuseZip $archive $mountPoint & pid=\$!;\
        $cmd;\
        $fusermount -u -z $mountPoint;\
        wait \$pid;\
    " ]
}

proc fuse-zip {action} {
    global mountPoint extractDir

    switch -exact $action {
        zip-random {
            return [ fusezipExec "cp -r -t $mountPoint $extractDir/data" ]
        }
        unzip-random {
            return [ fusezipExec "cp -r $mountPoint/* $extractDir" ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc kio-zip {action} {
    global archive extractDir kioCopy

    switch -exact $action {
        zip-random {
            return "n/a n/a n/a"
        }
        unzip-random {
            return [ timeExec $kioCopy zip://[ file normalize $archive ] file://$extractDir ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc mc-uzip {action} {
    global zip uzip archive extractDir

    switch -exact $action {
        zip-random {
            # mc vfs cannot create empty archives
            exec $zip $archive $::argv0
            return [ timeExec sh -c "\
                $uzip mkdir $archive data
                $uzip copyin $archive data/random $extractDir/data/random;\
                $uzip copyin $archive data/zero $extractDir/data/zero;\
            " ]
        }
        unzip-random {
            file mkdir $extractDir/data
            return [ timeExec sh -c "\
                $uzip copyout $archive data/random $extractDir/data/random;\
                $uzip copyout $archive data/zero $extractDir/data/zero;\
            " ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

###############################################################################
# MAIN
###############################################################################

puts "##########################"
puts "# VFS performance tester #"
puts "##########################"
puts ""

file mkdir $tmpDir $mountPoint $extractDir

set results ""

if [ catch {
    foreach {t l} $tests {
        set res ""
        prepareData $t
        foreach p $participants {
            puts "Running test $t for $p... "
            puts -nonewline "Running test $t for $p... "

            lappend res $p [ $p $t ]

            puts "OK"
        }
        cleanData $t
        lappend results $t $l $res
    }
} err ] {
    puts "FAILED"
    puts "Error message: $err"

    catch {exec $fusermount -uz $mountPoint}
    file delete -force $tmpDir $mountPoint $extractDir
    exit
}

file delete -force $tmpDir $mountPoint $extractDir

puts ""
puts "Program\t\treal\tuser\tsystem"
foreach {test label res} $results {
    puts "Test: $label \($test\)"
    foreach {p t} $res {
        puts "  $p\t[ join $t \t ]"
    }
    puts ""
}

