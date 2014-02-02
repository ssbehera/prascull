struct scull_dev {
	struct scull_qset *data; // Pointer to first quantum set
	int quantum; // each quantum size
	int qset; // each quantum set size
	unsigned long size; // amount of data stored in the device
	struct cdev char_dev;
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
