#include "toast.h"
#include "../ui_common.h"
#include <algorithm>

namespace ui {

void ToastCenter::push(void *icon, const std::string &title,
                       const std::string &desc, float duration) {
  Toast t;
  t.icon = icon;
  t.title = title;
  t.desc = desc;
  t.duration = duration;
  if (toasts_.size() >= 4)
    toasts_.pop_front();
  toasts_.push_back(std::move(t));
}

void ToastCenter::update(float dt) {
  for (auto &t : toasts_)
    t.age += dt;
  while (!toasts_.empty() && toasts_.front().age >= toasts_.front().duration)
    toasts_.pop_front();
}

void ToastCenter::draw(ImDrawList *dl, float W, float H, bool light) {
  if (toasts_.empty())
    return;

  const float pad = H * 0.016f;
  const float gap = H * 0.012f;
  const float icon_sz = H * 0.030f;
  const float in_t = 0.25f, out_t = 0.30f;

  float y = H * 0.015f;
  int shown = 0;

  for (auto &t : toasts_) {
    if (shown >= 3)
      break;

    ImGui::PushFont(g_font_date);
    ImVec2 ts = ImGui::CalcTextSize(t.title.c_str());
    ImGui::PopFont();
    ImGui::PushFont(g_font_hint);
    ImVec2 ds = ImGui::CalcTextSize(t.desc.c_str());
    ImGui::PopFont();

    float h = pad * 2 + std::max(icon_sz, ts.y + 4.0f + ds.y);
    float w = pad * 2 + (t.icon ? icon_sz + gap : 0.0f) + std::max(ts.x, ds.x);

    // entra deslizando desde arriba, sale hacia arriba al expirar
    float k_in = smooth(std::min(1.0f, t.age / in_t));
    float k_out = (t.duration - t.age) < out_t
                      ? std::max(0.0f, (t.duration - t.age) / out_t)
                      : 1.0f;
    float k = std::min(k_in, smooth(k_out));

    float x = (W - w) * 0.5f;
    float yy = y - (1.0f - k) * (h + H * 0.02f);

    int a = (int)(240 * k);
    ImU32 bg      = light ? IM_COL32(250, 250, 252, a) : IM_COL32(30, 32, 40, a);
    ImU32 c_title = light ? IM_COL32(25, 25, 30, a)   : IM_COL32(235, 235, 235, a);
    ImU32 c_desc  = light ? IM_COL32(60, 60, 70, a)   : IM_COL32(170, 175, 185, a);

    dl->AddRectFilled(ImVec2(x, yy), ImVec2(x + w, yy + h), bg, 8.0f);

    float cx = x + pad;
    float cy = yy + (h - icon_sz) * 0.5f;
    if (t.icon) {
      dl->AddImage((ImTextureID)t.icon, ImVec2(cx, cy),
                   ImVec2(cx + icon_sz, cy + icon_sz), ImVec2(0, 0),
                   ImVec2(1, 1), IM_COL32(255, 255, 255, a));
      cx += icon_sz + gap;
    }

    float ty = yy + (h - (ts.y + 4.0f + ds.y)) * 0.5f;
    dl->AddText(g_font_date, g_font_date->FontSize, ImVec2(cx, ty),
                c_title, t.title.c_str());
    dl->AddText(g_font_hint, g_font_hint->FontSize, ImVec2(cx, ty + ts.y + 4.0f),
                c_desc, t.desc.c_str());

    y += h + gap;
    ++shown;
  }
}

} // namespace ui