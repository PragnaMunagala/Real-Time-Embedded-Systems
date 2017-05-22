#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

int range[4] = {100,1000,10000,100000};        //ranges for periods
float utili[10] = {0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95};     //ranges for utilizations

int task_sets, *no_of_tasks;    //to hold the number of tasksets and number of tasks in each taskset
float ***tasks;   //3D pointer to hold the tasks info from input file
int edf_count = 0, rm_count = 0, dm_count = 0;           //count to hold the number of tasks schedulable
int printFlag = 0;				//flag to print schedulable result for 1st task (reading data from input file)
int indication = 0;

/*********** Function to find min among deadline and period *************/
float minimum(float d, float p)
{
	float mini;

	if(d < p)
		mini = d;
	else
		mini = p;

	return mini;
}

/******* Sorting Function (index = 1 => sort by period; index = 2 => sort by deadline;) **********/
void sort(int index)
{
	float *c;
	int i, j, k, l;

	/* To sort as per periods */
	if(index == 1)
		l = 2;
	else if(index == 2)    /* To sort as per deadlines */
		l = 1;

	/* sort for each task set */
	for(i = 0; i < task_sets; i++)
	{
		/* sort for tasks in taskset */
		for(k = 0; k < no_of_tasks[i] - 1; k++)
		{	
			j = k;
			/* to compare each task with all other tasks in set */
			while(j < no_of_tasks[i] - 1){	
				/* sorting */		
				if(tasks[i][k][l] > tasks[i][j+1][l]){
					c = tasks[i][k];
					tasks[i][k] = tasks[i][j+1];
					tasks[i][j+1] = c;
				}	
				j++;		
			}	
		}
	}
}

/* To calculate the next value of loading factors */
float LoadFactor(float n, int index)
{
	int i;
	float temp = 0.0;
    for (i = 0; i < no_of_tasks[index]; i++)
    {
        temp = temp + (ceil(n/(float)tasks[index][i][2])) * tasks[index][i][0]; 
    }
    return temp;
}

/* To calculate the busy period */
float BusyPeriod(int index)
{
	int j;
	float L0 = 0.0, L1 = 0.0;
	for(j = 0; j < no_of_tasks[index]; j++){
		L0 += tasks[index][j][0];
	}
	L1 = LoadFactor(L0, index);

	/* calculating the busy period until L(n+1) = L(n) */
	while(L1 != L0)
    {
        L0	=	L1;	
        L1	=	LoadFactor(L0, index);	
    }	
    return L1;
}

/* Load factor test for the EDF schedulability test */
void loadFactorTest(float bp, int index)
{
	int i = 0, temp = 0, flag = 1, k = 0, j;
	float *testPoints = NULL;

	while(1)
	{
		/* Finding the test points */
		if(flag == 1)
		{
			testPoints = calloc(1,sizeof(float));	
			flag = 0;
			if(tasks[index][i][1] + temp*tasks[index][i][2] == bp)
			{
				testPoints[0] = tasks[index][i][1] + temp*tasks[index][i][2];	  //calculating the next test point using deadline+period
				k=1;	
				break;
			}
		}
		else
			testPoints = realloc(testPoints, (k+1)*sizeof(float));	


		if(tasks[index][i][1] + temp*tasks[index][i][2] == bp)            //calculate test points until testpoint == busy period
		{			
			testPoints[k] = tasks[index][i][1] + temp*tasks[index][i][2];
			k++;
			break;			
		}
		if (tasks[index][i][1] + temp*tasks[index][i][2] > bp)		//break once test point > busy period value and printing first missing deadline
		{
			if(printFlag == 0)
				printf("The first missing deadline is at %f and ", (tasks[index][i][1] + temp*tasks[index][i][2]));
			break;
		}

		testPoints[k] = tasks[index][i][1] + temp*tasks[index][i][2];
		i++;k++;
		if(i%no_of_tasks[index] == 0)
		{
			temp++;
			i = 0;
		}			
	}

	if(k == 0)
		goto label1;

	float *h = calloc(k, sizeof(float));

	/* Calculating the loading factor for each test point */
	for(i = 0; i < k; i++)
	{
		for(j = 0; j < no_of_tasks[index]; j++)
		{
			if(tasks[index][j][1] <= testPoints[i])
			{
				h[i] += (1.0 + floor((testPoints[i] - tasks[index][j][1])/tasks[index][j][2]))*tasks[index][j][0];			
			}
			else
				break;
		}
		/* calculating the utilization for each test point */
		h[i] = h[i]/testPoints[i];

		/* Not schedulable condition if U > 1 */
		if(h[i] > 1.0)
		{
			label1: 
				if(printFlag == 0)
					printf("Task set %d is not schedulable using EDF\n", index+1);
			goto label;
		}
	}

	/* all test point U is <= 1, so schedulable */
	if(printFlag == 0)
		printf("Task set %d is schedulable using EDF\n", index+1);
	edf_count++;
	label:free(h);free(testPoints);
	asm volatile("nop");
}

