loci_module(std.concurrency std_concurrency.bc ""
	src/unix/condition_variable.loci
	src/unix/fiber.loci
	src/unix/message_queue.loci
	src/unix/mutex.loci
	src/unix/pthread_t.c
	src/unix/pthread_t.loci
	src/unix/pthread_cond_t.c
	src/unix/pthread_cond_t.loci
	src/unix/pthread_mutex_t.c
	src/unix/pthread_mutex_t.loci
	src/unix/thread.loci
	src/unix/thread_internal.loci
	${STD_CONTAINER_API_FILES}
	${STD_EVENT_API_FILES}
	${STD_MEMORY_API_FILES}
)
