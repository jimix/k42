# /etc/cshrc
#
# csh configuration for all shell invocations. Currently, a prompt.

if ($?prompt) then
  if ($?tcsh) then
    set prompt='[%n@%m %c]$ ' 
  else
    set prompt=\[`id -nu`@`hostname -s`\]\$\ 
  endif
endif
