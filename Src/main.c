/**********************************************************************************************************************
 * FILE DESCRIPTION
 * --------------------------------------------------------------------------------------------------------------------
 * Project:      OpenMophun
 * Component:    Desktop example
 * File:         main.c
 * Description:  Bare-metal-style MVM integration using desktop host services.
 *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include "MVM.h"
#include "FileApi.h"
#include "InputScript.h"
#include "Logger.h"
#include "SdlBackend.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**********************************************************************************************************************
 *  LOCAL DEFINES
 *********************************************************************************************************************/

#define MAX_STEPS_DEFAULT              (100000000U)
#define MAX_LOGGED_CALLS_DEFAULT       (0U)
#define VM_STEPS_PER_HOST_ITERATION    (5000U)
#define HOST_LOOP_DELAY_MS             (1U)

/**********************************************************************************************************************
 *  LOCAL TYPES
 *********************************************************************************************************************/

/**
 * @brief Describes parsed command-line options for the VM runner.
 */
typedef struct AppOptions
{
  const char *image_path;
  const char *profile_name;
  const char *input_script_path;
  const char *record_dir;
  const char *fixed_date_time;
  uint32_t max_steps;
  uint32_t max_logged_calls;
  uint32_t duration_ms;
} AppOptions;

/**
 * @brief Owns every resource used by one desktop VM instance.
 */
typedef struct AppContext
{
  MVM_InitConfig_t config;
  MVM_Instance_t *vm;
  void *vm_storage;
  void *auxiliary_pool;
  void *guest_memory_pool;
  void *code_cache;
  SdlBackend *platform;
  InputScript *input_script;
} AppContext;

/**********************************************************************************************************************
 *  LOCAL FUNCTION PROTOTYPES
 *********************************************************************************************************************/

static void print_usage(const char *program_name);
static int is_numeric_arg(const char *value);
static int parse_fixed_date_time(const char *value,
                                 uint16_t *year,
                                 uint8_t *month,
                                 uint8_t *day,
                                 uint8_t *hour,
                                 uint8_t *minute,
                                 uint8_t *second);
static int parse_options(int argc, char **argv, AppOptions *options);
static int get_date_time(void *context, MVM_TimeKind_t kind, MVM_DateTime_t *date_time);
static void print_stop_summary(MVM_Instance_t *vm);
static void deinit_application(AppContext *app);
static int init_application(AppContext *app, const AppOptions *options);

/**********************************************************************************************************************
 *  LOCAL CONSTANTS
 *********************************************************************************************************************/

static const MVM_DeviceProfile_t MVM_lProfiles[] =
{
  {
    .name = "SE_T310",
    .screen_width = 101U,
    .screen_height = 80U,
    .color_mode = 0x0008U,
    .sound_flags = 0x0099U,
    .system_flags = 0x0025U,
    .key_layout = 0x0001U,
    .frame_interval_ms = 16U,
    .device_id = (((uint32_t)1U << 16) | 3U),
    .supported_caps = MVM_DEVICE_CAP_VIDEO |
                      MVM_DEVICE_CAP_INPUT |
                      MVM_DEVICE_CAP_SOUND |
                      MVM_DEVICE_CAP_COMM |
                      MVM_DEVICE_CAP_SYSTEM
  },
  {
    .name = "SE_T610",
    .screen_width = 128U,
    .screen_height = 160U,
    .color_mode = 0x0008U,
    .sound_flags = 0x0099U,
    .system_flags = 0x0025U,
    .key_layout = 0x0001U,
    .frame_interval_ms = 16U,
    .device_id = (((uint32_t)2U << 16) | 3U),
    .supported_caps = MVM_DEVICE_CAP_VIDEO |
                      MVM_DEVICE_CAP_INPUT |
                      MVM_DEVICE_CAP_SOUND |
                      MVM_DEVICE_CAP_COMM |
                      MVM_DEVICE_CAP_SYSTEM
  }
};

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/

