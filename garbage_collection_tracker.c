#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "jni.h"
#include "jvmti.h"

// global jvmti pointer
static jvmtiEnv *jvmti;
// tracks garbage count
static int garbage_count;
static jrawMonitorID lock;

// global variable to calculate heap size
static long total_heap_size;
static time_t garbage_collection_start;
static time_t garbage_collection_stop;
// global variable to hold passed option from user
static char *parsed_options[2];
// divider divides the heap size to convert bytes to either KB, MB or GB
static long divider = 1;

static FILE *fpw;

/**
 * @brief Resets the global variables
 *
 */
static void reset()
{
  total_heap_size = 0;
}

/**
 * @brief This function checks the options passed from command line
 *
 * @param options options passed during the start
 */
static void parse_options(char *options)
{
  printf("Options received is: %s \n", options);
  char *token = strtok(options, ",");
  int token_counter = 0;
  while (token != NULL)
  {
    if (token_counter + 1 > 2)
    {
      fprintf(stderr, "Error; wrong option passed, token greater than 2 \n");
      break;
    }
    parsed_options[token_counter++] = token;
    token = strtok(NULL, ",");
  }
  if (strcasecmp(parsed_options[1], "KB") == 0)
  {
    divider = 1000;
  }
  else if (strcasecmp(parsed_options[1], "MB") == 0)
  {
    divider = 1000000;
  }
  else if (strcasecmp(parsed_options[1], "GB") == 0)
  {
    divider = 1000000000;
  }
  else
  {
    fprintf(stderr, "Error; wrong option passed %s \n", parsed_options[1]);
  }
}

/**
 * @brief heap iteration triggers this function
 *
 * @param class_tag
 * @param size
 * @param tag_ptr
 * @param user_data
 * @return jvmtiIterationControl
 */
static jvmtiIterationControl JNICALL
heapObject(jlong class_tag, jlong size, jlong *tag_ptr, void *user_data)
{
  total_heap_size = size + total_heap_size;
  return JVMTI_ITERATION_CONTINUE;
}

/**
 * @brief waits for garbage collection to complete and then
 * counts the heap size
 *
 * @param jvmti
 * @param jni
 * @param p
 */
static void JNICALL
thread_worker(jvmtiEnv *jvmti, JNIEnv *jni, void *p)
{
  jvmtiError err;
  while (1)
  {
    err = (*jvmti)->RawMonitorEnter(jvmti, lock);
    if (err != JVMTI_ERROR_NONE)
    {
      fprintf(stderr, "ERROR: RawMonitorEnter failed, err=%d\n", err);
      return;
    }
    while (garbage_count == 0)
    {
      err = (*jvmti)->RawMonitorWait(jvmti, lock, 0);
      if (err != JVMTI_ERROR_NONE)
      {
        fprintf(stderr, "ERROR: RawMonitorWait failed, err=%d\n", err);
        (*jvmti)->RawMonitorExit(jvmti, lock);
        return;
      }
    }
    garbage_count = 0;

    (*jvmti)->RawMonitorExit(jvmti, lock);

    /* Iterate over the heap and count */
    err = (*jvmti)->IterateOverHeap(jvmti, JVMTI_HEAP_OBJECT_EITHER,
                                    &heapObject, NULL);
    fprintf(fpw, "GC pause time: %.2f s\nTotal Heap Size: %li %s\n", difftime(garbage_collection_stop, garbage_collection_start), total_heap_size / divider, parsed_options[1]);
    reset();
  }
}

/**
 * @brief Creates a new thread
 *
 * @param env
 * @return jthread
 */
static jthread
alloc_thread(JNIEnv *env)
{
  jclass thrClass;
  jmethodID cid;
  jthread res;

  thrClass = (*env)->FindClass(env, "java/lang/Thread");
  cid = (*env)->GetMethodID(env, thrClass, "<init>", "()V");
  res = (*env)->NewObject(env, thrClass, cid);
  return res;
}

/**
 * @brief this function gets triggered on VM initialization
 *
 * @param jvmti
 * @param env
 * @param thread
 */
