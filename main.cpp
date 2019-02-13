#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <fstream>

#include "opt-parse/opt-parse.h"

#include "buttons.h"
#include "menu.h"

namespace Exit
{
enum
{
  CMD_LINE_ARGS = 1,
  SDL,
  TTF,
  REGISTER_EVENTS,
  CREATE_WINDOW
};
}

enum class Action
{
  None,
  Shutdown,
  Up,
  Down,
  Select,
  Quit
};

class Display
{
public:
  Display(int width, int height)
    : width_(width)
    , height_(height)
  {
    start();
  }

  ~Display()
  {
    stop();
  }

  void start()
  {
    int windowFlags = SDL_WINDOW_SHOWN;
    if( width_ == 0 )
    {
      windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    window_ = SDL_CreateWindow(
      "Rpi-Menu",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      width_,
      height_,
      windowFlags);
    if( window_ == nullptr )
    {
      std::string err("Failed to create SDL window: ");
      err.append(SDL_GetError());
      throw std::runtime_error(err);
    }
    SDL_GetWindowSize(window_, &width_, &height_);
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if( renderer_ == nullptr )
    {
      std::string err("Failed to create SDL renderer: ");
      err.append(SDL_GetError());
      throw std::runtime_error(err);
    }
  }

  void stop()
  {
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
  }

  operator SDL_Renderer*() { return renderer_; }

  int width() { return width_; }
  int height() { return height_; }

private:
  int width_;
  int height_;
  SDL_Window* window_;
  SDL_Renderer* renderer_;
};

class Suspend
{
public:
  Suspend(Buttons* buttons, Display* display)
    : buttons_(buttons)
    , display_(display)
  {
    buttons_->stop();
    display_->stop();
  }

  ~Suspend()
  {
    display_->start();
    buttons_->start();
  }

private:
  Buttons* buttons_;
  Display* display_;
};

int main(int argc, char** argv)
{
  int requestedWidth = 0;
  int requestedHeight = 0;
  std::string menuFileName;
  std::string buttonFileName;
  std::string fontName;

  if( !Opt::parseCmdLine(argc, argv, {
        Opt(
          "--menuFile=(.*)",
          "Name of file containing menu entries",
          [&menuFileName](std::cmatch const& m)
          {
            menuFileName = m[1];
          }),
        Opt(
          "--buttonFile=(.*)",
          "Name of file containing input button parameters",
          [&buttonFileName](std::cmatch const& m)
          {
            buttonFileName = m[1];
          }),
        Opt(
          "--font=(.*)",
          "Name of font, defaults to DejaVuSans",
          [&fontName](std::cmatch const& m)
          {
            fontName = m[1];
          }),
        Opt(
          "--screen=([0-9]+)x([0-9]+)",
          "Screen width and height in pixels, full screen if omitted",
          [&requestedWidth, &requestedHeight](std::cmatch const& m)
          {
            requestedWidth = atoi(m[1].str().c_str());
            requestedHeight = atoi(m[2].str().c_str());
          }),
      }) )
  {
    exit(Exit::CMD_LINE_ARGS);
  }

  std::map<std::string, Action> actions{
    {"shutdown", Action::Shutdown},
    {"up", Action::Up},
    {"down", Action::Down},
    {"select", Action::Select},
    {"quit", Action::Quit}
  };
  std::map<SDL_Keycode, Action> keys{
    {SDLK_s, Action::Shutdown},
    {SDLK_UP, Action::Up},
    {SDLK_DOWN, Action::Down},
    {SDLK_RETURN, Action::Select},
    {SDLK_q, Action::Quit}
  };

  auto e = SDL_Init(SDL_INIT_VIDEO);
  if( e < 0 )
  {
    std::cerr << "Failed to initialize SDL." << e << " - " << SDL_GetError() << std::endl;

    e = SDL_Init(0);
    if( e < 0 )
    {
      std::cerr << "...at all!" << SDL_GetError() << std::endl;
    }
    else
    {
      std::cerr << "Available video drivers:" << std::endl;
      auto numDrivers = SDL_GetNumVideoDrivers();
      for( auto i = 0; i < numDrivers; i++ )
      {
        std::cout << SDL_GetVideoDriver(i) << std::endl;
      }
    }
    exit(Exit::SDL);
  }

  if( TTF_Init() < 0 )
  {
    std::cerr <<
      "Failed to initialise SDL TTF support: " <<
      TTF_GetError() <<
      std::endl;
    exit(Exit::TTF);
  }

  auto buttonPressEventType = SDL_RegisterEvents(1);
  if( buttonPressEventType == static_cast<Uint32>(-1) )
  {
    std::cerr <<
      "Failed to register event types with SDL: " <<
      SDL_GetError() <<
      std::endl;
    exit(Exit::REGISTER_EVENTS);
  }

  try
  {
    Display display(requestedWidth, requestedHeight);
    Menu menu(std::move(fontName), display.width(), display.height());
    std::ifstream menuFile(menuFileName);
    if( menuFile.is_open() )
    {
      menu.load(menuFile);
      menuFile.close();
    }

    Buttons buttons(
      [&buttonPressEventType, &actions](std::string const& function)
      {
        auto a = actions.find(function);
        if( a != actions.end() )
        {
          SDL_Event event;
          event.type = buttonPressEventType;
          event.user.code = static_cast<Sint32>(a->second);
          SDL_PushEvent(&event);
        }
      });
    std::ifstream buttonFile(buttonFileName);
    if( buttonFile.is_open() )
    {
      buttons.loadConfig(buttonFile);
      buttonFile.close();
    }

    bool quit = false;
    SDL_Event e;
    while( !quit && SDL_WaitEvent( &e ) != 0 )
    {
      SDL_SetRenderDrawColor(display, 0x00, 0x00, 0x00, 0xFF);
      SDL_RenderClear(display);

      menu.render(display);

      Action action = Action::None;
      if( e.type == SDL_KEYDOWN &&
          e.key.repeat == 0 )
      {
        auto k = keys.find(e.key.keysym.sym);
        if( k != keys.end() )
        {
          action = k->second;
        }
      }
      else if( e.type == buttonPressEventType )
      {
        action = static_cast<Action>(e.user.code);
      }
      else if( e.type == SDL_QUIT )
      {
        std::cout << "got SDL_QUIT" << std::endl;
        action = Action::Quit;
      }

      switch(action)
      {
      case Action::Shutdown:
      {
        buttons.stop();
        display.stop();
        system("shutdown -h now");
        break;
      }
      case Action::Up:
      {
        menu.handleUp();
        break;
      }
      case Action::Down:
      {
        menu.handleDown();
        break;
      }
      case Action::Select:
      {
        Suspend suspend(&buttons, &display);
        menu.handleSelect();
        break;
      }
      case Action::Quit:
      {
        quit = true;
        break;
      }
      case Action::None:
      {
        break;
      }
      }

      SDL_RenderPresent(display);
    }

    display.stop();
  }
  catch(std::exception const& e)
  {
    std::cerr << "An exception occurred: " << e.what() << std::endl;
  }

  TTF_Quit();
  SDL_Quit();

  return 0;
}
