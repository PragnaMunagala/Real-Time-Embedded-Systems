#include <zephyr.h>
#include <kernel.h>
#include <misc/printk.h>
#include <device.h>
#include <gpio.h>
#include <pinmux.h>
#include <misc/util.h>
#include <board.h>
#include <i2c.h>
#include <sensor.h>
#include <misc/byteorder.h>
#include <toolchain.h>
#include "kalman.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define STACKSIZE 1024		//stack size for threads
#define RAD_TO_DEG 57.29578		//radians to degree conversion value for kalman filter
#define SAMPLING_TIME 0.001		//sampling time for kalman filter
#define PRIORITY 7

/* Device structures for gpio, pinmux and I2C device */
struct device *gpio_io0, *gpio_ls0, *gpio_io1, *gpio_ls1, *gpio_sda, *gpio_scl, *i2c_dev, *pinmuxio1, *pinmuxsda, *pinmuxscl;
int edgeDetect = 0;		//flag to indicate echo pulse width detection
uint32_t start_time;  //to hold start time of echo pulse
uint32_t end_time;		//to hold the end time of echo pulse
uint32_t cycles_spent;		//cycles spent in detection of echo pulse
uint32_t nanoseconds_spent;		//nano seconds duration of echo
double distance;	//to hold the distance of the object
double acc_X, acc_Y, acc_Z, gyro_X, gyro_Y, gyro_Z, gyro_X_old, gyro_Y_old, gyro_Z_old;   //to hold the acceleromter and gyroscopic x,y,z values
char s[50];

struct kalman kgyroX, kgyroY;    //structure for kalman filter
double roll, pitch;		//to hold roll and pitch values to kalman filter
double kalman_x = 0.0, kalman_y = 0.0;    //to hold the filtered x and y values from kalman filter

K_SEM_DEFINE(disThread_sem, 1, 1);	//semaphore for distance thread; starts off "available"
K_SEM_DEFINE(accThread_sem, 1, 1);	//semaphore for acceleration thread; starts off "available"
K_SEM_DEFINE(printThread_sem, 1, 1);	//semaphore for printing thread; starts off "available"


/************************* To set the GPIO pins ********************************/
void gpioSet(){
	/* Device binding for GPIO pins */
	gpio_io0 = device_get_binding("GPIO_0");
	gpio_ls0 = device_get_binding("EXP1");
	gpio_io1 = device_get_binding("GPIO_0");
	gpio_ls1 = device_get_binding("EXP0");
	gpio_sda = device_get_binding("EXP0");
	gpio_scl = device_get_binding("EXP0");

	/* device binding and mux set for GPIO pins */
	pinmuxio1 = device_get_binding(CONFIG_PINMUX_NAME);
	pinmux_pin_set(pinmuxio1, 1, PINMUX_FUNC_A);
	pinmuxsda = device_get_binding(CONFIG_PINMUX_NAME);
	pinmux_pin_set(pinmuxsda, 18, PINMUX_FUNC_C);
	pinmuxscl = device_get_binding(CONFIG_PINMUX_NAME);
	pinmux_pin_set(pinmuxscl, 19, PINMUX_FUNC_C);

	/* configure GPIO pins direction and value */
	gpio_pin_configure(gpio_io0, 3, GPIO_DIR_OUT);
	gpio_pin_configure(gpio_ls0, 0, GPIO_DIR_OUT);
	gpio_pin_write(gpio_ls0, 0, 0);
	gpio_pin_configure(gpio_io1, 4, GPIO_DIR_IN);
	gpio_pin_configure(gpio_ls1, 12, GPIO_DIR_IN);
	gpio_pin_write(gpio_ls1, 12, 1);	
}


