#ifndef KN_KNC_CAPTURE_H
#define KN_KNC_CAPTURE_H

#include "kn/ast.h"

typedef struct KncCapturePlan KncCapturePlan;

// Plan lexical cell ownership before emitting branches or loop optimizations.
// Declaration keys are Stmt*/Param* identities; NULL denotes the receiver.
// Literal bodies are planned separately when their synthetic function is built.
KncCapturePlan *knc_capture_plan_create(Stmt *body, ParamList *params, int has_receiver);
int knc_capture_plan_contains(const KncCapturePlan *plan, const void *declaration);
void knc_capture_plan_dispose(KncCapturePlan *plan);

#endif
