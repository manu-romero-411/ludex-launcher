#include "application.h"
#include "core/config.h"
#include "core/i18n.h"
#include <clocale>
#include <cstdlib>

namespace {
// idioma del INI -> nombre de locale instalado en el sistema
const char *localeForLanguage(const std::string &lang) {
  if (lang == "es")
    return "es_ES.UTF-8";
  if (lang == "en")
    return "en_US.UTF-8";
  if (lang == "fr")
    return "fr_FR.UTF-8";
  if (lang == "de")
    return "de_DE.UTF-8";
  if (lang == "it")
    return "it_IT.UTF-8";
  return nullptr;
}
} // namespace

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  Config pre = loadConfig();

  // Números SIEMPRE con punto: el INI y los floats no deben depender del idioma
  std::setlocale(LC_NUMERIC, "C");

  if (!pre.language.empty()) {
    // gettext (textos de la UI)
    setenv("LANGUAGE", pre.language.c_str(), 1);
    // strftime (nombres de día/mes) con el MISMO idioma
    const char *loc = localeForLanguage(pre.language);
    if (!loc || !std::setlocale(LC_TIME, loc))
      std::setlocale(LC_TIME, ""); // fallback: locale del sistema
    if (!loc || !std::setlocale(LC_MESSAGES, loc))
      std::setlocale(LC_MESSAGES, "");
  } else {
    // sin override: usar inglés por defecto
    setenv("LANGUAGE", "en", 1);
    std::setlocale(LC_MESSAGES, "en_US.UTF-8");
    std::setlocale(LC_TIME, "en_US.UTF-8");
  }

  Application app;
  if (!app.init())
    return 1;
  int exit_code = app.run();
  app.shutdown();
  return exit_code;
}