#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>          

#define  DEVICE_NAME "fifo_pipe"    
#define  CLASS_NAME  "fifo"         
MODULE_LICENSE("GPL");
static int    majorNumber;                  
static int    numberOpens = 0;              
static struct class*  charClass  = NULL;
static struct device* charDevice = NULL;
 
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
 
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};
 
char BUFFER[200][100];
int buffer_size;
static int max_limit = 5;
module_param(max_limit, int, 0);
wait_queue_head_t empty_q;
wait_queue_head_t full_q;
static DEFINE_MUTEX(mlock);
static int __init char_init(void){  // static int __init char_init(void){

printk(KERN_INFO " Initializing the pipe \n");
 
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "Char failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO " major number %d\n", majorNumber);
 
   charClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(charClass)){                
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(charClass);          
   }
 
  
  charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(charDevice)){              
      class_destroy(charClass);          
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(charDevice);
   }
   printk(KERN_INFO " device class created correctly\n"); 
   init_waitqueue_head(&empty_q);  
   init_waitqueue_head(&full_q);  
   return 0;
}
 
static void __exit char_exit(void){
   device_destroy(charClass, MKDEV(majorNumber, 0));    
   class_unregister(charClass);                          
   class_destroy(charClass);                            
   unregister_chrdev(majorNumber, DEVICE_NAME);            
  
}
 
static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO " Device has been opened %d time(s)\n", numberOpens);

   return 0;
}
 
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int i;
   int buf_len;
   if(buffer_size == 0){
      if(wait_event_interruptible(empty_q, buffer_size != 0) < 0){
         return -1;
      }
   }
   buf_len = strlen(BUFFER[0]);
   if(copy_to_user(buffer, BUFFER[0], buf_len) != 0){
      printk(KERN_INFO "error");
      return -1;
   }
   for( i = 0; i < buffer_size - 1; i ++){
      strcpy(BUFFER[i], BUFFER[i+1]);
   }
   buffer_size --;
   if(buffer_size == max_limit - 1){
      wake_up(&full_q);
   }
   return buf_len;
}
 
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   if(buffer_size == max_limit){
      if(wait_event_interruptible(full_q, buffer_size != max_limit) < 0){
         return -1;
      }
   }
   if(copy_from_user(BUFFER[buffer_size], buffer, len) != 0){
      printk(KERN_INFO "error");
      return -1;
   }
   buffer_size ++;
   if(buffer_size == 1){
      wake_up(&empty_q);
   }
   return len;
}
 
static int dev_release(struct inode *inodep, struct file *filep){
   int i;
   printk(KERN_INFO " Device successfully closed\n");
   printk(KERN_INFO " THe Buffer size is %d\n", buffer_size);
   for(i = 0; i < buffer_size; i ++){
      printk("The string at %d is %s\n", i, BUFFER[i]);
   }
   return 0;
}
 
module_init(char_init);
module_exit(char_exit);
 