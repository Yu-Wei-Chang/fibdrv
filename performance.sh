#!/bin/sh

# ASLR
sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"

# Set scaling_governor
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor"
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor"
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor"

# Disable Intel turbo mode
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"

