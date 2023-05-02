//Malcolm McKellips
//i2clight.c
//I2C functions for interfacing with VEML7700 ambient light sensor
//Reference: https://raspberry-projects.com/pi/programming-in-c/i2c/using-the-i2c-interface

#include <unistd.h>				
#include <fcntl.h>				
#include <sys/ioctl.h>			
#include <linux/i2c-dev.h>		
#include <stdio.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <string.h>

#define LUX_STR_LEN	(20)

int i2c_dev_fd;
char *i2c_dev_name = (char*)"/dev/i2c-1";

void write_light_sensor(char cmd_code, char data_lsb, char data_msb){
	unsigned char i2c_transfer_buffer[60];
	printf("Writing light sensor...\r\n");
	i2c_transfer_buffer[0] = cmd_code; //command code
	i2c_transfer_buffer[1] = data_lsb; //data LSB 
	i2c_transfer_buffer[2] = data_msb; //data MSB
	int bytes_to_write = 3;

	if (write(i2c_dev_fd, i2c_transfer_buffer, bytes_to_write) != bytes_to_write){
		printf("Failed to write cmd %X to the i2c device.\r\n",cmd_code);
		return;
	}
	printf("Successfully wrote to light sensor\r\n");
}

//reference: https://forums.raspberrypi.com/viewtopic.php?t=301652
void read_light_sensor(){
	unsigned char buffer[60];
	printf("Reading light sensor...\r\n");
	
	struct i2c_rdwr_ioctl_data my_i2c_rdwr;
	struct i2c_msg i2c_msg_buff[2];

	my_i2c_rdwr.nmsgs = 2;
	my_i2c_rdwr.msgs  = i2c_msg_buff;

	//First i2c message is a write of the register to edit
	buffer[0] = 0x04; //register
	i2c_msg_buff[0].addr = 0x10;
	i2c_msg_buff[0].buf  = buffer;
	i2c_msg_buff[0].len  = 1; 
	i2c_msg_buff[0].flags = 0;

	//Next, perform a 2 byte read of ALS
	i2c_msg_buff[1].addr = 0x10;
	i2c_msg_buff[1].buf  = buffer;
	i2c_msg_buff[1].len  = 2; 
	i2c_msg_buff[1].flags = I2C_M_RD;

	int res = ioctl(i2c_dev_fd, I2C_RDWR, &my_i2c_rdwr);
	if (res < 2){
		printf("Expected 2 bytes, got %d bytes\r\n", res);
	}
	printf("I2C_READ: MSB: %hhu, LSB: %hhu\r\n", buffer[1], buffer[0]);
	unsigned int MSB = (unsigned int)buffer[1];
	unsigned int LSB = (unsigned int)buffer[0];
	unsigned int raw_value = (MSB << 8) + LSB;
	printf("raw value: %d ",raw_value);
	//Equation for raw to lux is: lux = raw * 0.0576
	unsigned long lux = (unsigned long)raw_value * (unsigned long)576;
	lux = lux / (unsigned long)10000; 
	printf("LUX: %lu\r\n\r\n",lux);

	//Write lux value to the aesdchar device.
	char lux_str[LUX_STR_LEN];
	sprintf(lux_str,"LUX: %lu\r\n",lux);

    ssize_t bytes_written = 0;
    ssize_t bytes_to_write = strlen(lux_str);

    while (bytes_written != bytes_to_write){

        int char_dev_fd = open("/dev/aesdchar", (O_RDWR  | O_APPEND));

        bytes_written = write(char_dev_fd, lux_str,(bytes_to_write-bytes_written));

        close(char_dev_fd);
        
        if (bytes_written == -1)
            printf("ERROR: Issue with I2C write to char device\r\n");
    }
	
}

int main(){
	printf("I2C IOCTL program started!\r\n"); 

	if ((i2c_dev_fd = open(i2c_dev_name, O_RDWR)) < 0)
	{
		printf("Failed to open the i2c bus");
		return 1;
	}

	//might interfere with our future ioctls...
	int addr = 0x10;          //<<<<<The I2C address of the slave
	if (ioctl(i2c_dev_fd, I2C_SLAVE, addr) < 0)
	{
		printf("Failed to acquire bus access and/or talk to slave.\n");
		return 1;
	}

	printf("I2C driver initialization complete\r\n"); 

	usleep(3000); //sleep for a 3 mS to ensure that the uC has heated up

	//ALS power on, wait >= 2.5ms (ALS_SD=0)
	//ALS sensitivityx1, IT = 100ms, ALS persistence protect =1, ALS interrupt disabled, ALS enabled
	write_light_sensor(0x00, 0x00, 0x00); //confirmed as necessary prior to reads....

	usleep(5000); //wait > 2.5ms

	printf("I2C hardware initialization complete\r\n"); 

	//Integration time
	//ALS gain setting

	while(1){
		//ALS command code #4 (read out ALS data)

		read_light_sensor();

		//printf("Current Lux: %d", new_lux);

		sleep(1); //wait a second between readings...
		//might want to disable/reenable here...
	}
	
}