int main(int argc, char **argv)
{
  AppContext app;
  AppOptions options;
  MVM_RetCode_t run_status;
  uint32_t start_ms;
  uint32_t elapsed_ms;
  uint32_t remaining_steps;
  uint32_t step_budget;
  uint32_t executed_steps;
  int exit_code;

  /* Reject an invalid command line before acquiring application resources. */
  if (!parse_options(argc, argv, &options))
  {
    return 1;
  }

  /* Initialize the desktop services and one isolated VM instance. */
  if (!init_application(&app, &options))
  {
    deinit_application(&app);
    return 1;
  }

  MVM_DumpVmgpSummary(app.vm);
  MVM_DumpVmgpImports(app.vm, 64U);
  SdlBackend_Present(app.vm, app.platform);

  start_ms = SdlBackend_GetTicksMs(app.platform);
  remaining_steps = options.max_steps;

  /* Execute one bounded VM batch per parent-loop iteration. */
  while ((MVM_GetState(app.vm) == MVM_STATE_READY || MVM_GetState(app.vm) == MVM_STATE_RUNNING) &&
         remaining_steps != 0U)
  {
    elapsed_ms = SdlBackend_GetTicksMs(app.platform) - start_ms;

    /* Apply scripted keyboard input before the next VM batch. */
    if (app.input_script)
    {
      SdlBackend_SetSyntheticButtons(app.platform,
                                     InputScript_GetButtonMask(app.input_script, elapsed_ms));
    }

    /* Stop when the desktop window requests shutdown. */
    if (SdlBackend_PumpEvents(app.vm, app.platform))
    {
      break;
    }

    step_budget = remaining_steps < VM_STEPS_PER_HOST_ITERATION
                    ? remaining_steps
                    : VM_STEPS_PER_HOST_ITERATION;
    run_status = MVM_RunFrame(app.vm, step_budget, &executed_steps);
    remaining_steps -= executed_steps;
    SdlBackend_Present(app.vm, app.platform);

    elapsed_ms = SdlBackend_GetTicksMs(app.platform) - start_ms;

    /* Stop on an execution failure, no progress, or a configured parent limit. */
    if (run_status != MVM_OK ||
        executed_steps == 0U ||
        (options.max_logged_calls != 0U && MVM_GetLoggedCalls(app.vm) >= options.max_logged_calls) ||
        (options.duration_ms != 0U && elapsed_ms >= options.duration_ms))
    {
      break;
    }

    SdlBackend_Delay(HOST_LOOP_DELAY_MS);
  }

  print_stop_summary(app.vm);
  exit_code = MVM_GetState(app.vm) == MVM_STATE_ERROR ? 2 : 0;
  deinit_application(&app);

  return exit_code;
} /* End of main */

/**********************************************************************************************************************
 *  LOCAL FUNCTIONS
 *********************************************************************************************************************/

/**
 * @brief Prints the command-line usage string.
 */
static void print_usage(const char *program_name)
{
  fprintf(stderr,
          "Usage: %s <decrypted.mpn> [profile_name] [max_steps] [max_logged_calls] "
          "[--duration-ms N] [--input-script PATH] [--record-dir DIR] "
          "[--fixed-date-time YYYY-MM-DDTHH:MM:SS]\n",
          program_name);
}

/**
 * @brief Checks whether one argument contains only decimal digits.
 */
static int is_numeric_arg(const char *value)
{
  const unsigned char *p;

  /* Reject null and empty values. */
  if (!value || !*value)
  {
    return 0;
  }

  p = (const unsigned char *)value;
  /* Validate every character before converting the argument. */
  while (*p != '\0')
  {
    /* Reject the first non-decimal character. */
    if (!isdigit(*p))
    {
      return 0;
    }
    ++p;
  }

  return 1;
}

/**
 * @brief Parses one fixed date/time option.
 */
static int parse_fixed_date_time(const char *value,
                                 uint16_t *year,
                                 uint8_t *month,
                                 uint8_t *day,
                                 uint8_t *hour,
                                 uint8_t *minute,
                                 uint8_t *second)
{
  unsigned parsed_year;
  unsigned parsed_month;
  unsigned parsed_day;
  unsigned parsed_hour;
  unsigned parsed_minute;
  unsigned parsed_second;

  /* Require every destination used by the parser. */
  if (!value || !year || !month || !day || !hour || !minute || !second)
  {
    return 0;
  }

  /* Parse the complete ISO-like date/time form. */
  if (sscanf(value,
             "%u-%u-%uT%u:%u:%u",
             &parsed_year,
             &parsed_month,
             &parsed_day,
             &parsed_hour,
             &parsed_minute,
             &parsed_second) != 6)
  {
    return 0;
  }

  /* Reject fields that cannot fit the public fixed-date types. */
  if (parsed_year > 65535u || parsed_month > 255u || parsed_day > 255u ||
      parsed_hour > 255u || parsed_minute > 255u || parsed_second > 255u)
  {
    return 0;
  }

  *year = (uint16_t)parsed_year;
  *month = (uint8_t)parsed_month;
  *day = (uint8_t)parsed_day;
  *hour = (uint8_t)parsed_hour;
  *minute = (uint8_t)parsed_minute;
  *second = (uint8_t)parsed_second;

  return 1;
}

