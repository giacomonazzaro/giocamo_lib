#include "main_ui.h"

#include <cxxabi.h>
#include <imgui.h>
#include <rlImGui.h>

#include <cstdlib>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>

#include "rendering.h"
#include "struct/visit.hpp"

namespace {

// Readable spelling of a type, for the fields the panel can only report.
template <class T>
std::string type_name() {
  int   status = 0;
  char* text   = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
  std::string result = (status == 0 && text) ? text : typeid(T).name();
  std::free(text);
  // Drop the standard library's inline namespace, which is all noise here.
  for (size_t at = result.find("__1::"); at != std::string::npos;
       at        = result.find("__1::", at)) {
    result.erase(at, 5);
  }
  return result;
}

// Every field of a Thing goes through here, and so does every field of anything
// nested inside it. Three cases, and nothing knows what a Thing is:
//   visitable -> an expandable child holding its own fields
//   number    -> a widget that shows and edits it
//   anything else -> its name and its type
template <class T>
void draw_field(const char* name, T& value) {
  if constexpr (visit_struct::traits::is_visitable<T>::value) {
    if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_DefaultOpen)) {
      visit_struct::for_each(value, [](const char* field_name, auto& field) {
        draw_field(field_name, field);
      });
      ImGui::TreePop();
    }
  } else if constexpr (std::is_same_v<T, bool>) {
    ImGui::Checkbox(name, &value);
  } else if constexpr (std::is_floating_point_v<T>) {
    // Drag rather than slide: a slider needs a range, and a range is exactly
    // what reflection cannot tell us about an arbitrary field.
    float number = (float)value;
    if (ImGui::DragFloat(name, &number, 1.0f)) value = (T)number;
  } else if constexpr (std::is_integral_v<T>) {
    int number = (int)value;
    if (ImGui::DragInt(name, &number)) value = (T)number;
  } else {
    ImGui::TextDisabled("%s  (%s)", name, type_name<T>().c_str());
  }
}

}  // namespace

void draw_ui(Table_State& table, const Input&) {
  // run_tabletop opens the window, so this cannot happen any earlier.
  static bool imgui_ready = false;
  if (!imgui_ready) {
    rlImGuiSetup(true);
    imgui_ready = true;
  }

  // The panel follows the last thing that was dropped, and keeps showing it
  // until the next drop. process_input clears dropped_thing at the start of
  // every frame, so the drop has to be caught on the frame it happens and kept
  // here. Read it rather than poll_dropped_thing(), which would consume the
  // event that game logic is waiting for.
  static int selected_thing = -1;
  if (table.dropped_thing) {
    selected_thing = std::get<2>(*table.dropped_thing);
  }

  // The table is drawn through the letterbox transform. The panel belongs in
  // real screen pixels, so step out of that transform and back into it.
  end_screen_fit();
  rlImGuiBegin();

  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(340, 560), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Thing")) {
    if (selected_thing < 0 || selected_thing >= (int)table.things.size()) {
      ImGui::TextUnformatted("Drop a card to inspect it.");
    } else {
      Thing& thing = table.things[selected_thing];
      ImGui::Text("things[%d]", selected_thing);
      ImGui::Separator();
      visit_struct::for_each(thing, [](const char* name, auto& field) {
        draw_field(name, field);
      });
    }
  }
  ImGui::End();

  rlImGuiEnd();
  begin_screen_fit();
}
