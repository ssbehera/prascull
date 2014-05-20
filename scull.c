#include <linux/module.h> // basic includes
#include <linux/init.h>

#include <linux/kernel.h> //container_of macro
#include <linux/errno.h> // error nos such as -EFAULT & -EINTR
#include <linux/fs.h> // file & file_operations structure
#include <linux/slab.h> // kmalloc & kfree functions
#include <linux/types.h> //dev_t structure
#include <linux/cdev.h> // cdev structure

#include <asm/uaccess.h> // copy_to_user and copy_from_user functions
#include <linux/semaphore.h> // semaphore structure and function

#include "scull.h"

int scull_major = SCULL_MAJOR, scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum_size = SCULL_QUANTUM_SIZE;
int scull_quantum_qset_size = SCULL_QUANTUM_SET_SIZE;

dev_t devno;
struct scull_dev *my_dev;

static int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *dptr ,*next;
	int qset = dev->qset, i;
	for (dptr = dev->data; dptr ;dptr = next) {
		if (dptr->data) {
			for (i = 0; i < qset ;i++) {
				if (dptr->data[i])
					kfree(dptr->data[i]);
			}
			kfree(dptr->data);
			dptr->data = NULL;
		}
                next = dptr->next;
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = scull_quantum_size;
	dev->qset = scull_quantum_qset_size;
	dev->data = NULL;
	return 0;
}

struct scull_qset * scull_follow(struct scull_dev *dev, int index, int rw_flag)
{                                                                              
	struct scull_qset *dptr = NULL;
	int i;
	if (!dev->data && rw_flag) {
		dev->data = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);          
		if (dev->data == NULL) {       
			printk(KERN_ERR "[%s] Memory allocation failed for first qset\n", __FUNCTION__);
			return NULL;    
		}
		memset(dev->data, 0, sizeof(struct scull_qset));
		printk(KERN_INFO "[%s] Memory allocated for first qset\n", __FUNCTION__);
	}               
	else if (!dev->data && !rw_flag) {
		printk(KERN_WARNING "[%s] Trying to read unallocated memory of first qset: condition 1\n", __FUNCTION__);
		return NULL;                                                   
	}
	for (i = 0, dptr = dev->data; i < index; i++, dptr = dptr->next) {
		if (!dptr->next && rw_flag) {                                                              
			dptr->next = kmalloc(sizeof(struct scull_qset),GFP_KERNEL);     
			if (!dptr->next) {                                                      
				printk(KERN_WARNING "[%s] Memory allocation failed for [%d] qset\n", __FUNCTION__, i+1);
				return NULL;                                   
			}
			memset(dptr->next,0,sizeof(struct scull_qset));        
		}
		else if (!dptr->next && !rw_flag) {
			printk(KERN_WARNING "[%s] Trying to read unallocated memory of [%d] qset: condition 2\n",__FUNCTION__, i+1);
			return NULL;                                           
		}
	}
	return dptr;
}

int scull_open(struct inode *inode, struct file *filep)
{
	struct scull_dev *dev;

	dev = container_of(inode->i_cdev, struct scull_dev, char_dev);
	filep->private_data = dev;

	printk(KERN_INFO "[%s] scull device [%d] opened\n", __FUNCTION__, iminor(inode)+1);

	if ( (filep->f_flags & O_ACCMODE) == O_WRONLY ) {
		if (down_interruptible(&dev->sem)) // Get the device access here
			return -ERESTARTSYS;
		printk(KERN_INFO "[%s] scull device [%d] opened for writing\n", __FUNCTION__, iminor(inode)+1);
		scull_trim(dev);
		up(&dev->sem); // Give up the access to others
	}
	else
		printk(KERN_INFO "[%s] scull device [%d] opened for reading\n", __FUNCTION__, iminor(inode)+1);
	return 0;
}

int scull_release(struct inode *inode, struct file *filep)
{
	printk(KERN_INFO "[%s] scull device [%d] released\n", __FUNCTION__, iminor(inode)+1);
	return 0;
}

