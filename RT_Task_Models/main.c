#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sched.h>
#include <linux/input.h>
#include <linux/types.h>
#include <string.h> 
#include <signal.h>

typedef void (*functionType)(int x);         //defining a function type to hold thread function values
pthread_mutex_t m_lock[10], count_lock;		//m_lock[10] - 10 mutex locks and count_lock is lock used for lock section for activating threads
pthread_mutexattr_t mutexattr;			//mutex attribute to set PI
pthread_cond_t count = PTHREAD_COND_INITIALIZER;

/* Flag to indicate the termination condition */
int termination = 0;

/* To hold the tasks data read from the input file */
char **input;

struct timespec next, period;           //to set period for periodic task
int count_tasks = 0, no_of_tasks = 0, sigcount = 0;

pthread_t *thread, sample_thread;	// *thread - To create the number of tasks as given in the input file, sample thread to test mutex PI policy



/***************** signal handler function for SIGUSR1 which will be delivered by threads ***************/
void handlerFun(int signal_number){
	sigcount++;
	printf("signal count = %d\n", sigcount);
}



/**************** signal handler function for alarm signal ********************/
void handler(int signal_number){
	int i;

	//creating sigaction and assigning handler to signal
	struct sigaction si;
	memset(&si, 0, sizeof(si));
	si.sa_handler = &handlerFun;
	sigaction(SIGUSR1, &si, NULL);

	termination = 1;     //setting the termination flag
	
	/* To generate the signal to all the threads for termination */
	for(i = 0; i < no_of_tasks; i++)
		pthread_kill(thread[i], SIGUSR1);
	pthread_kill(sample_thread, SIGUSR1);
}


/******************* Periodic Task Thread function ******************/
void* p_thread_function(void *ptr){
	int i ,j = 0, lock_index;
   	
    period.tv_sec = (atoi(input[4]))/1000;
    period.tv_nsec = 0;

    /********* code for implemention of tasks activation at same time ********/
    pthread_mutex_lock(&count_lock);
    
    count_tasks++;         //counter for the number of tasks

    //to wait till all the threads are ready
    if(count_tasks != (atoi(input[0])+1))
    	pthread_cond_wait(&count, &count_lock);
    
    //to unblock the threads
    if(count_tasks == (atoi(input[0])+1))
    	pthread_cond_signal(&count);

    pthread_mutex_unlock(&count_lock);

    //to get the time for setting the period
    clock_gettime(CLOCK_MONOTONIC, &next);

    //to get the value of the mutex to be locked and unlocked
    if(strlen(input[6]) > 2)	//checking if mutex value is 10
    		lock_index = 10;
    else	
    	lock_index = (input[6][1]) - '0';
	
	//loop until time after which execution has to terminate
	while(!termination){
		next.tv_sec = next.tv_sec + period.tv_sec;	//calculating period

		/* Busy loop */
		for(i = 0; i < atoi(input[5]); i++)
			j = j + i;
			
		/* Locking the mutex */
		pthread_mutex_lock(&m_lock[lock_index - 1]);
		
		/* Busy loop */
		for(i = 0; i < atoi(input[7]); i++)
			j = j + i;
			
		/* Unlocking the mutex */
		pthread_mutex_unlock(&m_lock[lock_index - 1]);
		
		/* Busy loop */
		for(i = 0; i < atoi(input[9]); i++)
			j = j + i;

		/* To call the thread periodically */
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, 0);
	}
	return NULL;
}

/***************** Aperiodic Task Thread function *******************/
void* ap_thread_function(void *ptr){
	struct input_event event;      //for keyboard input event
	int check, i, j = 0, fd, event_key;

	/********* code for implemention of tasks activation at same time ********/
	pthread_mutex_lock(&count_lock);

	count_tasks++;         //counter for the number of tasks

    //to wait till all the threads are ready
	if(count_tasks != (atoi(input[0])+1))
    	pthread_cond_wait(&count, &count_lock);
    
    //to unblock the threads   
    if(count_tasks == (atoi(input[0])+1))
    	pthread_cond_signal(&count);

    pthread_mutex_unlock(&count_lock);
	
	/* To open the keyboard device file */
	fd = open("/dev/input/event2", O_RDONLY);
	
	//to check if device file open is successful
	if (fd == -1) {
		printf("Cannot open device file.\n");	
	}
	else{
		//loop until time after which execution has to terminate
		while(!termination){
			check = read(fd, &event, sizeof(struct input_event));       //To read the input event
			if(check != -1){
				/* Assigning key values based on the key pressed */
				switch(atoi(input[12])){
					case 0: event_key = 11; break;
					case 1: event_key = 2; break;
					case 2: event_key = 3; break;
					case 3: event_key = 4; break;
					case 4: event_key = 5; break;
					default: printf("Enter key events between 0 and 4\n");
				}
				
				/* To detect the key event and run a busy loop */
				if(event.type == EV_KEY && event.code == event_key && event.value == 0){ 
					printf("key is pressed\n");
					/* Busy loop */
					for(i = 0; i < atoi(input[13]); i++)
						j = j + i;
				}
			}
		}
	}	
	return NULL;
}

