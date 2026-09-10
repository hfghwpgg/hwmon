# Hwmon

Tool for monitoring temperature, power draw and more in real time.

> currently WIP

for compilation instruction, visit [server](./server), .[client](./client) folders respectively

## Supported metrics:
Cpu: 
- name
- temperature
- power draw
- utilization
- core frequency

Gpu:
- name
- temperature *
- power draw
- utilization
- core/memory frequency
> \* temperature may not be available on intel gpus

Other devices:
 - whatever is reported by sysfs hwmon interface

## Special thanks:
 - [btop](https://github.com/aristocratos/btop/) project
    - fully skided gpu reading

- [Hwmon-python](https://github.com/guicalare/Hwmon-python)
    - some sysfs paths
