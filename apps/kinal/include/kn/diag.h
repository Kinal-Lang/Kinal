#pragma once
#include "kn/source.h"

typedef enum
{
    KN_STAGE_PARSER,
    KN_STAGE_SEMA,
    KN_STAGE_CODEGEN
} KnDiagStage;

typedef enum
{
    KN_DIAG_SEV_ERROR = 0,
    KN_DIAG_SEV_WARNING = 1
} KnDiagSeverity;

typedef struct
{
    const KnSource *src;
    KnDiagStage stage;
    KnDiagSeverity severity;
    int line;
    int col;
    int len;
    const char *code;
    const char *title;
    const char *detail;
    const char *got;
} KnDiagEvent;

typedef void (*KnDiagSink)(const KnDiagEvent *ev, void *user_data);

typedef enum
{
    KN_COLOR_AUTO = 0,
    KN_COLOR_ALWAYS = 1,
    KN_COLOR_NEVER = 2
} KnDiagColorMode;

void kn_diag_reset(void);
void kn_diag_set_color_mode(int mode);          // KnDiagColorMode
void kn_diag_set_language(const char *lang);    // "en" or "zh"
void kn_diag_set_locale_file(const char *path); // JSON map by code
void kn_diag_set_sink(KnDiagSink sink, void *user_data);
void kn_diag_set_quiet(int quiet);
void kn_diag_set_max_errors(int max_errors);
void kn_diag_set_warn_as_error(int enabled);
void kn_diag_set_warning_level(int level);
int kn_diag_error_count(void);
int kn_diag_warning_count(void);
int kn_diag_warning_enabled(int level);
void kn_diag_print_summary(void);
int kn_diag_export_english_template(const char *out_path);
const char *kn_diag_localize(const char *key, const char *fallback);
void kn_diag_register_seed_code(const char *code, int severity, const char *title, const char *detail);
void kn_diag_register_seed(int severity, const char *title, const char *detail);

void kn_diag_report(const KnSource *src, KnDiagStage stage, int line, int col, int len,
                    const char *title, const char *detail, const char *got);
void kn_diag_warn(const KnSource *src, KnDiagStage stage, int warn_level, int line, int col, int len,
                  const char *title, const char *detail);