/**
 * @brief Parses runner options from the command line.
 */
static int parse_options(int argc, char **argv, AppOptions *options)
{
  int arg_index;
  int numeric_count;

  /* Require the decrypted image path. */
  if (argc < 2)
  {
    print_usage(argv[0]);

    return 0;
  }

  options->image_path = argv[1];
  options->profile_name = NULL;
  options->input_script_path = NULL;
  options->record_dir = NULL;
  options->fixed_date_time = NULL;
  options->max_steps = MAX_STEPS_DEFAULT;
  options->max_logged_calls = MAX_LOGGED_CALLS_DEFAULT;
  options->duration_ms = 0u;

  arg_index = 2;
  numeric_count = 0;
  /* Parse each optional or positional runner argument once. */
  while (arg_index < argc)
  {
    const char *arg;

    arg = argv[arg_index];
    /* Parse the optional wall-clock run limit. */
    if (strcmp(arg, "--duration-ms") == 0)
    {
      /* Require a decimal value after the option. */
      if (++arg_index >= argc || !is_numeric_arg(argv[arg_index]))
      {
        print_usage(argv[0]);
        return 0;
      }
      options->duration_ms = (uint32_t)strtoul(argv[arg_index], NULL, 0);
    }
    /* Parse the optional scripted keyboard input. */
    else if (strcmp(arg, "--input-script") == 0)
    {
      /* Require a path after the option. */
      if (++arg_index >= argc)
      {
        print_usage(argv[0]);
        return 0;
      }
      options->input_script_path = argv[arg_index];
    }
    /* Parse the optional frame recording directory. */
    else if (strcmp(arg, "--record-dir") == 0)
    {
      /* Require a path after the option. */
      if (++arg_index >= argc)
      {
        print_usage(argv[0]);
        return 0;
      }
      options->record_dir = argv[arg_index];
    }
    /* Parse the deterministic certificate date/time. */
    else if (strcmp(arg, "--fixed-date-time") == 0)
    {
      /* Require a date/time string after the option. */
      if (++arg_index >= argc)
      {
        print_usage(argv[0]);
        return 0;
      }
      options->fixed_date_time = argv[arg_index];
    }
    /* Reject unsupported named options. */
    else if (arg[0] == '-' && arg[1] == '-')
    {
      fprintf(stderr, "Unknown option: %s\n", arg);
      print_usage(argv[0]);
      return 0;
    }
    /* Assign the remaining positional arguments in their documented order. */
    else
    {
      /* Treat the first non-numeric positional value as the profile name. */
      if (!options->profile_name && numeric_count == 0 && !is_numeric_arg(arg))
      {
        options->profile_name = arg;
      }
      /* Treat the first numeric positional value as the step limit. */
      else if (numeric_count == 0 && is_numeric_arg(arg))
      {
        options->max_steps = (uint32_t)strtoul(arg, NULL, 0);
        ++numeric_count;
      }
      /* Treat the second numeric positional value as the log-call limit. */
      else if (numeric_count == 1 && is_numeric_arg(arg))
      {
        options->max_logged_calls = (uint32_t)strtoul(arg, NULL, 0);
        ++numeric_count;
      }
      /* Reject ambiguous or extra positional values. */
      else
      {
        print_usage(argv[0]);
        return 0;
      }
    }
    ++arg_index;
  }

  return 1;
}

/**
 * @brief Prints the final VM execution summary.
 */
static void print_stop_summary(MVM_Instance_t *vm)
{
  MVM_State_t state;
  MVM_Err_t error;
  MVM_HeapStats_t heap_stats;

  state = MVM_GetState(vm);
  error = MVM_GetLastError(vm);
  fprintf(stdout,
          "=== stop ===\nsteps=%u pc=0x%08X logged_calls=%u state=%u error=%u\n",
          MVM_GetExecutedSteps(vm),
          MVM_GetProgramCounter(vm),
          MVM_GetLoggedCalls(vm),
          (unsigned)state,
          (unsigned)error);
  /* Print allocator diagnostics when the VM exposes them. */
  if (MVM_GetHeapStats(vm, &heap_stats) == MVM_OK)
  {
    fprintf(stdout,
            "heap_capacity=%u heap_soft_limit=%u heap_high_water=%u heap_live=%u heap_peak_live=%u heap_quarantine=%u "
            "allocations=%u frees=%u allocation_failures=%u invalid_frees=%u double_frees=%u "
            "tracker_overflows=%u reuses=%u soft_limit_fallbacks=%u\n",
            heap_stats.capacity_bytes,
            heap_stats.soft_limit_bytes,
            heap_stats.high_water_bytes,
            heap_stats.live_bytes,
            heap_stats.peak_live_bytes,
            heap_stats.quarantine_bytes,
            heap_stats.allocation_requests,
            heap_stats.free_requests,
            heap_stats.allocation_failures,
            heap_stats.invalid_free_requests,
            heap_stats.double_free_requests,
            heap_stats.tracker_overflows,
            heap_stats.reuse_count,
            heap_stats.soft_limit_fallbacks);
  }
}

