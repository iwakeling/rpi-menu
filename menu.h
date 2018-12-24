#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <istream>
#include <vector>

#include "opt-parse/opt-parse.h"

static SDL_Color const white{0xFF, 0xFF, 0xFF};
static SDL_Color const grey{0x80, 0x80, 0x80};

class MenuEntry
{
public:
  MenuEntry(SDL_Rect const& pos);
  ~MenuEntry();

  MenuEntry(MenuEntry const&) = delete;
  MenuEntry& operator=(MenuEntry const&) = delete;

  void setPos(int y);
  void render(SDL_Renderer* renderer, TTF_Font* font);
  void focus(bool hasFocus);
  void act();

private:
  std::string title_;
  std::vector<std::string> cmdLine_;
  bool focussed_;
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_;
  SDL_Rect pos_;

  friend std::istream& operator>>(std::istream&, MenuEntry&);
};

class Menu
{
public:
  Menu(int width, int height);
  ~Menu();

  Menu(Menu const&) = delete;
  Menu& operator=(Menu const&) = delete;

  void load(std::istream& is);
  void render(SDL_Renderer* renderer);
  void handleUp();
  void handleDown();
  void handleSelect();

private:
  void moveFocus(size_t idx);

private:
  int width_;
  int height_;
  std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)> font_;
  std::vector<std::shared_ptr<MenuEntry>> items_;
  size_t focusIndex_;
  size_t topIndex_;

  static int const margin_ = 10;
};
