#! /bin/sh
# ############################################################################
# K42: (C) Copyright IBM Corp. 2000, 2003.
# All Rights Reserved
#
# This file is distributed under the GNU LGPL. You should have
# received a copy of the License along with K42; see the file LICENSE.html
# in the top-level directory for more details.
#
#  $Id: fileutils-test.sh,v 1.20 2004/03/01 16:01:51 dilma Exp $
# ############################################################################

cleanup_for_test() {
    rm -f chgrp.test chmod.test chown.test cp.test cp.test.copy dd.test
    rm -rf ln.test ln.test.symlink ln.test.hardlink mkdir.test rm.test
    rm -rf touch.epoch touch.test test.mv test.mv.moved mv.dir mv.dir.moved
}

chgrp_test() {
    # This is very hard to test completely so we let it run
    # it also tests /bin/ls
    echo "chgrp test" > chgrp.test
    /bin/ls -ln chgrp.test
    echo -e "\nfileutils: changing to gid 9121\n"
    /bin/chgrp 9121 chgrp.test
    /bin/ls -ln chgrp.test
    rm -f chgrp.test
}

chmod_test() {
    local ret
    echo "chmod test" > chmod.test
    /bin/chmod +x chmod.test
    if [ -x chmod.test ]; then
	echo -e '\nfileutils: chmod(1) test succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: chmod(1) test failed\n'
	ret=1
    fi
    rm -f chmod.test
    return $ret
}

chown_test() {
    # This is very hard to test completely so we let it run
    # it also tests /bin/ls
    echo "chown test" > chown.test
    /bin/ls -ln chown.test
    echo -e "\nfileutils: changing to uid 0\n"
    /bin/chown 0 chown.test
    /bin/ls -ln chown.test
    rm -f chown.test
}

cp_test() {
    local ret
    echo "cp test" > cp.test
    /bin/cp cp.test cp.test.copy
    if [ -f cp.test.copy ]; then
	echo -e '\nfileutils: cp(1) test succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: cp(1) test failed\n'
	ret=1
    fi
    #rm -f cp.test cp.test.copy
    return $ret
}

dd_test() {
    echo -e "\nfileutils: you should see the alphabet in lower case.\n"
    echo -e "\nABCEFGHIJKLMNOPQRSTUVWXYZ" > dd.test
    /bin/dd if=dd.test conv=lcase
    rm -f dd.test
}

df_test() {
    echo -e '\nfileutils: df(1) does not work yet.. testing anyway\n'
    /bin/df -k .
}

ln_test() {
    local sret
    local hret
    echo -e '\nfileutils: testing ln(1)\n'
    echo "ln test" > ln.test
    /bin/ln -s ln.test ln.test.symlink
    /bin/ln ln.test ln.test.hardlink
    if [ -L ln.test.symlink ]; then
	echo -e '\nfileutils: ln(1) symlink test succeeded\n'
	sret=0
    else
	echo -e '\nfileutils: ln(1) symlink test failed\n'
	sret=1
    fi
    if [ -f ln.test.hardlink -a ln.test -ef ln.test.hardlink ]; then
	echo -e '\nfileutils: ln(1) hardlink test succeeded\n'
	hret=0
    else
	echo -e '\nfileutils: ln(1) hardlink test failed\n'
	hret=1
    fi

    rm -f ln.test ln.test.symlink ln.test.hardlink
    if [ $sret -eq "0" -a $hret -eq "0" ]; then
	return 0
    else
	return 1
    fi
}

ls_test() {
    echo -e '\nfileutils: expect to see output of ls -la\n'
    /bin/ls -la .
}

mkdir_test() {
    local ret
    /bin/mkdir mkdir.test
    if [ -d mkdir.test ]; then
	echo -e '\nfileutils: mkdir(1) test succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: mkdir(1) test failed\n'
	ret=1
    fi
    ## do not delete the directory we will remove it later
    return $ret
}

rm_test() {
    local ret
    echo -e '\nfileutils: testing rm(1)\n'
    echo "rm test" > rm.test
    /bin/rm -f rm.test
    if [ ! -f rm.test ]; then
	echo -e '\nfileutils: rm(1) test succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: rm(1) test failed\n'
	ret=1
    fi
    return $ret
}