/***************** Earliest Deadline First Algorithm *****************/
void EDF()
{
	int i, j;
	float utilization, mini, bp;    //to hold the utilization and min(deadline, period)
	int counter=0;
	sort(2);

	/* schedulability of each taskset */
	for(i = 0; i < task_sets; i++)
	{
		utilization = 0.0;
		counter = 0;
		/* Running EDF for each taskset */
		for(j = 0; j < no_of_tasks[i]; j++)
		{
			if(tasks[i][j][1] == tasks[i][j][2])
				counter++;
			/* To calculate the minimum value among deadline and period of a task */
			mini = minimum(tasks[i][j][1], tasks[i][j][2]);	
			/* Calculating U = sum(ei/min(di, pi))*/
			utilization += tasks[i][j][0]/mini;
		}
		/* To check if each taskset is schedulable or not */
		if(utilization <= 1)
		{
			if(printFlag == 0)
				printf("Task set %d is schedulable using EDF\n", i+1);
			edf_count++;
		}
		if((utilization > 1) && (counter == no_of_tasks[i]))
		{
			if(printFlag == 0)
				printf("Task set %d is not schedulable using EDF\n", i+1);
		}
		/* load factor test if U > 1 and Di < Pi */
		if((utilization > 1) && (counter != no_of_tasks[i]))
		{	
			bp = BusyPeriod(i);
			loadFactorTest(bp, i);
		}
	}
}

/* Response time analysis for RM and DM algorithms */
void ResponseTimeTest(int index, int n, int flag){
	int i;
	float a0 = 0.0, aprev, acurr = 0.0;

	/* calculating the a0 value */
	for(i = 0; i < no_of_tasks[index]; i++)
		a0 += tasks[index][i][0];

	aprev = a0;

	/* iterating until a(n+1) = a(n) */
	while(1)
	{
		acurr += tasks[index][n][0];
		for(i = 0; i < n; i++)
		{
			acurr += (ceil(aprev/tasks[index][i][2]))*tasks[index][i][0];
		}
		if(acurr == aprev)
			break;
		else
		{
			aprev = acurr;
			acurr = 0.0;
		}
	}

	if(printFlag == 0)
		printf("Worst case Response Time = %f and ", acurr);        //printing WCET

	/* Rate Monotonic */
	if(flag == 1)
	{
		if(acurr <= tasks[index][n][1]){     //comparing WCET <= deadline
			if(printFlag == 0)
				printf("Task set %d is schedulable using Rate Monotonic\n", index+1);
			rm_count++;
		}
		else
		{
			if(printFlag == 0)
				printf("Task set %d is not schedulable using Rate Monotonic\n", index+1);
		}
	}
	/* Deadline Monotonic */
	else
	{
		if(acurr <= tasks[index][n][1])			//comparing WCET <= deadline
		{
			if(printFlag == 0)
				printf("Task set %d is schedulable using Deadline Monotonic\n", index+1);
			dm_count++;
		}
		else
		{
			if(printFlag == 0)
				printf("Task set %d is not schedulable using Deadline Monotonic\n", index+1);
		}
	}

}

/* Utilization bound test for each taskset */
void EffectiveUtest(int index, int flag){
	float utilization = 0.0, temp = 0.0;
	int i, test = 0;

	/* calculating utilization bound for each task and then response time analysis once test fails */
	for(i = 0; i < no_of_tasks[index]; i++){
		utilization = ((float)(i+1))*(pow(2, (1/(float)(i+1))) - 1.0);
		temp += tasks[index][i][0]/minimum(tasks[index][i][1], tasks[index][i][2]);	

		/* performing response time analysis */	
		if(temp > utilization)
		{	
			ResponseTimeTest(index, i, flag);
			break;	
		}else
		{
			test++;	
		}
	}

	/* if all tasks in taskset meets UB test then schedulable, so incrementing counter */
	if(test == no_of_tasks[index])
	{
		if(flag==1)
			rm_count++;
		else
			dm_count++;
	}	
}

