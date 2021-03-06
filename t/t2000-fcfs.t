#!/bin/bash
#set -x

test_description='Test fcfs scheduler in simulator
'

#
# variables
#
dn=`dirname $0`
tdir=`readlink -e $dn/../`

sched=`readlink -e $dn/../sched/schedsrv.so`
rdlconf=`readlink -e $dn/../conf/hype-io.lua`

submit=`readlink -e $dn/../simulator/submitsrv.so`
jobdata=`readlink -e $dn/data/hype-test.csv`

sim_exec=`readlink -e $dn/../simulator/sim_execsrv.so`
sim=`readlink -e $dn/../simulator/simsrv.so`

waitjob=`readlink -e $dn/../sched/flux-waitjob`

expected_order=`readlink -e $dn/fcfs_expected`

#
# source sharness from the directore where this test
# file resides
#
. ${dn}/sharness.sh

#
# print only with --debug
#
test_debug '
	echo ${tdir} &&
	echo ${sched} &&
	echo ${rdlconf} &&
    echo ${submit} &&
    echo ${jobdata} &&
    echo ${sim_exec} &&
    echo ${sim}
'

#
# test_under_flux is under sharness.d/
#
test_under_flux 1 $tdir

test_expect_success 'loading sim works' '
	flux module load ${sim} exit-on-complete=false
'
test_expect_success 'loading submit works' '
	flux module load ${submit} job-csv=${jobdata}
'
test_expect_success 'loading exec works' '
	flux module load ${sim_exec}
'
test_expect_success 'loading sched works' '
	flux module load ${sched} rdl-conf=${rdlconf} in-sim=true plugin=sched.plugin1
'

while flux kvs get lwj.12.complete_time 2>&1 | grep -q "No such file"; do sleep 0.5; done
sleep 0.5
for x in $(seq 1 12); do echo "$x $(flux kvs get lwj.$x.starting_time)"; done | sort -k 2n -k 1n | cut -d ' ' -f 1 > actual

test_expect_success 'jobs scheduled in correct order' '
    diff -u ${expected_order} ./actual
'

test_done
