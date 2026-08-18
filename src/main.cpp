#include "application.h"
#include "core/config.h"
#include <clocale>
#include <cstdlib>
#include <cstring>

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
  // Parsear argumentos de línea de comandos
  bool windowed = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "-w") == 0 || std::strcmp(argv[i], "--windowed") == 0) {
      windowed = true;
    }
  }

  bindtextdomain("ludex", LOCALEDIR);
  textdomain("ludex");
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
    // LC_CTYPE define el codeset (UTF-8). Sin esto, gettext convierte
    // a ASCII y las tildes se vuelven '?'.
    if (!loc || !std::setlocale(LC_CTYPE, loc))
      std::setlocale(LC_CTYPE, "");
  } else {
    // sin override: usar inglés por defecto
    setenv("LANGUAGE", "en", 1);
    std::setlocale(LC_MESSAGES, "en_US.UTF-8");
    std::setlocale(LC_TIME, "en_US.UTF-8");
    std::setlocale(LC_CTYPE, "en_US.UTF-8");
  }
  Application app(windowed);
  if (!app.init())
    return 1;
  int exit_code = app.run();
  app.shutdown();
  return exit_code;
}
