#pragma once
#include <libintl.h>

// Macro estándar de gettext: _(msgid) -> traducción según locale activo
#define _(s) gettext(s)