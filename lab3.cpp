#include "lab3.h"

#include <windows.h> 
#include <vector>
#include <string>

using namespace std;

#define NUMBER_TIME_LINES 6
#define NUMBER_THREADS 11
#define NUM_Q 3

HANDLE semaphore[NUMBER_TIME_LINES]; // семафоры для запуска потоков на отрезках времени
HANDLE semaphoreSeq[4]; // семафоры для точной синхронизации degh
HANDLE semaphoreTimeOut;

HANDLE mutex;

struct Data_Thread
{
	int start_time_stream;
	int number_time_lines;
	char nameT;
};

Data_Thread data_Thread[NUMBER_THREADS];

// возвращает позицию sequential потока в списке degh
int seq_position(char name)
{
	int pos = -1;
	const char* seq = lab3_sequential_threads();

	for (int i = 0; i < 4; i++)
	{
		if (name == seq[i])
			pos = i;
	}

	return pos;
}

// sequential поток из списка degh, вызывая это со своим pos и текущим номером отрезка времени T, разрешает работать следующему потоку из списка degh

void seq_next(int T, int cur_pos)
{
	int next_pos;

	switch (T)
	{
	case 0:
		ReleaseSemaphore(semaphoreSeq[(cur_pos + 1) % 2], 1, 0);
		break;
	case 1:
		ReleaseSemaphore(semaphoreSeq[(cur_pos + 1) % 2], 1, 0);
		break;
	case 2:
		ReleaseSemaphore(semaphoreSeq[(cur_pos + 1) % 4], 1, 0);
		break;
	case 3:
		next_pos = (cur_pos + 1) % 4;
		if (next_pos == 0)
			++next_pos;
		ReleaseSemaphore(semaphoreSeq[next_pos], 1, 0);
		break;
	case 4:
		ReleaseSemaphore(semaphoreSeq[3], 1, 0);
		break;
	}
}


DWORD WINAPI threads(LPVOID param)
{
	Data_Thread dt = data_Thread[(int)param];
	int seq_pos = seq_position(dt.nameT); // узнает свой pos среди degh (-1 для не degh)

	for (int i = dt.start_time_stream; i < dt.number_time_lines + dt.start_time_stream; i++)
	{
		for (int j = 0; j < NUM_Q; j++)
		{
			WaitForSingleObject(semaphore[i], INFINITE); 
			if (seq_pos >= 0) // если sequential
				WaitForSingleObject(semaphoreSeq[seq_pos], INFINITE); // то ещё нужно разрешение от предыдущего потока из списка degh
			WaitForSingleObject(mutex, INFINITE);
			if (i == 2)
			{
				if (seq_pos == 0)
					cout << lab3_sequential_threads() << flush;
			}
			else
			{
				cout << dt.nameT << flush;
			}
			ReleaseMutex(mutex);
			computation();

			if (seq_pos >= 0) // sequential после своей работы дает разрешение следующему sequential по списку
				seq_next(i, seq_pos);
		}

		ReleaseSemaphore(semaphoreTimeOut, 1, 0);
	}
	return 0;
}

void func_close()
{
	for (int i = 0; i < NUMBER_TIME_LINES; i++)
	{
		CloseHandle(semaphore[i]);
	}

	CloseHandle(mutex);
}

void release_time_line()
{
	for (int i = 0; i < NUMBER_TIME_LINES; i++)
	{
		switch (i)
		{
			
		case 0:
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 4, 0);
			for (int j = 0; j < 4; j++) // ждем окончания всех потоков на отрезке
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		case 1:
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 4, 0);
			for (int j = 0; j < 4; j++)
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		case 2:
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 4, 0);
			for (int j = 0; j < 4; j++)
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		case 3:
			ReleaseSemaphore(semaphoreSeq[1], 1, 0); // на T3 потока d больше нет, надо передать сигнал sequential для e из списка degh
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 4, 0);
			for (int j = 0; j < 4; j++)
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		case 4:
			ReleaseSemaphore(semaphoreSeq[3], 1, 0); // на T4 остался только h из списка degh, ему и надо передать сигнал sequential
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 3, 0);
			for (int j = 0; j < 3; j++)
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		case 5:
			for (int j = 0; j < NUM_Q; j++)
				ReleaseSemaphore(semaphore[i], 1, 0);
			for (int j = 0; j < 1; j++)
				WaitForSingleObject(semaphoreTimeOut, INFINITE);
			break;
		}
		computation();
	}
}

// Создание семафоров

void semaphore_create()
{
	for (int i = 0; i < NUMBER_TIME_LINES; i++)
	{
		string name_sem = "Semaphore";
		name_sem += char(49 + i);
		semaphore[i] = CreateSemaphore(NULL, 0, NUMBER_THREADS * NUM_Q, name_sem.c_str());
	}

	for (int i = 0; i < 4; i++)
	{
		string name_sem = "SemaphoreSeq";
		name_sem += char(49 + i);

		if (i == 0)
			semaphoreSeq[i] = CreateSemaphore(NULL, 1, NUMBER_THREADS * NUM_Q, name_sem.c_str());
		else
			semaphoreSeq[i] = CreateSemaphore(NULL, 0, NUMBER_THREADS * NUM_Q, name_sem.c_str());
	}

	semaphoreTimeOut = CreateSemaphore(NULL, 0, NUMBER_THREADS * NUM_Q, "semaphoreTimeOut");
	mutex = CreateMutex(NULL, 0, "IO_mutex");
}

void add_Data_Thread(string input, int start = 0)
{
	Data_Thread dt;
	for (int i = 0; i < NUMBER_THREADS; i++)
	{
		switch (i)
		{
		case 0://a

			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 0;
			break;
		case 1://b

			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 1;
			break;
		case 2://c

			dt.number_time_lines = 2;
			dt.nameT = input[i];
			dt.start_time_stream = 0;
			break;
		case 3://d

			dt.number_time_lines = 3;
			dt.nameT = input[i];
			dt.start_time_stream = 0;
			break;
		case 4://e

			dt.number_time_lines = 4;
			dt.nameT = input[i];
			dt.start_time_stream = 0;
			break;
		case 5://f

			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 3;
			break;
		case 6://g

			dt.number_time_lines = 2;
			dt.nameT = input[i];
			dt.start_time_stream = 2;
			break;
		case 7://h

			dt.number_time_lines = 3;
			dt.nameT = input[i];
			dt.start_time_stream = 2;
			break;
		case 8://i

			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 4;
			break;
		case 9://k

			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 4;
			break;
		case 10://m
			dt.number_time_lines = 1;
			dt.nameT = input[i];
			dt.start_time_stream = 5;
			break;
		}
		start++;
		data_Thread[i] = dt;
	}
}

unsigned int lab3_thread_graph_id()
{
	return 4;
}

const char* lab3_unsynchronized_threads()
{
	return "efgh";
}

const char* lab3_sequential_threads()
{
	return "degh";
}

int lab3_init()
{
	HANDLE Threads[NUMBER_THREADS];

	semaphore_create();
	string str = "abcdefghikm";
	add_Data_Thread(str);

	for (int i = 0; i < NUMBER_THREADS; i++)
	{
		Threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threads, (LPVOID)i, 0, NULL);
	}

	release_time_line();

	for (int i = 0; i < NUMBER_THREADS; i++)
	{
		WaitForSingleObject(Threads[i], INFINITE);
		CloseHandle(Threads[i]);
	}

	func_close();
	memset(data_Thread, 0, sizeof(data_Thread));

	return 0;
}
