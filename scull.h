#ifndef SCULLC_H
#define SCULLC_H


struct scull_dev {
	struct scull_qset *data; // Pointer to first quantum set
	int quantum; // each quantum size
	int qset; // each quantum set size
	unsigned long size; // amount of data stored in the device
	struct cdev char_dev; // char device structure
	struct semaphore sem; // mutual exclusion semaphore
};

struct scull_qset {
	void **data;
	struct scull_qset *next;
};

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 1
#endif

#ifndef SCULL_QUANTUM_SIZE
#define SCULL_QUANTUM_SIZE 5
#endif

#ifndef SCULL_QUANTUM_SET_SIZE
#define SCULL_QUANTUM_SET_SIZE 2
#endif

#ifndef SCULL_READ
#define SCULL_READ 0
#endif

#ifndef SCULL_WRITE
#define SCULL_WRITE 1
#endif

/*
 * Ioctl Definitions
 */

/*
 * S: Set thru pointer
 * T: Tell directly with arg
 * G: Reply thru pointer
 * Q: response on ret val
 * X: exchange G & S atomically
 * H: switch T & Q atomically
 */

#define SCULL_IOC_MAGIC 's'

#define SCULL_IOCRESET		_IO(SCULL_IOC_MAGIC, 0)
#define SCULL_IOCSQUANTUM	_IOW(SCULL_IOC_MAGIC, 1, int)
#define SCULL_IOCSQSET		_IOW(SCULL_IOC_MAGIC, 2, int)
#define SCULL_IOCTQUANTUM	_IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOCTQSET		_IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOCGQUANTUM	_IOR(SCULL_IOC_MAGIC, 5, int)
#define SCULL_IOCGQSET		_IOR(SCULL_IOC_MAGIC, 6, int)
#define SCULL_IOCQQUANTUM	_IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOCQQSET		_IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOCXQUANTUM	_IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET		_IOWR(SCULL_IOC_MAGIC, 10, int)
#define SCULL_IOCHQUANTUM	_IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOCHQSET		_IO(SCULL_IOC_MAGIC, 12)

#define SCULL_IOC_MAXNR		12

#endif
