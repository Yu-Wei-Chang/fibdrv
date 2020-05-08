#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include "bignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;

static inline void addBigN(struct BigN *output, struct BigN x, struct BigN y)
{
    output->upper = x.upper + y.upper;
    if (y.lower > ~x.lower)
        output->upper++;
    output->lower = x.lower + y.lower;
}

static inline void multiBigN(struct BigN *output, struct BigN x, struct BigN y)
{
    size_t w = 8 * sizeof(unsigned long long);
    struct BigN product = {.upper = 0, .lower = 0};

    for (size_t i = 0; i < w; i++) {
        if ((y.lower >> i) & 0x1) {
            struct BigN tmp;

            product.upper += x.upper << i;

            tmp.lower = (x.lower << i);
            tmp.upper = i == 0 ? 0 : (x.lower >> (w - i));
            addBigN(&product, product, tmp);
        }
    }

    for (size_t i = 0; i < w; i++) {
        if ((y.upper >> i) & 0x1) {
            product.upper += (x.lower << i);
        }
    }
    output->upper = product.upper;
    output->lower = product.lower;
}

static inline void subBigN(struct BigN *output, struct BigN x, struct BigN y)
{
    if ((x.upper >= y.upper) && (x.lower >= y.lower)) {
        output->upper = x.upper - y.upper;
        output->lower = x.lower - y.lower;
    } else {
        output->upper = x.upper - y.upper - 1;
        output->lower = ULLONG_MAX + x.lower - y.lower + 1;
    }
}

static struct BigN fib_sequence_fd(long long k)
{
    /* The position of the highest bit of k. */
    /* So we need to loop `rounds` times to get the answer. */
    int rounds = 0;
    for (int i = k; i; ++rounds, i >>= 1)
        ;

    struct BigN t1 = {.upper = 0, .lower = 0}, t2 = {.upper = 0, .lower = 0};
    struct BigN a = {.upper = 0, .lower = 0}, b = {.upper = 0, .lower = 1};
    struct BigN multi_two = {.upper = 0, .lower = 2},
                tmp = {.upper = 0, .lower = 0};

    for (int i = rounds; i > 0; i--) {
        // t1 = a * (2 * b - a); /* F(2k) = F(k)[2F(k+1) − F(k)] */
        multiBigN(&t1, b, multi_two);
        subBigN(&t1, t1, a);
        multiBigN(&t1, a, t1);
        // t2 = b * b + a * a;   /* F(2k+1) = F(k+1)^2 + F(k)^2 */
        multiBigN(&t2, b, b);
        multiBigN(&tmp, a, a);
        addBigN(&t2, t2, tmp);

        if ((k >> (i - 1)) & 1) {
            a = t2; /* Threat F(2k+1) as F(k) next round. */
            addBigN(&b, t1,
                    t2); /* Threat F(2k) + F(2k+1) as F(k+1) next round. */
        } else {
            a = t1; /* Threat F(2k) as F(k) next round. */
            b = t2; /* Threat F(2k+1) as F(k+1) next round. */
        }
    }

    return a;
}

static struct BigN fib_sequence(long long k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    struct BigN f[k + 2];

    f[0].lower = 0; /* f[0] = 0; */
    f[0].upper = 0;

    f[1].lower = 1; /* f[1] = 1; */
    f[1].upper = 0;

    for (int i = 2; i <= k; i++) {
        addBigN(&f[i], f[i - 1], f[i - 2]);
    }

    // pr_warn("offset [%llu], upper [%llu], lower [%llu]\n", k, f[k].upper,
    //        f[k].lower);
    return f[k];
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    struct BigN fib_seq;
    long long tmp_time_ns = 0;

    if ((!buf) || (size < sizeof(fib_seq))) {
        return -EINVAL;
    }

    kt = ktime_get();
    fib_sequence_fd(*offset);
    kt = ktime_sub(ktime_get(), kt);
    tmp_time_ns = ktime_to_ns(kt);

    kt = ktime_get();
    fib_seq = fib_sequence(*offset);
    kt = ktime_sub(ktime_get(), kt);
    fib_seq.fib_cost_time_ns = ktime_to_ns(kt);
    fib_seq.fib_fd_cost_time_ns = tmp_time_ns;

    if (copy_to_user(buf, &fib_seq, sizeof(fib_seq))) {
        return -EFAULT;
    }

    return (ssize_t) sizeof(fib_seq);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return 1;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