/**
 * @brief Releases every resource owned by one desktop application instance.
 */
static int get_date_time(void *context, MVM_TimeKind_t kind, MVM_DateTime_t *date_time)
{
  time_t now;
  struct tm *calendar;

  (void)context;
  if (!date_time)
  {
    return 0;
  }

  now = time(NULL);
  calendar = kind == MVM_TIME_UTC ? gmtime(&now) : localtime(&now);
  if (!calendar)
  {
    return 0;
  }

  date_time->year = (uint16_t)(calendar->tm_year + 1900);
  date_time->month = (uint8_t)(calendar->tm_mon + 1);
  date_time->day = (uint8_t)calendar->tm_mday;
  date_time->hour = (uint8_t)calendar->tm_hour;
  date_time->minute = (uint8_t)calendar->tm_min;
  date_time->second = (uint8_t)calendar->tm_sec;
  date_time->weekday = (uint8_t)calendar->tm_wday;
  return 1;
} /* End of get_date_time */

static void deinit_application(AppContext *app)
{
  /* Allow cleanup after a partially completed initialization. */
  if (!app)
  {
    return;
  }

  /* Deinitialize the VM before releasing its parent-owned memory. */
  if (app->vm)
  {
    MVM_Deinit(app->vm);
    app->vm = NULL;
  }

  free(app->auxiliary_pool);
  free(app->guest_memory_pool);
  free(app->code_cache);
  free(app->vm_storage);
  app->auxiliary_pool = NULL;
  app->guest_memory_pool = NULL;
  app->code_cache = NULL;
  app->vm_storage = NULL;

  SdlBackend_StopRecording(app->platform);
  InputScript_Destroy(app->input_script);
  SdlBackend_Destroy(app->platform);
  app->input_script = NULL;
  app->platform = NULL;
}

/**
 * @brief Creates the desktop platform and one configured VM instance.
 */