/************************* kalman filter function ********************************/
void kalman_filter(){	
	static int first = 1;

	/* For initial tuning of kalman filter */
	if(first){
		gyro_X_old = gyro_X;
		gyro_Y_old = gyro_Y;
		gyro_Z_old = gyro_Z;

		kgyroX.Q_angle = 0.001;
		kgyroX.Q_bias = 0.003;
		kgyroX.R_measure = 0.03;

		/* Reset angle and bias */
		kgyroX.angle = 0.0;
		kgyroX.bias = 0.0;

		kgyroX.P[0][0] = 0.0;
		kgyroX.P[0][1] = 0.0;
		kgyroX.P[1][0] = 0.0;
		kgyroX.P[1][1] = 0.0;

		kgyroY.Q_angle = 0.001;
		kgyroY.Q_bias = 0.003;
		kgyroY.R_measure = 0.03;

		/* Reset angle and bias */
		kgyroY.angle = 0.0;
		kgyroY.bias = 0.0;

		kgyroY.P[0][0] = 0.0;
		kgyroY.P[0][1] = 0.0;
		kgyroY.P[1][0] = 0.0;
		kgyroY.P[1][1] = 0.0;

		/* calculating the initial roll and pitch values */
		roll  = atan2(acc_Y, acc_Z) * RAD_TO_DEG;
    	pitch = atan(-acc_X / sqrt(acc_Y * acc_Y + acc_Z * acc_Z)) * RAD_TO_DEG;

    	/* setting the initial angles of x and y kalman variables */
    	kgyroX.angle = roll;
    	kgyroY.angle = pitch;

    	first = 0;
	}else{
		/* Converting to degrees/s */
		double gyroXrate = gyro_X / 131.0; 
  		double gyroYrate = gyro_Y / 131.0;

  		//calculating the roll value
		roll  = atan2(acc_Y, acc_Z) * RAD_TO_DEG;

		//checking for infinity value of tan and returning tan(infinity) = 90
		if ((float)acc_Y == 0.0f && (float)acc_Z == 0.0f)
		{
			pitch =  90;
		}
		else
    		pitch = atan(-acc_X / sqrt(acc_Y * acc_Y + acc_Z * acc_Z)) * RAD_TO_DEG;

    	//setting the angle of filter as per the roll range
    	if ((roll < -90 && kalman_x > 90) || (roll > 90 && kalman_x < -90)) {
    		kgyroX.angle = roll;
    		kalman_x = roll;
		  } else
		    kalman_x = kalman_angle(roll, gyroXrate, SAMPLING_TIME, &kgyroX); 

		if (abs(kalman_x) > 90)
		    gyroYrate = -gyroYrate;

		//setting the pitch i.e., y angle to kalman filter
		kgyroY.angle = pitch;

		//return angle from kalman filter
		kalman_y = kalman_angle(pitch, gyroYrate, SAMPLING_TIME, &kgyroY);
	}
}

/************************* To call sensor channel get for various channel flags **************************/
double channel_fetch(struct device *i2c, enum sensor_channel flag, struct sensor_value temp){
	int rc;
	double val;

	//get the channel data from MPU6050 sensor
	rc = sensor_channel_get(i2c, flag, &temp);
	if (rc != 0) {
		printk("sensor_channel_get error: %d\n", rc);
		return 0;
	}

	//to properly return the double value of the sensed value
	if(temp.val2 < 0)
		temp.val2 = -1*temp.val2;
	if(temp.val1 < 0)
		val = (double)((double)temp.val1 - (double)temp.val2 / 1000000);
	else
		val = (double)((double)temp.val1 + (double)temp.val2 / 1000000);
	return  val;
}

/**********  To return the double value for printing   ***********/
char * printing(double x)
{
	sprintf(s,"%f",x);
	return s;
}