rmdir_test() {
    local ret
    echo -e '\nfileutils: testing rmdir(1)\n'
    # remove directory that mkdir test created
    if [ ! -d mkdir.test ]; then
	echo -e '\nfileutils: rmdir(1): no directory to remove\n'
	return 1;
    fi
    
    /bin/rmdir mkdir.test
    if [ ! -d mkdir.test ]; then
	echo -e '\nfileutils: rmdir(1) test succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: rmdir(1) test failed\n'
	ret=1
    fi
    return $ret
}

sync_test() {
    echo -e '\nfileutils: Testing sync(1)\n'    
    /bin/sync
}

touch_test() {
    local ret
    local tret

    test \! -f touch.epoch && /bin/touch touch.epoch
    test \! -f touch.test && /bin/touch touch.test

    if [ -f touch.epoch ]; then
	echo -e '\nfileutils: touch(1) to create succeeded\n'
	ret=0
    fi

    # touch.test has time of creation; if NFS is being used this
    # time may be very inconsistent with k42's time. So we touch
    #it again, and it will have a k42 time
    /bin/touch touch.test

    # The following line is a hack to get the K42 time to advance by
    # at least 1 second. Ideally, "sleep 1" will be used when signal
    # facilities are available.
    for i in 1 2 3; do
	cat /tests/linux/regress.sh > /dev/null
    done

    /bin/touch touch.epoch
    if [ touch.test -ot touch.epoch ]; then
	echo -e '\nfileutils: touch(1) without timestamp succeeded\n'
	ret=0
    else
	echo -e '\nfileutils: touch(1) without timestamp failed\n'
	ret=1
    fi
    /bin/touch 0101000070 touch.epoch
    if [ touch.test -nt touch.epoch ]; then
	echo -e '\nfileutils: touch(1) with timestamp succeeded\n'
	tret=0
    else
	echo -e '\nfileutils: touch(1) with timestamp failed\n'
	tret=1
    fi

    rm -f touch.test touch.epoch

    if [ $ret -eq "0" -a $tret -eq "0" ]; then
	return 0
    else
	return 1
    fi
}

mv_test() {
    echo -e '\nfileutils: testing mv(1)\n'
    echo "test.mv" > test.mv
    mv test.mv test.mv.moved

    if [ -f test.mv ]; then
	echo -e  \
            '\nfileutils: mv(1) file to file failed(skipping other mv tests)\n'
	return 1
    fi

    if [ -f test.mv.moved ]; then
	echo -e '\nfileutils: mv(1) file to file succeeded\n'
    else
	echo -e \
	   '\nfileutils: mv(1) file to file failed(skipping other mv tests)\n'
	return 1
    fi

    mkdir mv.dir
    mv mv.dir mv.dir.moved

    if [ -d mv.dir ]; then
	echo -e '\nfileutils: mv(1) dir to dir failed\n'
	return 1
    fi

    if [ -d mv.dir.moved ]; then
	echo -e '\nfileutils: mv(1) dir to dir succeeded\n'
    fi

    rm -rf test.mv.moved mv.dir.moved
    return 1
}

fileutils_testall() {
    echo -e "\nCleaning up for testing the fileutils package.\n"
    cleanup_for_test
    echo -e '\nNow testing the fileutils package.\n'
    echo -e "starting chgrp_test\n"
    chgrp_test
    echo -e "starting chmod_test\n"
    chmod_test
    echo -e "starting chown_test\n"
    chown_test
    echo -e "starting cp_test\n"
    cp_test
    echo -e "starting dd_test\n"
    dd_test
    echo -e "starting df_test\n"
    df_test
    echo -e "starting ln_test\n"
    ln_test
    echo -e "starting ls_test\n"
    ls_test
    echo -e "starting mkdir_test\n"
    mkdir_test
    echo -e "starting rm_test\n"
    rm_test
    echo -e "starting rmdir_test\n"
    rmdir_test
    echo -e "starting sync_test\n"
    sync_test
    echo -e "starting touch_test\n"
    touch_test
    echo -e "starting mv_test\n"
    mv_test
    echo -e "\nFinal clean up\n"
    cleanup_for_test
 }
