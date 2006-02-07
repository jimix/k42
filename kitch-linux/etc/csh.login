# /etc/csh.login

# System wide environment and startup programs for csh users

if ($?PATH) then
	setenv PATH "${PATH}:/usr/X11R6/bin"
else
	setenv PATH "/bin:/usr/bin:/usr/local/bin:/usr/X11R6/bin"
endif

limit coredumpsize unlimited

[ `id -gn` = `id -un` -a `id -u` -gt 14 ]
if $status then
	umask 022
else
	umask 002
endif

setenv HOSTNAME `/bin/hostname`
set history=1000

test -f $HOME/.inputrc
if ($status != 0) then
	setenv INPUTRC /etc/inputrc
endif

test -d /etc/profile.d
if ($status == 0) then
	set nonomatch
        foreach i ( /etc/profile.d/*.csh )
		test -f $i
		if ($status == 0) then
               		source $i
		endif
        end
	unset i nonomatch
endif
