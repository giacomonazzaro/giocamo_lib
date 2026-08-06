#include "main_ui.h"

#include <imgui.h>
#include <rlImGui.h>

#include <cstdlib>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "rendering.h"
#include "struct/print.h"
#include "struct/visit.hpp"

namespace {

void draw_field(const char* name, Color& color) {
  float rgba[4] = {
    color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f
  };
  if (ImGui::ColorEdit4(name, rgba)) {
    color.r = (unsigned char)(rgba[0] * 255.0f);
    color.g = (unsigned char)(rgba[1] * 255.0f);
    color.b = (unsigned char)(rgba[2] * 255.0f);
    color.a = (unsigned char)(rgba[3] * 255.0f);
  }
}

template <class T>
void draw_field(const char* name, T& value);

// A list gets its own scroll area, five rows tall. Only the rows actually on
// screen are built, so a vector of thousands costs the same as one of five.
template <class T>
void draw_vector(const char* name, T& values) {
  const int count = (int)values.size();
  // The field name alone is the id; the row text is only text. Otherwise the
  // count would be part of the id, and selecting a thing whose list is a
  // different length would look like a different node and spring shut.
  if (!ImGui::TreeNodeEx(
        name, 0, "%s: %s (%d)", name, get_type_name(values).c_str(), count
      )) {
    return;
  }

  // Frame height, not text height: a row holds a widget, which is taller than
  // a line of text by its padding.
  const float row_height = ImGui::GetFrameHeightWithSpacing();
  ImGui::PushID(name);
  if (ImGui::BeginChild("list", ImVec2(0.0f, row_height * 5.0f),
                        ImGuiChildFlags_Borders)) {
    // Rows are one line each, which is what lets the clipper skip the rest.
    // An element expanded into a taller row makes the scrollbar an estimate.
    ImGuiListClipper clipper;
    clipper.Begin(count, row_height);
    while (clipper.Step()) {
      for (int index = clipper.DisplayStart; index < clipper.DisplayEnd;
           ++index) {
        std::string element = "[" + std::to_string(index) + "]";
        draw_field(element.c_str(), values[index]);
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopID();
  ImGui::TreePop();
}

// Every field of a Thing goes through here, and so does every field of anything
// nested inside it. Nothing in it knows what a Thing is:
//   visitable -> an expandable child holding its own fields
//   vector    -> an expandable, scrollable list of its elements
//   number    -> a widget that shows and edits it
//   anything else -> its name and its type
template <class T>
void draw_field(const char* name, T& value) {
  if constexpr (is_std_vector<T>::value) {
    draw_vector(name, value);
  } else if constexpr (visit_struct::traits::is_visitable<T>::value) {
    // Id from the field name only, so the row text never affects it.
    if (ImGui::TreeNodeEx(name, 0, "%s: %s", name,
                          get_type_name(value).c_str())) {
      visit_struct::for_each(value, [](const char* field_name, auto& field) {
        draw_field(field_name, field);
      });
      ImGui::TreePop();
    }
  } else if constexpr (std::is_same_v<T, std::string>) {
    ImGui::Text("%s: \"%s\"", name, value.c_str());
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
    ImGui::Text("%s: %s", name, get_type_name(value).c_str());
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
