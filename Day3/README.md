# Day 3

## Info - Resetting eMMC debian password
Step 1: Boot your Beaglebone Board from your SDCard

Step 2: From the Beaglebone linux prompt
```
lsblk
```
Step 3: Mount your emmc partition
```
sudo mkdir -p /mnt/emmc
sudo mount /dev/mmcblk1p1 /mnt/emmc
ls /mnt/emmc
```

Step 4: Change the password, by making eMMC's filesystem as your temporary root
```
sudo chroot /mnt/emmc /bin/bash
passwd eagle
exit
```

Step 5: Unmount cleanly and reboot to eMMC
```
sudo umount /mnt/emmc
sudo poweroff
```

## Lab - Hello World module
hello.c
```
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
```

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

## Lab - Let's develop a LED character driver

On your beaglebone board, create a folder for your led-driver and paste the code below in led_driver.c
```
// led_cdev.c - character driver that toggles the LED on P9_12 (Trixie/BBB).
// echo 1 > /dev/tektutor_led  -> LED on ;  echo 0 -> LED off ;  cat -> state.
// P9_12 = gpiochip0 line 28 = global GPIO 28 on THIS board (verified).
// Confirm on any board with:  gpioinfo -c gpiochip0 | grep P9_12
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/version.h>

#define NAME     "tektutor_led"

// P9_12 = gpiochip0(offset- 512) line 28, so we need to add 512 + 28 = 540
#define LED_GPIO 540 

static dev_t         devno;
static struct cdev   my_cdev;
static struct class *my_class;
static int           led_state;

static int my_open(struct inode *i, struct file *f)    { return 0; }
static int my_release(struct inode *i, struct file *f) { return 0; }

static ssize_t my_read(struct file *f, char __user *buf, size_t count, loff_t *pos)
{
    char tmp[3];
    int len;
    if (*pos > 0) return 0;
    len = scnprintf(tmp, sizeof(tmp), "%d\n", led_state);
    if (count < (size_t)len) return -EINVAL;
    if (copy_to_user(buf, tmp, len)) return -EFAULT;
    *pos += len;
    return len;
}

static ssize_t my_write(struct file *f, const char __user *buf, size_t count, loff_t *pos)
{
    char c;
    if (count < 1) return -EINVAL;
    if (copy_from_user(&c, buf, 1)) return -EFAULT;
    if (c == '1')      { gpio_set_value(LED_GPIO, 1); led_state = 1; }
    else if (c == '0') { gpio_set_value(LED_GPIO, 0); led_state = 0; }
    else return -EINVAL;
    return count;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE, .open = my_open, .release = my_release,
    .read = my_read, .write = my_write,
};

static int __init my_init(void)
{
    int ret;

    ret = gpio_request(LED_GPIO, NAME);
    if (ret) { pr_err("%s: gpio_request(%d) failed: %d\n", NAME, LED_GPIO, ret); return ret; }
    gpio_direction_output(LED_GPIO, 0);
    led_state = 0;

    ret = alloc_chrdev_region(&devno, 0, 1, NAME);
    if (ret) goto free_gpio;
    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, devno, 1);
    if (ret) goto unregister_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
    my_class = class_create(NAME);
#else
    my_class = class_create(THIS_MODULE, NAME);
#endif
    if (IS_ERR(my_class)) { ret = PTR_ERR(my_class); goto del_cdev; }
    device_create(my_class, NULL, devno, NULL, NAME);   // /dev/tektutor_led

    pr_info("%s: ready on gpio %d\n", NAME, LED_GPIO);
    return 0;

del_cdev:            cdev_del(&my_cdev);
unregister_region:   unregister_chrdev_region(devno, 1);
free_gpio:           gpio_free(LED_GPIO);
    return ret;
}

static void __exit my_exit(void)
{
    device_destroy(my_class, devno);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(devno, 1);
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
    pr_info("%s: removed\n", NAME);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("TekTutor");
MODULE_DESCRIPTION("Character driver to toggle the LED on P9_12 (BBB)");
```

Create a Makefile
```
obj-m += led_cdev.o
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

Build and run
```
make
sudo dmesg -C
sudo insmod led_driver.ko
dmesg | tail -2                          # "ready on gpio 28"
ls -l /dev/tektutor_led
echo 1 | sudo tee /dev/tektutor_led      # LED ON
sudo cat /dev/tektutor_led               # prints 1
echo 0 | sudo tee /dev/tektutor_led      # LED OFF
sudo rmmod led_driver

# check turns ON LED
sudo -i
echo 1 > /dev/tektutor_led 

# check turns OFF LED
echo 0 > /dev/tektutor_led

exit
```

