#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define FRAME_SIZE 4096
#define FRAME_COUNT 250

struct TLB {
    char* processNames[250];
    int pageNumbers[250];
    int frameNumbers[250];
    int tail;
} TLB;

struct Memory {
    bool* freeFrameList;
    int freeFrameCount;
} memory;

struct Process {
    char* processName;
    int* accesList;
    int* pageTable;
    int accessCount;
    int pageCount;
    int processSize;
    int hitCount;
    int missCount;
    bool finished;
    int currentAccess;
    int dispatchCount;
};

struct Process processes[FRAME_COUNT];
int totalProcessCount = 0;

void readFileAndInitProcesses();
void initMemory();
void initProcess(char* processString);
void parceProcess(struct Process* p, char* processString);
void fillPageTableFor(struct Process* p);
int chooseFreeSpace();
void initialise(struct Process* p, int accessCount, int pageCount);
bool hasEnoughSpaceFor(struct Process* p);
void run();
bool isFinished();
void dispatchProcess(int turn);
void access(struct Process* p, int access);

void readFileAndInitProcesses() {
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen("tasks.txt", "r");
    if (fp == NULL)
        exit(-1);

    while ((read = getline(&line, &len, fp)) != -1) {
        initProcess(line);
    }

    fclose(fp);
    if (line)
        free(line);
}

void initMemory() {
    memory.freeFrameList = (bool*) malloc(sizeof(bool) * FRAME_COUNT);
    memory.freeFrameCount = FRAME_COUNT;
    for(int i = 0; i < FRAME_COUNT; i++)
        memory.freeFrameList[i] = true;
}

void initProcess(char* processString) {
    struct Process p;
    parceProcess(&p, processString);
    if (!hasEnoughSpaceFor(&p)) {
        printf ("\n!!!Memory doesn\'t have enough space for process %s!!!\n\n", p.processName);
        return;
    }
    fillPageTableFor(&p);

    processes[totalProcessCount++] = p;
}

void parceProcess(struct Process* p, char* processString) {
    char* copyString = (char*) malloc(strlen(processString) + 1);
    strcpy(copyString, processString);

    char* processName = strtok(copyString, " ");

    int pageCount = 0;
    int accessCount = 0;

    struct stat st;
    stat(processName, &st);
    off_t size = st.st_size;

    p->processSize = (int)size;
    pageCount = ((int)size / FRAME_SIZE) + 1;
    
    p->processName = processName;

    char* token;
    while ((token = strtok(NULL, " ")) != NULL) {
        int access = atoi(token);
        accessCount++;

        if (access / FRAME_SIZE >= pageCount) {
            printf("exceeded program segment\n");
            exit(-1);
        }
    }

    initialise(p, accessCount, pageCount);

    strcpy(copyString, processString);
    strtok(copyString, " ");
    int accessIndex = 0;
    while ((token = strtok(NULL, " ")) != NULL) {
        int access = atoi(token);
        p->accesList[accessIndex++] = access;
    }

    p->accessCount = accessCount;
    p->pageCount = pageCount;
}

void fillPageTableFor(struct Process* p) {
    int tableIndex = 0;

    for (int i = 0; i < p->pageCount; i++) {
        int choosen = chooseFreeSpace();
        p->pageTable[i] = choosen;
        memory.freeFrameCount--;
        memory.freeFrameList[choosen] = false;
    }
}

int chooseFreeSpace() {
    int arr[FRAME_COUNT];
    int index = 0;
    
    for (int i = 0; i < FRAME_COUNT; i++)
        if (memory.freeFrameList[i])
            arr[index++] = i;

    return rand() / (RAND_MAX / (index + 1));
}

void initialise(struct Process* p, int accessCount, int pageCount) {
    p->accesList = (int*) malloc(sizeof(int) * accessCount);
    p->pageTable = (int*) malloc(sizeof(int) * pageCount);
    p->missCount = 0;
    p->hitCount = 0;
    p->currentAccess = 0;
    p->dispatchCount = 0;
    p->finished = false;
}

bool hasEnoughSpaceFor(struct Process* p) {
    return p->pageCount <= memory.freeFrameCount;
}

void run() {
    int turn = 0;
    while(!isFinished()) {
        dispatchProcess(turn);
        turn = (turn + 1) % totalProcessCount;
    }
}

bool isFinished() {
    for(int i = 0; i < totalProcessCount; i++)
        if (!processes[i].finished)
            return false;

    return true;
}

void dispatchProcess(int turn) {
    struct Process* p = &processes[turn];

    if (p->finished)
        return;

    p->dispatchCount++;

    for(int m = 0; m < 5; m++) {
        
        access(p, p->accesList[p->currentAccess]);

        p->currentAccess++;
        if (p->currentAccess >= p->accessCount) {
            p->finished = true;
            return;
        }
    }
}

void access(struct Process* p, int access) {
    int pageNumber = access / FRAME_SIZE;
    for (int i = 0; i < TLB.tail; i++)
        if (strcmp(TLB.processNames[i], p->processName) == 0 
            && TLB.pageNumbers[i] == pageNumber) {
                p->hitCount++;
                return;
            }

    TLB.processNames[TLB.tail] = p->processName;
    TLB.pageNumbers[TLB.tail] = pageNumber;
    TLB.frameNumbers[TLB.tail] = p->pageTable[pageNumber];

    p->missCount++;

    TLB.tail++;
}

void printTables() {
    for (int processIndex = 0; processIndex < totalProcessCount; processIndex++) {
        struct Process* p = &processes[processIndex];
        int* pageTable = p->pageTable;
        printf("---------------------------------------------\n");
        printf("process %s\'s page table:\n\n", p->processName);
        for (int pageIndex = 0; pageIndex < p->pageCount; pageIndex++) {
            printf("page %d - frame %d\n", pageIndex, pageTable[pageIndex]);
        }
    }
}

void printInfos() {
    printf("---------------------------------------------\n");
    for(int i = 0; i < totalProcessCount; i++) {
        struct Process p = processes[i];

        printf("process name: %s, dispatch: %d, hit: %d, miss: %d\n", p.processName, p.dispatchCount, p.hitCount, p.missCount);
    }
}

int main() {
    srand(time(0));
    initMemory();
    readFileAndInitProcesses();
    printTables();
    TLB.tail = 0;
    run();
    printInfos();
    return 0;
}