/***************************  Thread function for printing the distance and position values  **************************/
void threadPrintFunc(void *a, void *b, void *c){
	while(1){
		k_sem_take(&printThread_sem, K_FOREVER);        //taking print thread semaphore
		
		printk("distance = %s\n", printing(distance));

		printk("Acceleration X = %s\n", printing(acc_X));
		printk("Acceleration Y = %s\n", printing(acc_Y));
		printk("Acceleration Z = %s\n", printing(acc_Z));

		printk("Gyroscope X = %s\n", printing(gyro_X));
		printk("Gyroscope Y = %s\n", printing(gyro_Y));
		printk("Gyroscope Z = %s\n", printing(gyro_Z));

		printk("Filtered X value(roll) = %s\n", printing(kalman_x));
		printk("Filtered Y value(pitch) = %s\n\n\n\n", printing(kalman_y));
		k_sleep(1000);
		k_sem_give(&disThread_sem);     //giving distance thread semaphore
	}

}

/**************************  Thread function for fetching the MPU6050 sensor values  ****************************/
void threadAccelFunc(void *a, void *b, void *c){
	gpioSet();      //to set the gpio

	/* To bind the I2C device */
	i2c_dev = device_get_binding(CONFIG_MPU6050_NAME);
	if (i2c_dev == NULL) {
		printk("I2C0: Device not found.\n");
	}
	
	while(1){
		k_sem_take(&accThread_sem, K_FOREVER);		//taking acceleration thread semaphore
		struct sensor_value temp;
		int rc;
		rc = sensor_sample_fetch(i2c_dev);           //fetching the sample values of mpu6050
		
		/* fetching the acceleration x,y,z and gyroscope x,y,z values */
		acc_X = channel_fetch(i2c_dev, SENSOR_CHAN_ACCEL_X, temp);
		acc_Y = channel_fetch(i2c_dev, SENSOR_CHAN_ACCEL_Y, temp);
		acc_Z = channel_fetch(i2c_dev, SENSOR_CHAN_ACCEL_Z, temp);
		gyro_X = channel_fetch(i2c_dev, SENSOR_CHAN_GYRO_X, temp);
		gyro_Y = channel_fetch(i2c_dev, SENSOR_CHAN_GYRO_Y, temp);
		gyro_Z = channel_fetch(i2c_dev, SENSOR_CHAN_GYRO_Z, temp);	

		//function for kalman filtering
		kalman_filter();

		k_sem_give(&printThread_sem);		//giving print thread semaphore
	}
}

void threadDistanceFunc(void *a, void *b, void *c){
	uint32_t val = 0;
	gpioSet();
	
	while (1) {
		k_sem_take(&disThread_sem, K_FOREVER);     //taking distance thread semaphore

		/* to generate the trigger */
		gpio_pin_write(gpio_io0, 3, 1);
		k_busy_wait(30);
		gpio_pin_write(gpio_io0, 3, 0);

		/* reading echo pin until 1 is detected */
		while(1){
			gpio_pin_read(gpio_io1, 4, &val);
			if((int)val == 1){
				start_time = k_cycle_get_32();     //start time of echo pulse
				break;
			}
		}

		/* reading echo pin until 0 is detected */
		while(1){
			gpio_pin_read(gpio_io1, 4, &val);		
			if((int)val == 0){
				end_time = k_cycle_get_32();	//end time of echo pulse
				edgeDetect = 1;
				break;
			}
		}
		
		/* calculating distance once echo pulse is detected */
		if(edgeDetect){
			edgeDetect = 0;
			cycles_spent = end_time - start_time;
			nanoseconds_spent = SYS_CLOCK_HW_CYCLES_TO_NS(cycles_spent);
			distance = (nanoseconds_spent* 17.0 )/1000000;
		}
		k_sem_give(&accThread_sem); 	//giving acceleration thread semaphore
	}
}

K_THREAD_DEFINE(threadA_id, STACKSIZE, threadDistanceFunc, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);	//distance thread
K_THREAD_DEFINE(threadB_id, STACKSIZE, threadAccelFunc, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);		//mpu6050 sensor thread
K_THREAD_DEFINE(threadPrint_id, STACKSIZE, threadPrintFunc, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);	//printing thread
