# Day 3

## Lab - Hello World module
hello.c
<pre>
#include <linux/module.h>
#include <linux/kernel.h>
  
static int __init hello_init(void) {
  printk(KERN_INFO "Hello from the TekTutor module\n");
  return 0;
}
static void __exit hello_exit(void) {
  printk(KERN_INFO "Goodbye from the TekTutor module\n");
}
  
module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
</pre>

Makefile (the indented line must start with a real TAB)
<pre>
obj-m += hello.o
all:
$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
</pre>

Build and install
```
make
sudo insmod hello.ko
dmesg | tail -2
# your init message
lsmod | grep hello ; cat /proc/modules | grep hello
sudo rmmod hello ; dmesg | tail -2
```
