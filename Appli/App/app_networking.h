#define PORT 8080 // needs a better name

#define SOCKET_TIMEOUT_MS       1000

#define CHILD_TASK_STACK_SIZE   2048

#define MIRROR_THREAD_PRIO      20

#define DISPLAY_THREAD_PRIO     10

#define CHILD_TASK_BUFFER_SIZE  16

void server_init();
void client_init();

