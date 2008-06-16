#!/bin/sh

############################################################################
#    Copyright (C) 2008 by Alexander Galanin                               #
#    gaa.nnov@mail.ru                                                      #
#                                                                          #
#    This program is free software; you can redistribute it and/or modify  #
#    it under the terms of the GNU Library General Public License as       #
#    published by the Free Software Foundation; either version 3 of the    #
#    License, or (at your option) any later version.                       #
#                                                                          #
#    This program is distributed in the hope that it will be useful,       #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#    GNU General Public License for more details.                          #
#                                                                          #
#    You should have received a copy of the GNU Library General Public     #
#    License along with this program; if not, write to the                 #
#    Free Software Foundation, Inc.,                                       #
#    51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               #
############################################################################

#restart using tclsh \
exec tclsh "$0" "$@"

package require Tcl 8.4

# source files
set linuxSources [ file normalize "linux-sources" ]

# utilities
set fuseZip "../fuse-zip"
set fusermount "fusermount"
set kioCopy "kio_copy/kio_copy"
set uzip "/usr/share/mc/extfs/uzip"
set gvfsdArchive "/usr/lib/gvfs/gvfsd-archive"
set unpackfsBinary "unpackfs-0.0.6/src/unpackfs"
set avfsBinary "avfs-0.9.8/fuse/avfsd"
set fuseJClasspath "fuse-j-2.4-prerelease1"
set jdkHome "/usr/lib/jvm/java-6-sun-1.6.0.06/"

set zip "zip"
set unzip "unzip"

# print real user system, in seconds
set timeCmd "/usr/bin/time -f \"%e\t%U\t%S\" -o /dev/stdout"

# paths
set tmpDir [ file normalize "fusezip-tests-temp" ]
set mountPoint "$tmpDir/mountpoint-subdir/mount-point"
set extractDir "$tmpDir/extract-dir"
set archive "$tmpDir/archive.zip"

set participants {
    fuse-zip
    kio-zip
    kio-krarc
    mc-uzip
    gvfs-fuse
    unpackfs
    avfs-fuse
    fusej-zip
}

set tests {
    zip-zero        "Compress files generated from /dev/zero"
    unzip-zero      "Uncompress files generated from /dev/zero"
    zip-urandom     "Compress files generated from /dev/urandom"
    unzip-urandom   "Uncompress files generated from /dev/urandom"
    zip-mixed       "Compress files generated from /dev/{urandom,zero}"
    unzip-mixed     "Uncompress files generated from /dev/{urandom,zero}"

    extract-one-1   "Extract one file from archive with many files"
    extract-one-2   "Extract one file from big archive"

    add-small-small "Add small file to small archive"
    add-small-big   "Add small file to big archive"
    add-big-small   "Add big file to small archive"
    add-big-big     "Add big file to big archive"

    zip-linuxsrc    "Compress (a part of) linux kernel sources"
    unzip-linuxsrc  "Uncompress (a part of) linux kernel sources"
}

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
    puts -nonewline "Generating data for $action... "
    flush stdout
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
            exec cp -r -t $::extractDir $::linuxSources/drivers
        }
        unzip-linuxsrc {
            # Compressing not full sources because mc-uzip is too sloooow :)
            prepareArchive $::linuxSources drivers
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
        extract-one-1 {
            file mkdir $::extractDir/data
            for {set i 0} {$i < 100} {incr i} {
                makeFile data/$i small
            }
            makeFile data/file small
            prepareArchive $::extractDir data
            file delete -force $::extractDir/data
        }
        extract-one-2 {
            file mkdir $::extractDir/data
            for {set i 0} {$i < 3} {incr i} {
                makeFile data/$i big
            }
            makeFile data/file small
            prepareArchive $::extractDir data
            file delete -force $::extractDir/data
        }
    }
    puts "OK"
}

