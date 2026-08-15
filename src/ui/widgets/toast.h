#pragma once
#include <imgui.h>
#include <deque>
#include <string>

namespace ui {
struct Toast {
  void *icon = nullptr;          // textura SVG ya cargada (ui_icons, etc.)
  std::string title, desc;
  float age = 0.0f, duration = 3.5f;
};
// Centro de notificaciones: cola FIFO, apila hasta 3 visibles,
// entra deslizando desde arriba y sale hacia arriba al expirar.
class ToastCenter {
public:
  void push(void *icon, const std::string &title, const std::string &desc,
            float duration = 3.5f);
  void update(float dt);
  void draw(ImDrawList *dl, float W, float H, bool light);
  bool empty() const { return toasts_.empty(); }
private:
  std::deque<Toast> toasts_;
};
} // namespace ui