ssize_t scull_read(struct file *filep, char __user *buff, size_t count, loff_t *offp)
{
	struct scull_dev *dev = filep->private_data;
	struct scull_qset *dptr;
	int qset = dev->qset;
	int quantum = dev->quantum;
	int size = qset * quantum;
	int index, qset_index, quantum_index;
	ssize_t retval = 0;

	if (down_interruptible(&dev->sem)) // Get the device access here
		return -ERESTARTSYS;

	printk(KERN_INFO "[%s] Reading [%d] data from the file\n",__FUNCTION__,count);
	printk(KERN_INFO "[%s] Current file position: [%ld]\n",__FUNCTION__,(long)*offp);

	index = (long) *offp / size;
	qset_index = ((long) *offp % size ) / quantum;
	quantum_index = ((long) *offp % size) % quantum;

	printk(KERN_INFO "[%s] Positions found in the device are:\n",__FUNCTION__);
	printk(KERN_INFO "[%s] index: [%d]\n",__FUNCTION__,index);
	printk(KERN_INFO "[%s] qset index: [%d]\n",__FUNCTION__,qset_index);
	printk(KERN_INFO "[%s] quantum index: [%d]\n",__FUNCTION__,quantum_index);

	if (*offp >= dev->size)
		goto err;

	if (*offp + count > dev->size) {
		count = dev->size - *offp;
	}

	printk(KERN_INFO "[%s] dev->size: [%ld] count: [%d]\n",__FUNCTION__,dev->size,count);

	dptr = scull_follow(dev,index,SCULL_READ);

	if (dptr == NULL || !dptr->data || !dptr->data[qset_index])
                goto err;
	if (count > (quantum - quantum_index))
		count = quantum - quantum_index;
	if (copy_to_user(buff, dptr->data[qset_index]+quantum_index,count)) {
		printk(KERN_WARNING "[%s] Reading data at [%d] quantum set, [%d] quantum failed\n",__FUNCTION__, qset_index, quantum_index);
		retval = -EFAULT;
		goto err;
	}
	*offp += count;
	retval = count;

	printk(KERN_INFO "[%s] File position after reading: [%ld]\n",__FUNCTION__,(long) *offp);

	up(&dev->sem); // Give up the access to others
	return retval;
	err:
		if (*offp >= dev->size)
			printk(KERN_INFO "[%s] Read Complete\n", __FUNCTION__);
		else
			printk(KERN_INFO "[%s] Error\n",__FUNCTION__);
		up(&dev->sem); // Give up the access even when it goes bad
		return retval;
}

ssize_t scull_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp)
{
	struct scull_dev *dev = filep->private_data;
	struct scull_qset *dptr;
	int qset = dev->qset;
	int quantum = dev->quantum;
	int size = qset * quantum;
	int index, qset_index, quantum_index;
	ssize_t retval = -ENOMEM;

	if (down_interruptible(&dev->sem)) // Get the device access here
		return -ERESTARTSYS;

	printk(KERN_INFO "[%s] Writing [%d] data into the file\n", __FUNCTION__, count);
	printk(KERN_INFO "[%s] File position before writing: [%ld]\n", __FUNCTION__, (long)*offp);

	index = (long) *offp / size;
	qset_index = ((long) *offp % size ) / quantum;
	quantum_index = ((long) *offp % size) % quantum;

	printk(KERN_INFO "[%s] Positions found in the device are:\n",__FUNCTION__);
	printk(KERN_INFO "[%s] index: [%d]\n",__FUNCTION__,index);
	printk(KERN_INFO "[%s] qset index: [%d]\n",__FUNCTION__,qset_index);
	printk(KERN_INFO "[%s] quantum index: [%d]\n",__FUNCTION__,quantum_index);
	
	dptr = scull_follow(dev,index,SCULL_WRITE);

	if (dptr == NULL)
		goto err;
	if (!dptr->data) {
		printk(KERN_INFO "[%s] [%d] quantum set data pointers are not set\n", __FUNCTION__, index);
		dptr->data = kmalloc(qset * sizeof(char *),GFP_KERNEL);
		if (!dptr->data) {
			goto err;
		}
		memset(dptr->data,0,qset * sizeof(char *));
		printk(KERN_INFO "[%s] [%d] quantum set data pointer are set\n", __FUNCTION__, index);
	}
	if (!dptr->data[qset_index]) {
		printk(KERN_INFO "[%s] [%d] quantum data pointer is not set\n", __FUNCTION__, qset_index);
		dptr->data[qset_index] = kmalloc(quantum,GFP_KERNEL);
		if (!dptr->data[qset_index]) { 
			goto err;
		}
		memset(dptr->data[qset_index],0,quantum);
		printk(KERN_INFO "[%s] [%d] quantum data pointer is set\n", __FUNCTION__, qset_index);
	}
	if (count > (quantum - quantum_index))
	{
		count = quantum - quantum_index;
	}
	if (copy_from_user(dptr->data[qset_index]+quantum_index,buff,count))
	{
		printk(KERN_WARNING "[%s] Writing data at [%d] quantum set, [%d] quantum failed\n", __FUNCTION__, qset_index, quantum_index);
		retval = -EFAULT;
		goto err;
	}
	*offp += count;
	retval = count;

	printk(KERN_INFO "[%s] File position after writing: [%ld]\n",__FUNCTION__,(long) *offp);

	if (dev->size < *offp)
		dev->size = *offp;

	up(&dev->sem); // Give up the access to others
	return retval;

	err:
		up(&dev->sem); // Give up the access even when it goes bad
		printk(KERN_INFO "[%s] Error\n", __FUNCTION__);
		return retval;
}

