//Malcolm McKellips
//i2clight.c
//I2C functions for interfacing with VEML7700 ambient light sensor
//Reference: https://raspberry-projects.com/pi/programming-in-c/i2c/using-the-i2c-interface

#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port
#include <stdio.h>

int file_i2c;
unsigned char buffer[60] = {0};
char *filename = (char*)"/dev/i2c-1";


void write_light_sensor(char cmd_code, char data_lsb, char data_msb){
	printf("Writing light sensor...\r\n");
	buffer[0] = cmd_code; //command code
	buffer[1] = data_lsb; //data LSB 
	buffer[2] = data_msb; //data MSB
	int bytes_to_write = 3;

	if (write(file_i2c, buffer, bytes_to_write) != bytes_to_write){
		printf("Failed to write cmd %X to the i2c device.\r\n",cmd_code);
		return;
	}
	printf("Successfully wrote to light sensor\r\n");
}

void read_light_sensor(){
	printf("Reading light sensor...\r\n");
	buffer[0] = 0x04;
	int bytes_to_read = 2; 
	write(file_i2c, buffer, 1);
	read(file_i2c, buffer,bytes_to_read);
	printf("write and read finished\r\n");
	printf("First byte: %u, Second byte: %u\r\n",(unsigned int)buffer[0],(unsigned int)buffer[1]);
	/*
	if (write(file_i2c, buffer, 1) != 1){
		printf("Failed to select 0x04 register in i2c device read.\r\n");
		return -1;
	}
	//printf("location 0x04 selected in light sensor...\r\n");
	//int bytes_to_read = 2;

	if (read(file_i2c, buffer,bytes_to_read) != bytes_to_read){
		printf("Failed to read both bytes in i2c device read.\r\n");
		return -1;
	}
	printf("Successfully read both i2c bytes...\r\n");
	unsigned int raw_value = (buffer[1] << 8) + buffer[0]; //data is in form LSB, MSB
	printf("Raw Value: %d\r\n",raw_value);
	float lux = (float)raw_value * 0.0576;
	*/
	//return (int)lux;
}

int main(){
	printf("I2C program started!\r\n"); 

	if ((file_i2c = open(filename, O_RDWR)) < 0)
	{
		printf("Failed to open the i2c bus");
		return 1;
	}

	int addr = 0x10;          //<<<<<The I2C address of the slave
	if (ioctl(file_i2c, I2C_SLAVE, addr) < 0)
	{
		printf("Failed to acquire bus access and/or talk to slave.\n");
		return 1;
	}

	printf("I2C driver initialization complete\r\n"); 

	usleep(3000); //sleep for a 3 mS to ensure that the uC has heated up

	//ALS power on, wait >= 2.5ms (ALS_SD=0)
	//ALS sensitivityx1, IT = 100ms, ALS persistence protect =1, ALS interrupt disabled, ALS enabled
	////write_light_sensor(0x00, 0x00, 0x00); 

	//usleep(5000); //wait > 2.5ms

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
