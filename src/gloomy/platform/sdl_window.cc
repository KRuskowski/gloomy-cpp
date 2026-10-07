#include "gloomy/platform/sdl_window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

namespace gloomy::platform {

namespace {

auto TranslateKey(SDL_Keycode key) -> ui::Key {
  using ui::Key;
  if (key >= SDLK_a && key <= SDLK_z) {
    return static_cast<Key>(static_cast<int>(Key::kA) + (key - SDLK_a));
  }
  if (key >= SDLK_0 && key <= SDLK_9) {
    return static_cast<Key>(static_cast<int>(Key::k0) + (key - SDLK_0));
  }
  if (key >= SDLK_F1 && key <= SDLK_F12) {
    return static_cast<Key>(static_cast<int>(Key::kF1) + (key - SDLK_F1));
  }
  switch (key) {
    case SDLK_TAB:
      return Key::kTab;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      return Key::kEnter;
    case SDLK_ESCAPE:
      return Key::kEscape;
    case SDLK_BACKSPACE:
      return Key::kBackspace;
    case SDLK_DELETE:
      return Key::kDelete;
    case SDLK_INSERT:
      return Key::kInsert;
    case SDLK_LEFT:
      return Key::kLeft;
    case SDLK_RIGHT:
      return Key::kRight;
    case SDLK_UP:
      return Key::kUp;
    case SDLK_DOWN:
      return Key::kDown;
    case SDLK_HOME:
      return Key::kHome;
    case SDLK_END:
      return Key::kEnd;
    case SDLK_PAGEUP:
      return Key::kPageUp;
    case SDLK_PAGEDOWN:
      return Key::kPageDown;
    case SDLK_SPACE:
      return Key::kSpace;
    case SDLK_SLASH:
      return Key::kSlash;
    default:
      return Key::kNone;
  }
}

auto TranslateMods(Uint16 mod) -> std::uint8_t {
  std::uint8_t m = ui::kModNone;
  if ((mod & KMOD_SHIFT) != 0)
    m |= ui::kModShift;
  if ((mod & KMOD_CTRL) != 0)
    m |= ui::kModCtrl;
  if ((mod & KMOD_ALT) != 0)
    m |= ui::kModAlt;
  if ((mod & KMOD_GUI) != 0)
    m |= ui::kModSuper;
  return m;
}

auto TranslateButton(Uint8 button) -> ui::PointerButton {
  switch (button) {
    case SDL_BUTTON_RIGHT:
      return ui::PointerButton::kRight;
    case SDL_BUTTON_MIDDLE:
      return ui::PointerButton::kMiddle;
    default:
      return ui::PointerButton::kLeft;
  }
}

}  // namespace

auto TranslateEvent(const SDL_Event &event, float scale)
    -> std::optional<ui::InputEvent> {
  using ui::InputEvent;
  switch (event.type) {
    case SDL_MOUSEMOTION:
      return InputEvent::Move(static_cast<float>(event.motion.x) * scale,
                              static_cast<float>(event.motion.y) * scale);
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      const float x = static_cast<float>(event.button.x) * scale;
      const float y = static_cast<float>(event.button.y) * scale;
      const ui::PointerButton b = TranslateButton(event.button.button);
      InputEvent e = event.type == SDL_MOUSEBUTTONDOWN
                         ? InputEvent::Down(x, y, b)
                         : InputEvent::Up(x, y, b);
      e.mods = TranslateMods(static_cast<Uint16>(SDL_GetModState()));
      return e;
    }
    case SDL_MOUSEWHEEL: {
      float dx = event.wheel.preciseX;
      float dy = event.wheel.preciseY;
      if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
        dx = -dx;
        dy = -dy;
      }
      InputEvent e = InputEvent::Wheel(dx, dy);
      e.mods = TranslateMods(static_cast<Uint16>(SDL_GetModState()));
      return e;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      const ui::Key key = TranslateKey(event.key.keysym.sym);
      if (key == ui::Key::kNone) {
        return std::nullopt;
      }
      InputEvent e =
          InputEvent::KeyDown(key, TranslateMods(event.key.keysym.mod));
      e.repeat = event.key.repeat != 0;
      if (event.type == SDL_KEYUP) {
        e.type = InputEvent::Type::kKeyUp;
      }
      return e;
    }
    case SDL_TEXTINPUT:
      return InputEvent::Text(event.text.text);
    default:
      return std::nullopt;
  }
}

auto SdlWindow::Create(const std::string &title, int width, int height)
    -> std::expected<std::unique_ptr<SdlWindow>, std::string> {
  if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
    return std::unexpected(std::string("SDL_Init: ") + SDL_GetError());
  }
  SDL_Window *window = SDL_CreateWindow(
      title.c_str(),
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      width,
      height,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window == nullptr) {
    return std::unexpected(std::string("SDL_CreateWindow: ") + SDL_GetError());
  }
  SDL_StartTextInput();
  return std::unique_ptr<SdlWindow>(new SdlWindow(window));
}

SdlWindow::~SdlWindow() {
  SDL_DestroyWindow(window_);
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

auto SdlWindow::RequiredInstanceExtensions() const -> std::vector<std::string> {
  unsigned count = 0;
  SDL_Vulkan_GetInstanceExtensions(window_, &count, nullptr);
  std::vector<const char *> names(count);
  SDL_Vulkan_GetInstanceExtensions(window_, &count, names.data());
  return {names.begin(), names.end()};
}

auto SdlWindow::CreateSurface(VkInstance instance) const -> VkSurfaceKHR {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (SDL_Vulkan_CreateSurface(window_, instance, &surface) == SDL_FALSE) {
    return VK_NULL_HANDLE;
  }
  return surface;
}

auto SdlWindow::DrawableExtent() const -> VkExtent2D {
  int w = 0;
  int h = 0;
  SDL_Vulkan_GetDrawableSize(window_, &w, &h);
  return {static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)};
}

auto SdlWindow::PixelScale() const -> float {
  int w = 0;
  int h = 0;
  SDL_GetWindowSize(window_, &w, &h);
  const VkExtent2D drawable = DrawableExtent();
  return w > 0 ? static_cast<float>(drawable.width) / static_cast<float>(w)
               : 1.0F;
}

auto SdlWindow::PollEvents(std::vector<ui::InputEvent> &out) -> bool {
  const float scale = PixelScale();
  bool running = true;
  SDL_Event event;
  while (SDL_PollEvent(&event) != 0) {
    if (event.type == SDL_QUIT) {
      running = false;
    }
    if (auto e = TranslateEvent(event, scale)) {
      out.push_back(std::move(*e));
    }
  }
  return running;
}

void SdlWindow::InstallClipboard(ui::Context &ctx) {
  ctx.clipboard().get = [] {
    char *text = SDL_GetClipboardText();
    std::string result = text != nullptr ? text : "";
    SDL_free(text);
    return result;
  };
  ctx.clipboard().set = [](const std::string &text) {
    SDL_SetClipboardText(text.c_str());
  };
}

}  // namespace gloomy::platform
