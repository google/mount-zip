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

    add-small-small "Add small file to small archive"
    add-small-big   "Add small file to big archive"
    add-big-small   "Add big file to small archive"
    add-big-big     "Add big file to big archive"

    zip-linuxsrc    "Compress linux kernel sources"
    unzip-linuxsrc  "Uncompress linux kernel sources"
}

##debug
#set tests {
#}

###############################################################################
# FUNCTIONS
###############################################################################

proc prepareRandomData {args} {
    global extractDir

    set size 2
    set count 10

    file mkdir $extractDir/data
    foreach type $args {
        for {set i 0} {$i < $count} {incr i} {
            exec dd if=/dev/$type of=$extractDir/data/$type$i bs=1M count=$size > /dev/null 2> /dev/null
        }
    }
}

proc prepareArchive {dir file} {
    global zip archive

    set w [ pwd ]
    cd $dir
    exec $zip -r $archive $file
    cd $w
}

proc makeFile {name size} {
    global extractDir
    array set sizes {
        small   1
        big     20
    }
    exec dd if=/dev/zero of=$extractDir/$name bs=1M count=$sizes($size) > /dev/null 2> /dev/null
}

proc prepareData {action} {
    puts "Generating data... "
    puts -nonewline "Generating data... "
    switch -glob $action {
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
            file delete -force $::extractDir/data
        }
        unzip-zero {
            prepareRandomData zero
            prepareArchive $::extractDir data
            file delete -force $::extractDir/data
        }
        unzip-mixed {
            prepareRandomData urandom zero
            prepareArchive $::extractDir data
            file delete -force $::extractDir/data
        }
        zip-linuxsrc {
            exec cp -r -t $::extractDir $::linuxSources
        }
        unzip-linuxsrc {
            prepareArchive $::linuxSources .
        }
        add-* {
            set a [ lrange [ split $action "-" ] 1 2 ]
            file mkdir $::extractDir/data
            makeFile data/content [ lindex $a 0 ]
            makeFile file [ lindex $a 1 ]
            prepareArchive $::extractDir data
            file delete -force $::extractDir/data
            exec cp $::archive $::archive.copy
        }
    }
    puts "OK"
}

proc cleanData {action} {
    global archive extractDir

    switch -glob $action {
        zip-* {
            file delete -force $extractDir
            file mkdir $extractDir
        }
        add-* {
            file delete $::archive.copy
            file delete -force $extractDir
            file mkdir $extractDir
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
        add-* {
            exec cp $::archive.copy $::archive
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
        while \[ -n `pgrep fuse-zip`\"\" \];do sleep 0.1s;done;\
    " ]
    checkArchive
    return $res
}

proc fuse-zip {action} {
    global mountPoint extractDir linuxSources

    switch -glob $action {
        add-* -
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
        add-* -
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
        add-* -
        zip-* {
            # mc uzip cannot handle empty files
            exec $zip $archive $::argv0
            return [ timeExec sh -c "\
                find $extractDir -type d | cut -b 2- | tail -n +2 | while read f;\
                do $uzip mkdir $archive \$f; done;\
                find $extractDir -type f | cut -b 2- | while read f;\
                do $uzip copyin $archive \$f /\$f; done;\
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
        exec sync
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