static void JNICALL
vm_init(jvmtiEnv *jvmti, JNIEnv *env, jthread thread)
{
  jvmtiError err;
  printf(" VMInitialization started...\n");

  err = (*jvmti)->RunAgentThread(jvmti, alloc_thread(env), &thread_worker, NULL,
                                 JVMTI_THREAD_MAX_PRIORITY);
  if (err != JVMTI_ERROR_NONE)
  {
    fprintf(stderr, "ERROR: RunAgentThread failed, err=%d\n", err);
  }

  fpw = fopen(parsed_options[0], "w");

  if (fpw == NULL)
  {
    fprintf(stderr, "ERROR: Error opening the file \n");
    exit(1);
  }
  err = (*jvmti)->IterateOverHeap(jvmti, JVMTI_HEAP_OBJECT_EITHER,
                                  &heapObject, NULL);
  fprintf(fpw, "Initial Heap Size: %li %s\n", total_heap_size / divider, parsed_options[1]);
  reset();
}

/**
 * @brief This function gets called on vm death event
 *
 * @param jvmti_env
 * @param jni_env
 */
static void JNICALL
vm_death(jvmtiEnv *jvmti_env,
         JNIEnv *jni_env)
{
  printf("VM DEATH");
  jvmtiError err;
  fclose(fpw);
}

/**
 * @brief This function gets triggered when garbage collection is started
 *
 * @param jvmti_env
 */
static void JNICALL
gc_start(jvmtiEnv *jvmti_env)
{
  time(&garbage_collection_start);
  fprintf(fpw, "GC started: %s", ctime(&garbage_collection_start));
}

/**
 * @brief This function is triggered when garbage collection is finished
 *
 * @param jvmti_env
 */
static void JNICALL
gc_finish(jvmtiEnv *jvmti_env)
{
  jvmtiError err;
  err = (*jvmti)->RawMonitorEnter(jvmti, lock);
  if (err != JVMTI_ERROR_NONE)
  {
    fprintf(stderr, "ERROR: RawMonitorEnter failed, err=%d\n", err);
  }
  else
  {
    garbage_count++;
    err = (*jvmti)->RawMonitorNotify(jvmti, lock);
    if (err != JVMTI_ERROR_NONE)
    {
      fprintf(stderr, "ERROR: RawMonitorNotify failed, err=%d\n", err);
    }
    err = (*jvmti)->RawMonitorExit(jvmti, lock);
    time(&garbage_collection_stop);
    fprintf(fpw, "GC stopped: %s", ctime(&garbage_collection_stop));
  }
}

/**
 * @brief This function is the entry point when and triggered when loaded
 *
 * @param vm
 * @param options
 * @param reserved
 * @return JNIEXPORT
 */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
  jint rc;
  jvmtiError err;
  jvmtiCapabilities capabilities;
  jvmtiEventCallbacks callbacks;

  rc = (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION);
  if (rc != JNI_OK)
  {
    fprintf(stderr, "ERROR: Unable to create jvmtiEnv, GetEnv failed, error=%d\n", rc);
    return -1;
  }

  err = (*jvmti)->GetCapabilities(jvmti, &capabilities);
  if (err != JVMTI_ERROR_NONE)
  {
    fprintf(stderr, "ERROR: GetCapabilities failed, error=%d\n", err);
  }

  /* Parse any options supplied on command line */
  parse_options(options);

  capabilities.can_generate_garbage_collection_events = 1;
  capabilities.can_tag_objects = 1;
  err = (*jvmti)->AddCapabilities(jvmti, &capabilities);
  if (err != JVMTI_ERROR_NONE)
  {
    fprintf(stderr, "ERROR: AddCapabilities failed, error=%d\n", err);
    return -1;
  }

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.VMInit = &vm_init;
  callbacks.GarbageCollectionStart = &gc_start;
  callbacks.GarbageCollectionFinish = &gc_finish;
  callbacks.VMDeath = &vm_death;
  (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));
  (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                     JVMTI_EVENT_VM_INIT, NULL);
  (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                     JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);
  (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                     JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);

  err = (*jvmti)->CreateRawMonitor(jvmti, "lock", &lock);
  if (err != JVMTI_ERROR_NONE)
  {
    fprintf(stderr, "ERROR: Unable to create raw monitor: %d\n", err);
    return -1;
  }
  return 0;
}

/**
 * @brief This gets called on unload
 *
 * @param vm
 * @return JNIEXPORT
 */
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm)
{
  if (fpw != NULL)
  {
    fclose(fpw);
  }
}