struct file_operations scull_fops = 
{
	.owner = THIS_MODULE,
	.read = scull_read,
	.write = scull_write,
	.open = scull_open,
	.release = scull_release
};

static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err;
	dev_t devno = MKDEV(scull_major, scull_minor + index);
	cdev_init(&dev->char_dev, &scull_fops);
	dev->char_dev.owner = THIS_MODULE;
	dev->char_dev.ops = &scull_fops;
	err = cdev_add(&dev->char_dev,devno,1);
	if (err)
		printk(KERN_WARNING "[%s] Error [%d] in adding scull [%d]\n", __FUNCTION__, err, index);
}

static void scull_exit(void)
{
	int i;
	printk(KERN_INFO "[%s] Exit of scull device driver\n", __FUNCTION__);
	if (my_dev) {
		for (i = 0;i < scull_nr_devs; i++) {
			scull_trim(my_dev + i);
			cdev_del(&my_dev[i].char_dev);
			printk(KERN_INFO  "[%s] Deleted device no. [%d]\n", __FUNCTION__,i+1);
		}
		kfree(my_dev);
		printk(KERN_INFO "[%s] Freed  scull device memory\n", __FUNCTION__);
	}
	unregister_chrdev_region(devno, scull_nr_devs);
	printk(KERN_INFO "[%s] Unregistered devices\n", __FUNCTION__);
}

static int scull_init(void)
{
	int result, i;
	printk(KERN_INFO "[%s] Initialization of scull device driver\n", __FUNCTION__);
	if (scull_major) {
		devno = MKDEV(scull_major,scull_minor);
		result = register_chrdev_region(devno, scull_nr_devs, "myscull");
	}
	else {
		result = alloc_chrdev_region(&devno, scull_minor, scull_nr_devs, "myscull");
		if (result < 0) {
			printk(KERN_ERR "[%s] Can't get major no.\n", __FUNCTION__);
			return result;
		}
		scull_major = MAJOR(devno);
	}

	printk(KERN_INFO "[%s] Registered [%d] devices with major no. [%d]\n", __FUNCTION__, scull_nr_devs, scull_major);
	printk(KERN_INFO "[%s] Now setting up the scull devices\n", __FUNCTION__);

	my_dev = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if ( !my_dev ) {
		printk(KERN_ERR "[%s] Memory allocation for the device failed\n", __FUNCTION__);
		result = -ENOMEM;
		goto fail;
	}
	memset(my_dev,0,sizeof(struct scull_dev) * scull_nr_devs);
	
	for (i = 0; i < scull_nr_devs ; i++) {
		my_dev[i].quantum = scull_quantum_size;
		my_dev[i].qset = scull_quantum_qset_size;
		sema_init(&my_dev[i].sem, 1);
		scull_setup_cdev(&my_dev[i], i);
		printk(KERN_INFO "[%s] device no. [%d] setup is complete\n", __FUNCTION__, i+1);
	}
	return 0;
	fail:
		scull_exit();
		return result;
}

MODULE_AUTHOR("Subhendu Sekhar Behera");
MODULE_LICENSE("Dual BSD/GPL License");

module_init(scull_init);
module_exit(scull_exit);