proc cleanData {action} {
    global archive extractDir

    switch -glob $action {
        extract-one-* -
        zip-* {
            file delete $archive
            exec chmod -R a+wx $extractDir
            file delete -force $extractDir
            file mkdir $extractDir
        }
        add-* {
            file delete $::archive.copy
            exec chmod -R a+wx $extractDir
            file delete -force $extractDir
            file mkdir $extractDir
        }
    }
}

proc cleanTestData {action} {
    global archive extractDir mountPoint

    switch -glob $action {
        zip-* {
            file delete $archive
            file delete -force $mountPoint
            file mkdir $mountPoint
        }
        extract-one-* -
        unzip-* {
            exec chmod -R a+wx $extractDir
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

proc fuseExec {mountCmd binary args} {
    global mountPoint fusermount
    set res [ timeExec sh -c "
        $mountCmd;
        $args;
        $fusermount -uz $mountPoint;
        while \[ -n `pgrep $binary`\"\" \];
        do
            sleep 0.1s;
        done;
    " ]
    checkArchive
    return $res
}

proc fuseZipExec {args} {
    global fuseZip archive mountPoint
    return [ eval [ concat [ list fuseExec "$fuseZip $archive $mountPoint" "fuse-zip" ] $args ] ]
}

proc fuse-zip {action} {
    global mountPoint extractDir linuxSources

    switch -glob $action {
        add-* -
        zip-* {
            return [ fuseZipExec cp -r -t $mountPoint $extractDir ]
        }
        unzip-* {
            return [ fuseZipExec cp -r $mountPoint/* $extractDir ]
        }
        extract-one-* {
            return [ fuseZipExec cp -r $mountPoint/data/file $extractDir ]
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
        extract-one-* {
            return [ timeExec $kioCopy zip://$archive/data/file file://$extractDir/content ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc kio-krarc {action} {
    global archive extractDir kioCopy zip

    switch -glob $action {
        add-* -
        zip-* {
            # krarc:// cannot handle empty files
            exec $zip $archive $::argv0
            return [ timeExec $kioCopy file://$extractDir krarc://$archive/data ]
        }
        unzip-linuxsrc {
            return [ timeExec $kioCopy krarc://$archive/drivers file://$extractDir/content ]
        }
        unzip-* {
            return [ timeExec $kioCopy krarc://$archive/data file://$extractDir/content ]
        }
        extract-one-* {
            return [ timeExec $kioCopy krarc://$archive/data/file file://$extractDir/content ]
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
            return [ timeExec sh -c "
                find $extractDir -type d | cut -b 2- | tail -n +2 | while read f;
                do $uzip mkdir $archive \$f; done;
                find $extractDir -type f | cut -b 2- | while read f;
                do $uzip copyin $archive \$f /\$f; done;
            " ]
        }
        unzip-* {
            return [ timeExec sh -c "
                $uzip list $archive | cut -b 64- | grep /\\\$ | while read f;
                do mkdir -p $extractDir/\$f; done;
                $uzip list $archive | cut -b 64- | grep -v /\\\$ | while read f;
                do $uzip copyout $archive \$f $extractDir/\$f; done;
            " ]
        }
        extract-one-* {
            return [ timeExec $uzip copyout $archive data/file $extractDir/file ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc gvfsExec {args} {
    global gvfsdArchive archive

    return [ timeExec sh -c "$gvfsdArchive file=$archive 2> /dev/null |
        (
            awk \"/register_mount_callback/ {
                system(\\\"$args\\\");
                system(\\\"gvfs-mount -u `gvfs-mount -l | grep [ file tail $archive ] | cut -d ' ' -f 4`\\\");
                exit;
            }\";
            cat > /dev/null
        )" \
    ]
}

proc gvfs-fuse {action} {
    global archive extractDir

    switch -glob $action {
        add-* -
        zip-* {
            return "n/a n/a n/a"
        }
        unzip-* {
            return [ gvfsExec cp -r -t $extractDir "$::env(HOME)/.gvfs/[ file tail $archive ]/*"  ]
        }
        extract-one-* {
            return [ gvfsExec cp -r -t $extractDir "$::env(HOME)/.gvfs/[ file tail $archive ]/data/file"  ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc unpackFsExec {args} {
    global unpackfsBinary mountPoint
    return [ eval [ concat [ list fuseExec "$unpackfsBinary -c unpackfs.config $mountPoint" "unpackfs" ] $args ] ]
}

proc unpackfs {action} {
    global extractDir mountPoint archive

    set dir $mountPoint/[ file rootname $archive ]
    switch -glob $action {
        add-* -
        zip-* {
            return "n/a n/a n/a"
        }
        unzip-* {
            return [ unpackFsExec cp -r $dir/* $extractDir ]
        }
        extract-one-* {
            return [ unpackFsExec cp -r $dir/data/file $extractDir ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}

proc avfsExec {args} {
    global avfsBinary mountPoint
    return [ eval [ concat [ list fuseExec "$avfsBinary $mountPoint" "avfsd" ] $args ] ]
}

proc avfs-fuse {action} {
    global extractDir mountPoint archive

    set dir $mountPoint/$archive#
    switch -glob $action {
        add-* -
        zip-* {
            return "n/a n/a n/a"
        }
        unzip-* {
            return [ avfsExec cp -r $dir/* $extractDir ]
        }
        extract-one-* {
            return [ avfsExec cp -r $dir/data/file $extractDir ]
        }
        default {
            error "Action $action not implemented"
        }
    }
}


proc fusejExec {args} {
    global archive mountPoint fuseJClasspath jdkHome

    set fuseJMount "LD_LIBRARY_PATH=$fuseJClasspath/jni:/usr/lib $jdkHome/bin/java -classpath $fuseJClasspath/build:$fuseJClasspath/lib/commons-logging-1.0.4.jar -Dorg.apache.commons.logging.Log=fuse.logging.FuseLog fuse.zipfs.ZipFilesystem -s -f $mountPoint $archive 2>&1"
    return [ timeExec sh -c "$fuseJMount |
        (
            awk \"/.*Mounting filesystem.*/ {
                system(\\\"sleep 0.01\\\");
                system(\\\"$args\\\");
                system(\\\"fusermount -uz $mountPoint\\\");
                exit;
            }\";
            cat > /dev/null
        )" \
    ]
}

proc fusej-zip {action} {
    global extractDir mountPoint archive

    switch -glob $action {
        add-* -
        zip-* {
            return "n/a n/a n/a"
        }
        unzip-* {
            return [ fusejExec cp -r $mountPoint/* $extractDir ]
        }
        extract-one-* {
            return [ fusejExec cp -r $mountPoint/data/file $extractDir ]
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
exec chmod -R a+wx $tmpDir
file delete -force $tmpDir
file mkdir $tmpDir $mountPoint $extractDir

set results ""

if [ catch {
    foreach {t l} $tests {
        set res ""
        prepareData $t
        foreach p $participants {
            puts -nonewline "Running test $t for $p... "
            flush stdout

            if [ catch {
                lappend res $p [ $p $t ]
            } err ] {
                puts "FAILED"
                puts "Error: $err"
                lappend res $p "FAIL FAIL FAIL"
            } else {
                puts "OK"
            }
            cleanTestData $t
        }
        cleanData $t
        exec sync
        lappend results $t $l $res
    }
} err ] {
    puts "FAILED"
    puts "Error message: $err"
    puts "Error info: $errorInfo"

    catch {exec $fusermount -uz $mountPoint}

    exec chmod -R a+wx $tmpDir
    file delete -force $tmpDir
    exit
}

exec chmod -R a+wx $tmpDir
file delete -force $tmpDir

file mkdir logs

puts ""

set f [ open logs/[ clock format [ clock seconds ] ].log "w" ]
foreach fd "stdout $f" {
    puts $fd "|| Program || real || user || system ||"
    foreach {test label res} $results {
        puts $fd "== $label \($test\) =="
            foreach {p t} $res {
            puts $fd [ join [ list "|| $p\t||" [ join $t "\t|| " ] " ||" ] ]
        }
        puts $fd ""
    }
}
close $f