/******************* sample thread function to implement mutex PI *******************/
void* sample_function(void *ptr){
	int i ,j = 0, lock_index;

	/********* code for implemention of tasks activation at same time ********/
	pthread_mutex_lock(&count_lock);

	count_tasks++;         //counter for the number of tasks

    //to wait till all the threads are ready
	if(count_tasks != (atoi(input[0])+1))
    	pthread_cond_wait(&count, &count_lock);
    
    //to unblock the threads   
    if(count_tasks == (atoi(input[0])+1))
    	pthread_cond_signal(&count);

    pthread_mutex_unlock(&count_lock);

    //to get the value of the mutex to be locked and unlocked
	if(strlen(input[6]) > 2)	//checking if mutex value is 10
    		lock_index = 10;
    else	
    	lock_index = (input[6][1]) - '0';

    //loop until time after which execution has to terminate
	while(!termination){

		/* Busy loop */
		for(i = 0; i < 300; i++)
			j = j + i;
			
		/* Locking the mutex */
		pthread_mutex_lock(&m_lock[lock_index - 1]);
		
		/* Busy loop */
		for(i = 0; i < 500; i++)
			j = j + i;
			
		/* Unlocking the mutex */
		pthread_mutex_unlock(&m_lock[lock_index - 1]);
	}
	return NULL;
}
	
/******************************  Main Program  ******************************/
int main(){
	int i = 0, j, flag = 0;	

	//to open the input file for reading the tasks input
	FILE *fp = fopen("./input.txt","r");
	char buf[10];

	//to read input data word by word into input[] array
	while(fscanf(fp, "%s", buf) != EOF )
	{
		if(flag == 0)
		{
			input = (char **)malloc(sizeof(char *));	
			flag = 1;
		}
		else
		{
			input = realloc(input, sizeof(char *)*(i+1));    //to allocate the memory dynamically
		}

		input[i] = malloc(strlen(buf)*sizeof(char));
		strcpy(input[i], buf);
		i++;		
	}

	no_of_tasks = atoi(input[0]);     //number of tasks to be created

	struct sigaction sa;
	
	/* creating sigaction and handler for alarm signal */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &handler;
	sigaction(SIGALRM, &sa, NULL);
	
	//generating the alarm after the execution time from input file
	alarm((atoi(input[1]))/1000);	

	//threads and attributes creation for the tasks
	thread = malloc(sizeof(pthread_t)*no_of_tasks);
	pthread_attr_t attr[no_of_tasks], sample_attr;
	struct sched_param param[no_of_tasks], sample_param;
	int thread_priority[no_of_tasks];

	//assigning function pointers of the threads function
	functionType *fptr = (functionType *)malloc(sizeof(functionType *) * 2);
	fptr[0] = (functionType)&p_thread_function;
	fptr[1] = (functionType)&ap_thread_function;

	//to read the threads priorities from input file
	j = 3;
	for(i = 0; i < no_of_tasks; i++){
		thread_priority[i] = atoi(input[j]);
		j = j + 8;
	}

	//setting mutex attribute for PI implementation
	pthread_mutexattr_init(&mutexattr); 		
	pthread_mutexattr_setprotocol(&mutexattr, PTHREAD_PRIO_INHERIT);

	/* initializing the mutex lock */
	for(i = 0; i < 10; i++)
		pthread_mutex_init(&m_lock[i], &mutexattr);
	
	pthread_mutex_init(&count_lock,NULL);        //mutex used for activation of threads

	/* initialize a condition variable to its default value */
	pthread_cond_init(&count, NULL);

	/* Initialized with default attributes */
	for(i = 0; i <  no_of_tasks; i++){
		pthread_attr_init(&attr[i]);

		/* setting the scheduling policy */
		pthread_attr_setschedpolicy(&attr[i], SCHED_FIFO);

		/* safe to get existing scheduling param */
		pthread_attr_getschedparam(&attr[i], &param[i]);

		/* setting the priority */
		param[i].sched_priority = thread_priority[i];	
	}

	//setting the attributes for the sample thread
	pthread_attr_init(&sample_attr);
	pthread_attr_setschedpolicy(&sample_attr, SCHED_FIFO);
	pthread_attr_getschedparam(&sample_attr, &sample_param);
	sample_param.sched_priority = 30;
	
	//creating number of tasks read from input file
	for(i = 0; i < no_of_tasks; i++)
		pthread_create(&thread[i], &attr[i], (void *)fptr[i%2], NULL);

	//creating sample thread
	pthread_create(&sample_thread, &sample_attr, sample_function, NULL);
	
	//joining threads so that main function doesn't terminate before thread execution
	for(i = 0;i < no_of_tasks; i++)
		pthread_join(thread[i], NULL);

	//join for sample thread
	pthread_join(sample_thread, NULL);
	
	//to destroy the mutex locks
	for(i = 0; i < 10; i++)	
		pthread_mutex_destroy(&m_lock[i]);

	//to destroy the mutex created for activating threads simultaneously
	pthread_mutex_destroy(&count_lock);	
		
	return 0;
}
