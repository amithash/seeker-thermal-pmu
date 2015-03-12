This is a full system Performance and Thermal profiling tool mainly for the Intel Core Micro-architecture but can be easily extended to other architectures.
(Basic support for the AMD Opteron is also provided - Minus the temperature and PMU interrupt support)

# Abstract #

---


There is an abundance of software out there to access the hardware performance counters. Namely, perfctr+PAPI, perfmon2, Vtune(Intel Trademark), Oprofile etc. Very few of them provide sampling features and even few can do full system profiling. But what makes seeker different is that currently one can profile the core temperature data along with the performance counter data and easily extend it to profile other parameters. Seeker comes with an interface document which allows one to write basic modules to manipulate hardware performance counters and hence porting Seeker to other architectures very easy. It currently provides minimal support for the k8 a.k.a. AMD Opteron architectures. This is provided as an example to prove the previous statement.

Seeker is licensed under the GPLv3. I am just a graduate student who likes to hacks the kernel as a hobby. So, this is a summing up of the license: "The piece of software is provided as is without warranty, feel free to use/modify/distribute it. But do not blame me if something goes wrong. If you distribute it, make sure that the copyright is included and GPL still holds recursively". But as a side note, Email me if you find bugs, I will try hard to fix them, infact send me an email anyway so I know that someone other than me uses this piece of garbled kernel code.

# Features #

---


1. Timer based profiling

2. Support for multi-processor architectures. (Seeker supports the system as long as linux does -> smp is enabled)

3. Support for PMU Counter based sampling for the x86 architectures - Patches for linux kernel 2.6.18.x and 2.6.24.x are provided.

4. Temperature profiling

5. Can be easily extended to add more devices to sample

6. Log acquisition daemon can be signalled to change log files -> easy to automate

7. Generic data handling and automation scripts are provided.

8. For more subtle features download the package and refer to the README

NOTE: If all you care about is sampling hardware performance counters, then perfmon2 or vtune(Registered Trademark Intel) are better and more robust/stable.

# Basic Usage #

---


make

./load sample\_freq=F [log\_events=X,Y log\_ev\_masks=MX,XY os\_flag=O pmu\_intr=P]

sample\_freq : Take F samples per second, without pmu\_intr

sample\_freq : Take a sample every time Fixed counter P counts F events.

pmu\_intr : If provided, samples are taken every time counter P counts F events.

os\_flag : 0(Default) - Count events in user space only. 1 - Count events in both user and
OS space.

log\_events : A list of events to monitor. For the core architecture, a max of 2 can be provided. 

log\_ev\_masks : The mask corrosponding to each of the events provided in log\_events. 

**For events and their corresponding masks, refer to C2D\_EVENTS.pdf (Core micro architecture) or K8\_EVENTS.pdf (K8 Architecture).**

Now to run the log acquisition daemon, Run the following:

'Scripts/generic\_log\_dump /dev/seeker\_samples /path/to/bin/log &'

This will dump the log in binary format in /path/to/bin/log0
If you feel that an experiment is over, and you need to conduct another one, do the following after waiting for 20 seconds:

'Scripts/send.pl'

This will cause generic\_log\_dump to shift logs from log0 to log1, then to log2 and so on.
And finally, when all the experiments are over:

'Scripts/send.pl -t'

This will cause generic\_log\_dump to terminate gracefully.

To convert the bin logs to csv logs, do the following:

Scripts/decodelog < /path/to/bin/log > /path/to/csv/log

Now, there are a ton of data handling scripts/applications which perform: Pulling per application data, Linear spline Interpolation, Moving Window averaging, Per sample max/min, csv to tsv conversion, And finally automation scripts to all of the above.