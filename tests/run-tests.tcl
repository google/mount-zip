#!/bin/sh

#restart usinf tclsh \
exec tclsh "$0" "$@"

package require Tcl 8.4

# source files
set linuxSources [ file normalize "linux-sources" ]
set cdImage "/home/share/Soft/Linux_dist/debian-40r1-amd64-CD-1.iso"

# utilities
set fuseZip "../fuse-zip"
set fusermount "fusermount"
set kioCopy "kio_copy/kio_copy"
set uzip "/usr/share/mc/extfs/uzip"

set zip "zip"
set unzip "unzip"

# print real user system, in seconds
set timeCmd "/usr/bin/time -f \"%e\t%U\t%S\" -o /dev/stdout"

# paths
set tmpDir [ file normalize "fusezip-tests-temp" ]
set mountPoint "$tmpDir/mount-point"
set extractDir "$tmpDir/extract-dir"
set archive "$tmpDir/archive.zip"

set participants {
    fuse-zip
    kio-zip
    mc-uzip
}

set tests {
    zip-zero        "Compress file generated from /dev/zero"
    unzip-zero      "Uncompress file generated from /dev/zero"
    zip-urandom     "Compress file generated from /dev/urandom"
    unzip-urandom   "Uncompress file generated from /dev/urandom"
    zip-mixed       "Compress files generated from /dev/{urandom,zero}"
    unzip-mixed     "Uncompress files generated from /dev/{urandom,zero}"

    zip-linuxsrc    "Compress linux kernel sources"
    unzip-linuxsrc  "Uncompress linux kernel sources"
}

#debug
#set tests {
#    zip-linuxsrc    "compress linux sources"
#    unzip-linuxsrc    "compress linux sources"
#}

###############################################################################
# FUNCTIONS
###############################################################################

proc prepareRandomData {args} {
    global extractDir

    set size 50

    file mkdir $extractDir/data
    foreach type $args {
        exec dd if=/dev/$type of=$extractDir/data/$type bs=1M count=$size > /dev/null 2> /dev/null
    }
}

proc cleanRandomData {} {
    global extractDir

    file delete -force $extractDir/data
}

proc prepareArchive {dir file} {
    global zip archive

    set w [ pwd ]
    cd $dir
    exec $zip -r $archive $file
    cd $w
}

proc prepareData {action} {
    puts "Generating data... "
    puts -nonewline "Generating data... "
    switch -exact $action {
        zip-urandom {
            prepareRandomData urandom
        }
        zip-zero {
            prepareRandomData zero
        }
        zip-mixed {
            prepareRandomData urandom zero
        }
        unzip-urandom {
            prepareRandomData urandom
            prepareArchive $::extractDir data
            cleanRandomData
        }
        unzip-zero {
            prepareRandomData zero
            prepareArchive $::extractDir data
            cleanRandomData
        }
        unzip-mixed {
            prepareRandomData urandom zero
            prepareArchive $::extractDir data
            cleanRandomData
        }
        zip-linuxsrc {
            exec cp -r -t $::extractDir $::linuxSources
        }
        unzip-linuxsrc {
            prepareArchive $::linuxSources .
        }
    }
    puts "OK"
}

proc cleanData {action} {
    global archive extractDir

    switch -exact $action {
        zip-urandom -
        zip-zero -
        zip-mixed {
            cleanRandomData
        }
    }
}

proc cleanTestData {action} {
    global archive extractDir

    switch -glob $action {
        zip-* {
            file delete $archive
        }
        unzip-* {
            file delete -force $extractDir
            file mkdir $extractDir
        }
    }
}

proc checkArchive {} {
    global archive unzip

    if [ catch {
        exec $unzip -t $archive
    } err ] {
        error "Archive $archive is corrupted! $err"
    }
}

proc timeExec {args} {
    global timeCmd
    return [ eval [ concat exec $timeCmd $args ] ]
}

###############################################################################
# TESTS
###############################################################################

proc fusezipExec {cmd} {
    global fuseZip archive mountPoint fusermount
    set res [ timeExec sh -c "\
        $fuseZip $archive $mountPoint;\
        $cmd;\
        $fusermount -uz $mountPoint;\
    " ]
    checkArchive
    return $res
}

proc fuse-zip {action} {
    global mountPoint extractDir linuxSources

    switch -glob $action {
        zip-* {
            return [ fusezipExec "cp -r -t $mountPoint $extractDir" ]
        }
        unzip-* {
            return [ fusezipExec "cp -r $mountPoint/* $extractDir" ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc kio-zip {action} {
    global archive extractDir kioCopy

    switch -glob $action {
        zip-* {
            return "n/a n/a n/a"
        }
        unzip-* {
            return [ timeExec $kioCopy zip://$archive file://$extractDir/content ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc mc-uzip {action} {
    global zip uzip archive extractDir

    switch -glob $action {
        zip-* {
            # mc uzip cannot handle empty files
            exec $zip $archive $::argv0
            return [ timeExec sh -c "\
                find $extractDir -type d | cut -b 2- | tail -n +2 | while read f;\
                do $uzip mkdir $archive \$f; done;\
                find $extractDir -type d | cut -b 2- | while read f;\
                do $uzip mkdir $archive \$f /\$f; done;\
            " ]
        }
        unzip-* {
            return [ timeExec sh -c "\
                $uzip list $archive | cut -b 64- | grep /\\\$ | while read f;\
                do mkdir -p $extractDir/\$f; done;\
                $uzip list $archive | cut -b 64- | grep -v /\\\$ | while read f;\
                do $uzip copyout $archive \$f $extractDir/\$f; done;\
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

file mkdir $tmpDir
file delete -force $tmpDir
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
            cleanTestData $t
        }
        cleanData $t
        lappend results $t $l $res
    }
} err ] {
    puts "FAILED"
    puts "Error message: $err"

    catch {exec $fusermount -uz $mountPoint}
exit

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

