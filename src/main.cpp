#include "application.h"
#include "core/config.h" // Necesario para cargar la configuración antes de la app
#include <clocale>
#include <cstdlib>   // Para setenv
#include <libintl.h> // Para bindtextdomain y textdomain

// Respaldo por si CMake no define LOCALEDIR
#ifndef LOCALEDIR
#define LOCALEDIR "./locale"
#endif

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  // 1. Cargar configuración temprana para leer el idioma
  Config pre = loadConfig();

  // 2. Aplicar el idioma al entorno antes de setlocale
  if (!pre.language.empty()) {
    setenv("LANGUAGE", pre.language.c_str(), 1); // solo traducciones de gettext
  }

  // 3. Inicializar localización y gettext
  std::setlocale(LC_ALL, "");
  bindtextdomain("ludex", LOCALEDIR);
  textdomain("ludex");

  Application app;
  if (!app.init()) {
    return 1;
  }
  int exit_code = app.run();
  app.shutdown();
  return exit_code;
}