/***************** Rate Monotonic Algorithm *****************/
void RateMonotonic()
{
	int i, j;
	float utilization, mini, suff_value;    //to hold the utilization, min(deadline, period) and sufficient condition value
	int counter;
	sort(1);
	

	/* schedulability of each taskset */
	for(i = 0; i < task_sets; i++)
	{
		utilization = 0.0;
		counter = 0;
		/* Running EDF for each taskset */
		for(j = 0; j < no_of_tasks[i]; j++)
		{
			/* To calculate the minimum value among deadline and period of a task */
			if(tasks[i][j][1] == tasks[i][j][2])
				counter++;
			mini = minimum(tasks[i][j][1], tasks[i][j][2]);

			/* Calculating U = sum(ei/min(di, pi))*/
			utilization += tasks[i][j][0]/mini;
		}

		suff_value = no_of_tasks[i]*(pow(2, (1/no_of_tasks[i])) - 1);

		/* To check if each taskset is schedulable or not */
		if((utilization <= suff_value) && (counter == no_of_tasks[i]))
		{
			if(printFlag == 1)
				printf("Task set %d is schedulable using Rate Monotonic\n", i+1);
			rm_count++;
		}
		else
		{
			EffectiveUtest(i, 1);
		}
	}
}

/***************** Deadline Monotonic Algorithm *****************/
void DeadlineMonotonic()
{
	int i, j;
	float utilization, mini, suff_value;
	int counter=0;
	sort(2);

	/* schedulability of each taskset */
	for(i = 0; i < task_sets; i++)
	{
		utilization = 0.0;
		counter=0;
		/* Running EDF for each taskset */
		for(j = 0; j < no_of_tasks[i]; j++)
		{
			if(tasks[i][j][1] == tasks[i][j][2])
				counter++;
			/* To calculate the minimum value among deadline and period of a task */
			mini = minimum(tasks[i][j][1], tasks[i][j][2]);

			/* Calculating U = sum(ei/min(di, pi))*/
			utilization += tasks[i][j][0]/mini;
		}

		suff_value = no_of_tasks[i]*(pow(2, (1/no_of_tasks[i])) - 1);

		/* To check if each taskset is schedulable or not */
		if(utilization <= suff_value && counter == no_of_tasks[i]){
			if(printFlag == 0)
				printf("Task set %d is schedulable using Rate Monotonic\n", i+1);
			dm_count++;
		}
		else
			EffectiveUtest(i, 0);
	}
}

/* To generate uniform distribution of utilizations */
float *UUniFast(int n, float u)
{	
	int i;
	float *utilizations = calloc(n, sizeof(float));
	float sumU = u, nextSumU;
	for(i = 0; i < n - 1; i++){
		nextSumU = sumU * pow( (double)rand()/RAND_MAX , 1.0 / (float)(n - i));
		utilizations[i] = sumU - nextSumU;
		sumU = nextSumU;
	}
	utilizations[n-1] = sumU;
	return utilizations;
}

/* To generate random integer values within given range */
int randr(int mino, int maxo)
{
       double scaled = (double)rand()/RAND_MAX;
       return (maxo - mino + 1) * scaled + mino;
}

/* To generate random period values */
void periodGenerate(int *res, int n)
{
	int i;
	for (i=0;i<n;i++)
	{
		if(i < n / 3)
		{
			res[i] = randr(range[0],range[1]);
		}

		else if((i > n / 3) && (i < 2 * n / 3))
		{
			res[i] = randr(range[1],range[2]);
		}
		else
		{
			res[i] = randr(range[2],range[3]);
		}
	}
}

/* To generate float deadline values within given range */
float deadlineGenerate(float w, int p){
	return (p - w + 1) * drand48() + w;
}

