#pragma once

// NOLINTBEGIN(bugprone-reserved-identifier,cppcoreguidelines-macro-usage)
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) gettext_noop(String)
#else
#define _(String) (String)
#define N_(String) (String)
#endif
// NOLINTEND(bugprone-reserved-identifier,cppcoreguidelines-macro-usage)
