#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <istream>
#include <memory>
#include <vector>

class MenuEntry;

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