/***************** Main Program *****************/
int main()
{
	int i, j, k, task, *periods, l;
	srand(time(NULL));
	srand48(time(NULL));
	float *uti;
	FILE *fp = fopen("./input.txt", "r");

	/* To scan the number of task sets */
	fscanf(fp, "%d", &task_sets);

	/* To allocate the memory for task sets */
	tasks = (float ***)calloc(task_sets,sizeof(float **));
	no_of_tasks = (int *)calloc(task_sets,sizeof(int));


	/* Allocating memory and reading the tasks info for all tasksets */
	for(i = 0;i < task_sets; i++)
	{	
		/* To scan the number of tasks in each taskset */	
		fscanf(fp, "%d", &no_of_tasks[i]);

		/* To allocate memory for tasks in each taskset */
		tasks[i] = (float **)calloc(no_of_tasks[i],sizeof(float *));

		/* To allocate memory for task parameters */
		for(j = 0; j < no_of_tasks[i]; j++)
		{
			tasks[i][j] = (float *)calloc(3,sizeof(float));
			/* To scan the parameters of all tasks in each taskset */
			for(k = 0; k < 3; k++)
			{
				fscanf(fp, "%f", &tasks[i][j][k]);	
			}
		}
	}

	/* scheduling algorithms for task sets read from input file */
	EDF();
	RateMonotonic();
	DeadlineMonotonic();

	/* Free of memory */
	for(i=0;i<task_sets;i++)
	{		
		free(tasks[i]);
	}
	free(tasks);
	free(no_of_tasks);

	/* 2nd task - random task sets */
	task_sets = 5000;
	printFlag = 1;

	printf("\n\n\n");

	/* schedulable tests for four different cases */
	for(l = 0; l < 4; l++)
	{
		/* 10 tasks when l = 0 and 2 */
		if(l%2 == 0)
		{
			task = 10;
		}
		/* 25 tasks when l = 0 and 2 */
		else
			task = 25;

		/* Generating 5000 tasksets for each utilization point on graph */
		for(k = 0;k < 10; k++)
		{
			tasks = (float ***)calloc(task_sets,sizeof(float **));         //holds the task sets data
			no_of_tasks = (int *)calloc(task_sets,sizeof(int));			//holds the number of tasks in each task set

			/* counters for each scheduling algorithm */
			edf_count = 0;
			rm_count = 0;
			dm_count = 0;

			/* Generating parameters for each task set */
			for(j = 0; j < task_sets; j++)
			{
				uti = UUniFast(task, utili[k]);        //generating utilizations using UUnifast algorithm
				periods = calloc(task,sizeof(int));		
				periodGenerate(periods, task);		//generating periods
				
				no_of_tasks[j] = task;
				tasks[j] = (float **)calloc(no_of_tasks[j],sizeof(float *));

				/* assigning parameters to tasks and deadline generation within required range of WCET and Periods */
				for(i = 0; i < no_of_tasks[j]; i++)
				{
					tasks[j][i] = (float *)calloc(3,sizeof(float));
					tasks[j][i][0] = uti[i] * periods[i];
					tasks[j][i][2] = (float)periods[i];
					switch(l)
					{
						case 0:
						case 1:
							tasks[j][i][1] = deadlineGenerate(tasks[j][i][0], tasks[j][i][2]);    //deadline range- (Ci, Ti)
							break;
						case 2:
						case 3:
							tasks[j][i][1] = deadlineGenerate(tasks[j][i][0] + (tasks[j][i][2] - tasks[j][i][0]) / 2, tasks[j][i][2]);		//deadline range- (Ci+(Ti-Ci)/2, Ti)
							break;
						default: asm volatile("nop");			
					}
				}
				free(periods);
			}

			/* calling scheduling algorithms */
			EDF();
			RateMonotonic();
			DeadlineMonotonic();	

			/* printing the number of tasks schedulable out of 5000 for each utilization by each algorithm */
			printf("Number of tasks schedulable using EDF = %d\n", edf_count);
			printf("Number of tasks schedulable using Rate Monotonic algorithm = %d\n", rm_count);
			printf("Number of tasks schedulable using Deadline Monotonic = %d\n", dm_count);

			/* Free of memory */
			for(i=0;i<task_sets;i++)
			{		
				free(tasks[i]);
			}
			free(tasks);
			free(no_of_tasks);
		}
		printf("\n\n\n");
	}
	return 0;
}