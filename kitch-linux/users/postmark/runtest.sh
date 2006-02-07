#!/bin/sh
###############################################################################
# Script to run PostMark benchmark.
# You must specify the number of cpus as the first parameter.
# Runs binary executable "postmark" with "input.txt" as the input file.
# Makes temporary directories testdirs/dir0 to testdirs/dir23 for use during
# experiment.
# Performance results are piped to resultsX.txt
# Standard errors are piped to stderrX.txt
###############################################################################

# Random number generator seed value (42 is the PostMark default)
seed=42

# Check usage
if [ $# != "1" ]
then
    echo "Usage: $0 <num_cpus>"
    exit
fi

# Make temporary directories
i=0
while [ $i -lt $1 ]
do
    mkdir -p testdirs/dir$i
    i=`expr $i + 1`
done

# Run $1 instances of the benchmark
i=0
while [ $i -lt $1 ]
do
    cd testdirs/dir$i
    uid=`expr 2400 + $i`
    # DON'T COMMIT
    if [ ! -x ../../postmark ] ; then
        echo "pwd is " pwd
	exit
    fi
    echo "set seed $seed" | cat - ../../input.txt | ../../runprog -u $uid ../../postmark > ../../results$i.txt 2> ../../stderr$i.txt &
    cd ../..
    seed=`expr $seed + 1`
    i=`expr $i + 1`
done

echo "All benchmark instances have been started( i-th in testdirs/dir<i>)."
echo "Waiting for all instances to complete."
wait
i=`expr $i - 1`


# Delete temporary directories
i=0
while [ $i -lt $1 ]
do
    rmdir testdirs/dir$i
    i=`expr $i + 1`
done
rmdir testdirs
