//
// Created by Admin on 12.05.2020.
//

#include <cstdio>
#include <pthread.h>
#include <cstdlib>
#include <cmath>
#include <mpi.h>
#include <iostream>

int iterCounter = 4;
int L = 500;
int *tl;
int nextPosition;

double globalRes = 0; // для сборки окончательного результата
int tasksCount = 28000;
int processTasksCount;
int iterTaskCount;

int size;
int rank;

// объекты типа "описатель потока"
pthread_t threads[2];
// mutex == mutual exclusion
pthread_mutex_t mutex;

void errorHandler(int error_code, const char *argv);
void initList(int *taskList, int procTaskCount, int iterCount);
void createThreads();
void *doTasks(void *);
void *sendTask(void *);

void errorHandler(int error_code, const char *argv) {
    errno = error_code;
    perror(argv);
    MPI_Finalize();
    std::exit(errno);
}

int main(int argc, char **argv) {
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (MPI_THREAD_MULTIPLE != provided) {
        std::cerr << "Required level was not provided\n" << std::endl;
        MPI_Finalize();
        std::exit(0);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //инициализация мьютекса
    int err_code = pthread_mutex_init(&mutex, nullptr);
    if (0 != err_code) {
        errorHandler(err_code, "pthread_mutex_init");
    }

    //распределяем задания на каждый процесс, зависит от rank & size
    processTasksCount = tasksCount / size;
    if (rank < tasksCount % size) {
        processTasksCount = tasksCount / size + 1;
    } else {
        processTasksCount = tasksCount / size;
    }

    tl = (int *)malloc(processTasksCount * sizeof(int));

    double startTime = MPI_Wtime();
    createThreads();
    double endTime = MPI_Wtime();
    double time = endTime - startTime;

    double generalTime;
    MPI_Reduce(&time, &generalTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if(rank == 0) {
        printf("Time:%f\n", generalTime);
    }

    pthread_mutex_destroy(&mutex);
    free(tl);
    MPI_Finalize();
    return 0;
}

void createThreads(){
    int err_code;

    pthread_attr_t attr;
    if (0 != (err_code = pthread_attr_init(&attr))) {
        errorHandler(err_code, "pthread_attr_init");
    }

    if (0 != (err_code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE))) {
        errorHandler(err_code, "pthread_attr_setdetachstate");
    }

    if (0 != (err_code = pthread_create(&threads[0], &attr, doTasks, nullptr))) {
        errorHandler(err_code, "pthread_create");
    }

    if (0 != (err_code = pthread_create(&threads[1], &attr, sendTask, nullptr))) {
        errorHandler(err_code, "pthread_create");
    }

    pthread_attr_destroy(&attr);

    if (0 != (err_code = pthread_join(threads[0], nullptr))) {
        errorHandler(err_code, "pthread_join");
    }

    if (0 != (err_code = pthread_join(threads[1], nullptr))) {
        errorHandler(err_code, "pthread_join");
    }
}

void initList(int *taskList, int procTaskCount, int iterCount) {
    for(int i = 0; i < procTaskCount; i++) {
        // определим Tl[i].repeatNum
        taskList[i] = abs(50 - i % 100) * abs(rank - (iterCount % size)) * L;
    }
}

// обрабатывает задания и, когда задания закончились, обращается к другим компьютерам за добавкой к работе
void *doTasks(void *args) {
    MPI_Status st;
    int iterationCompletedTasksNum;
    double startIteration_Time;

    // обработка заданий из списка процессом
    int request;
    int currListNum = 0; //счетчик глобальных итераций
    while(currListNum != iterCounter) {
        initList(tl, processTasksCount, currListNum);
        iterationCompletedTasksNum = 0;
        nextPosition = 0;

        // засечка сделанная в самом начале выполнения итерации
        startIteration_Time = MPI_Wtime();

        //выполняем задания которые есть
        iterTaskCount = processTasksCount;
        while(iterTaskCount != 0) {
            pthread_mutex_lock(&mutex);

            int weight = tl[nextPosition];
            nextPosition++;
            iterTaskCount--;

            pthread_mutex_unlock(&mutex);

            for(int i = 0; i < weight; i++) {
                //sin, чтобы накопленная сумма не была слишком большой
                globalRes += sin(i);
            }
            iterationCompletedTasksNum++;
        }

        // обращается к другим процессам за новыми заданиями для себя
        for(int i = 0; i < size; i++){
            request = 1;

            if((rank + i) % size != rank) {
                while(true) {
                    // модель: по несколько заданий от нескольких процессов
                    MPI_Send(&request, 1, MPI_INT, (rank + i) % size, 0, MPI_COMM_WORLD);
                    int response;
                    MPI_Recv(&response, 1, MPI_INT, (rank + i) % size, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    // iterTaskCount <= 100 -> (-1)
                    if(response != -1) {
                        // размера кол-ва заданий, заполнен весами
                        int *response_tasks = (int *)malloc(sizeof(int) * response);
                        MPI_Recv(response_tasks, response, MPI_INT, (rank + i) % size, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                        for(int j = 0; j < response; j++) {
                            for(int k = 0; k < response_tasks[j]; k++) {
                                //sin, чтобы накопленная сумма не была слишком большой
                                globalRes += sin(k);
                            }

                            // процесс на текущей итерации выполнил еще одно задание
                            iterationCompletedTasksNum++;
                        }

                        free(response_tasks);
                    } else
                        break;
                }
            }
        }
        //обработали все задания

        // засечка сделанная процессом p  в конце выполнения итерации k
        // сразу после того, как он закончил работать над заданиями и перестал брать задания у других
        double endIteration_Time = MPI_Wtime();

        //время активной работы процесса p на данной итерации
        double iterationTimeProc = endIteration_Time - startIteration_Time;
        printf("Process#%d | TasksIterationCount:%d | IterationTime:%f\n", rank, iterationCompletedTasksNum, iterationTimeProc);

        double m, n;
        MPI_Allreduce(&iterationTimeProc, &m, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        MPI_Allreduce(&iterationTimeProc, &n, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

        if(rank == 0) {
            // после каждой итерации нужно посчитать время дисбаланса
            printf("Imbalance:%f\n", m - n);
            printf("Share of imbalance:%.2f\n", ((m - n) / m) * 100);
        }

        // прибавляем к общему текущую итерацию
        double globalResIteration;
        MPI_Allreduce(&globalRes, &globalResIteration, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        if(rank == 0)
            printf("GlobalRes iteration:%.3f\n", globalResIteration);

        currListNum++;
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // request 0 отправляем rank'у, когда currListNum == iterCounter
    request = 0;
    MPI_Send(&request, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);

    return nullptr;
}

// ожидает запросов о работе от других MPI-процессов
void *sendTask(void *args) {
    MPI_Status status;
    int request;
    int response;

    while(true) {
        // получает этот реквест от любого процесса ANY_SOURCE [send.tag = 0 в doTasks]
        MPI_Recv(&request, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);

        if(request == 0)
            break;

        //
        pthread_mutex_lock(&mutex);
        int *sendTasks = nullptr;
        // сверка количества оставшихся заданий у процесоов
        if(iterTaskCount > 100) {
            response = 50;

            sendTasks = (int *)malloc(sizeof(int) * response);
            for(int i = 0; i < response; i++){
                // заполняем массив с весом задния
                sendTasks[i] = tl[nextPosition];
                nextPosition++;
                iterTaskCount--;
            }
        } else {
            response = -1;
        }

        pthread_mutex_unlock(&mutex);

        //отправляем длину
        MPI_Send(&response, 1, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
        if(response > 0) {
            // отправляем сам массив с весом
            MPI_Send(sendTasks, response, MPI_INT, status.MPI_SOURCE, 2, MPI_COMM_WORLD);
            free(sendTasks);
        }

    }

    return nullptr;
}