static int init_application(AppContext *app, const AppOptions *options)
{
  MVM_MemReqs_t memory_requirements;
  const MVM_DeviceProfile_t *profile;
  MVM_RetCode_t retVal;
  uint16_t fixed_year;
  uint8_t fixed_month;
  uint8_t fixed_day;
  uint8_t fixed_hour;
  uint8_t fixed_minute;
  uint8_t fixed_second;
  uint32_t profile_index;

  /* Reject invalid integration arguments. */
  if (!app || !options)
  {
    return 0;
  }

  *app = (AppContext){0};
  memory_requirements = (MVM_MemReqs_t){0};

  profile = NULL;

  /* Select the requested profile, or use the first profile by default. */
  for (profile_index = 0U;
       profile_index < (uint32_t)(sizeof(MVM_lProfiles) / sizeof(MVM_lProfiles[0]));
       ++profile_index)
  {
    if (!options->profile_name || strcmp(MVM_lProfiles[profile_index].name, options->profile_name) == 0)
    {
      profile = &MVM_lProfiles[profile_index];
      break;
    }
  }

  /* Report the configured choices when the requested profile is unknown. */
  if (!profile)
  {
    fprintf(stderr, "Unknown device profile: %s\nAvailable profiles:", options->profile_name);

    /* Print every parent-owned profile name to help correct the command line. */
    for (profile_index = 0U;
         profile_index < (uint32_t)(sizeof(MVM_lProfiles) / sizeof(MVM_lProfiles[0]));
         ++profile_index)
    {
      fprintf(stderr, " %s", MVM_lProfiles[profile_index].name);
    }

    fprintf(stderr, "\n");
    return 0;
  }

  /* Create desktop I/O services for the selected device geometry. */
  app->platform = SdlBackend_Create(profile);
  /* Stop when the desktop backend cannot be created. */
  if (!app->platform)
  {
    return 0;
  }

  /* Load scripted keyboard input only when requested. */
  if (options->input_script_path)
  {
    app->input_script = InputScript_Load(options->input_script_path);
    /* Reject missing or malformed input scripts. */
    if (!app->input_script || InputScript_GetLastError(app->input_script)[0] != '\0')
    {
      fprintf(stderr,
              "Could not load input script: %s (%s)\n",
              options->input_script_path,
              InputScript_GetLastError(app->input_script));
      return 0;
    }
  }

  /* Start frame recording only when a destination was requested. */
  if (options->record_dir && !SdlBackend_StartRecording(app->platform, options->record_dir))
  {
    fprintf(stderr, "Could not start recording in: %s\n", options->record_dir);
    return 0;
  }

  /*
   * This is the complete integration descriptor. The parent owns the profile,
   * image path, filesystem API, platform callbacks, and both memory regions.
   */
  app->config.profile = *profile;
  app->config.image_path = options->image_path;
  app->config.file_api = &FileApi;
  app->config.log_level = MVM_LOG_LEVEL_INFO;
  app->config.services.context = app->platform;
  app->config.services.display_flush = SdlBackend_DisplayFlush;
  app->config.services.input_get_buttons = SdlBackend_InputGetButtons;
  app->config.services.audio_play = SdlBackend_AudioPlay;
  app->config.services.audio_stop = SdlBackend_AudioStop;
  app->config.services.get_ticks_ms = SdlBackend_GetTicks;
  app->config.services.get_date_time = get_date_time;
  app->config.services.get_random = NULL;
  app->config.services.log = Logger_Log;
  app->config.services.event = NULL;
  app->config.services.system_message = NULL;
  SdlBackend_SetLogLevel(app->platform, app->config.log_level);

  retVal = MVM_QueryMemory(&app->config, &memory_requirements);
  /* Validate the descriptor before allocating the runtime memory. */
  if (MVM_OK != retVal)
  {
    fprintf(stderr, "Could not query VM memory requirements. ret=%u\n", (unsigned)retVal);
    return 0;
  }

  app->vm_storage = malloc(MVM_GetInstanceStorageSize());
  app->auxiliary_pool = malloc(memory_requirements.auxiliary_pool_bytes);
  app->guest_memory_pool = malloc(memory_requirements.guest_memory_bytes);
  app->code_cache = memory_requirements.code_cache_bytes != 0U
                        ? malloc(memory_requirements.code_cache_bytes)
                        : NULL;
  app->config.runtime_pool = app->auxiliary_pool;
  app->config.runtime_pool_size = memory_requirements.auxiliary_pool_bytes;
  app->config.guest_memory_pool = app->guest_memory_pool;
  app->config.guest_memory_pool_size = memory_requirements.guest_memory_bytes;
  app->config.code_cache = app->code_cache;
  app->config.code_cache_size = memory_requirements.code_cache_bytes;
  /* Require every parent-owned memory region used by this integration. */
  if (!app->vm_storage || !app->auxiliary_pool || !app->guest_memory_pool ||
      (memory_requirements.code_cache_bytes != 0U && !app->code_cache))
  {
    fprintf(stderr, "Could not allocate VM instance memory.\n");
    return 0;
  }

  retVal = MVM_Init(app->vm_storage, MVM_GetInstanceStorageSize(), &app->config, &app->vm);
  /* Stop when the library cannot initialize inside the supplied memory. */
  if (MVM_OK != retVal)
  {
    fprintf(stderr,
            "Failed to initialize VMGP context. ret=%u auxiliary=%llu guest=%llu code=%llu\n",
            (unsigned)retVal,
            (unsigned long long)memory_requirements.auxiliary_pool_bytes,
            (unsigned long long)memory_requirements.guest_memory_bytes,
            (unsigned long long)memory_requirements.code_cache_bytes);
    return 0;
  }

  /* Apply deterministic time only when requested by the parent. */
  if (options->fixed_date_time)
  {
    /* Parse and register the complete fixed date/time atomically. */
    if (!parse_fixed_date_time(options->fixed_date_time,
                               &fixed_year,
                               &fixed_month,
                               &fixed_day,
                               &fixed_hour,
                               &fixed_minute,
                               &fixed_second) ||
        MVM_SetFixedDateTime(app->vm,
                             fixed_year,
                             fixed_month,
                             fixed_day,
                             fixed_hour,
                             fixed_minute,
                             fixed_second) != MVM_OK)
    {
      fprintf(stderr, "Invalid fixed date/time: %s\n", options->fixed_date_time);
      return 0;
    }
  }

  return 1;
}

/**********************************************************************************************************************
 *  END OF FILE: main.c
 *********************************************************************************************************************/
