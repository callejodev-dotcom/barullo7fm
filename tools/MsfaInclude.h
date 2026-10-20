// Inclúe as cabeceiras do motor msfa sen os avisos do compilador (é código de terceiros).
#pragma once
#if defined(_MSC_VER)
 #pragma warning(push, 0)
#elif defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wfloat-conversion"
 #pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "msfa/synth.h"
#include "msfa/dx7note.h"
#include "msfa/controllers.h"
#include "msfa/fm_core.h"
#include "msfa/lfo.h"
#include "msfa/env.h"
#include "msfa/pitchenv.h"
#include "msfa/exp2.h"
#include "msfa/sin.h"
#include "msfa/freqlut.h"
#include "msfa/porta.h"
#if defined(_MSC_VER)
 #pragma warning(pop)
#elif defined(__clang__)
 #pragma clang diagnostic pop
#elif defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif
