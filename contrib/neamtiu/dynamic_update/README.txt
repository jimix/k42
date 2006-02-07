Safe and Easy Software Hot-Swapping in the K42 Operating System
----------------------------------------------------------------

This is a collection of programs intended to make hot swapping in K42
easier and safer.

Making Hot Swapping Easy
------------------------

There are 4 steps involved in creating a dynamic patch: 

1. Detect source code changes using diff, or syntax tree comparison

patch_generation/scripts/find_modified_files.pl compares two
directories (or current version against the one in the repository) 
and produces a list of candidate files that have been changed.

2. Generate dynamic patch source code: new code/data plus glue code,
   e.g. state transfer.

patch_generation/scripts/generate_next_version.pl generates a 
skeleton for objects containing factories, with an updated version
number and auto-generated standard functions.

patch_generation/code/gen_dto.cc generates a standard data transfer
object for a certain class.

3. Compile dynamic patch source code into a .so (loadable module).

4. Load the module into running system (using the module loader).


Making Hot Swapping Safe
------------------------
When doing hot-swapping, during the 'Blocking' phase incoming
threads not in the hash table are, well, blocked. So there's potential
for deadlock if clustered objects call themselves recursively, in
other words - there's a cycle in the method call graph.
There are two possible solutions for this:

 Static analysis - Check for potential deadlocks by finding cycle in
 the method call graph.
analysis/cycle_detect.cc looks for cycles in the method call graph. 

 Dynamic analysis - deadlock detection by checking for cycles in the
 resource allocation graph.

 usr/deadlock.C and analysis/small_deadlock.cc are examples of such
dynamic deadlock detection.

Iulian Neamtiu, neamtiu@cs.umd.edu