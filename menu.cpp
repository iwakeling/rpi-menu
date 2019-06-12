#include "menu.h"

#include <fontconfig/fontconfig.h>
#include <iostream>
#include <unistd.h>
#include <wait.h>

static SDL_Color const white{0xFF, 0xFF, 0xFF};
static SDL_Color const grey{0x80, 0x80, 0x80};

class MenuEntry
{
public:
  MenuEntry(SDL_Rect const& pos)
  : focussed_(false),
    texture_(nullptr, SDL_DestroyTexture),
    pos_(pos)
  {
  }

  MenuEntry(MenuEntry const&) = delete;
  MenuEntry& operator=(MenuEntry const&) = delete;

  void setPos(int y){ pos_.y = y; }
  void focus(bool hasFocus){ focussed_ = hasFocus; }

  void render(SDL_Renderer* renderer, TTF_Font* font)
  {
    if( (!texture_ || renderer != prevRenderer_) && font != nullptr)
    {
      auto surface = TTF_RenderUTF8_Blended(font, title_.c_str(), white);
      if( surface == nullptr )
      {
        std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
      }
      else
      {
        TTF_SizeUTF8(font, title_.c_str(), &pos_.w, &pos_.h);
        prevRenderer_ = renderer;
        texture_.reset(
          SDL_CreateTextureFromSurface(renderer, surface));
        SDL_FreeSurface(surface);
      }
    }

    if( texture_ )
    {
      SDL_RenderCopy(renderer, texture_.get(), nullptr, &pos_);
    }

    if( focussed_ )
    {
      auto uPos = pos_.y + TTF_FontAscent(font);
      SDL_RenderDrawLine(
        renderer,
        pos_.x,
        uPos,
        pos_.x + pos_.w,
        uPos);
    }
  }

  void act()
  {
    pid_t cpid = fork();

    if( cpid < 0 )
    {
      perror("Failed to fork");
    }
    else if( cpid == 0 )
    {
      std::vector<char*> args(cmdLine_.size() + 1);
      for( size_t i = 0; i < cmdLine_.size(); i++ )
      {
        args[i] = &cmdLine_[i][0];
      }
      args[cmdLine_.size()] = nullptr;

      execvp(cmdLine_[0].c_str(), args.data());
      perror("Failed to exec");
    }
    else
    {
      // wait for sub-program to exit
      int status = 0;
      int result = 0;
      do
      {
        status = waitpid(cpid, &result, 0);
        if( status == -1 && errno == EINTR )
        {
          status = 0;
          result = 0;
        }
      } while( status == 0 && !WIFEXITED(result) && !WIFSIGNALED(result) );
    }
  }

private:
  std::string title_;
  std::vector<std::string> cmdLine_;
  bool focussed_;
  SDL_Renderer* prevRenderer_; // to detect changes
  std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_;
  SDL_Rect pos_;

  friend std::istream& operator>>(std::istream&, MenuEntry&);
};

std::istream& operator>>(std::istream& is, MenuEntry& me)
{
  std::string cmdLine;
  std::getline(is, me.title_, '=');
  std::getline(is, cmdLine);

  size_t n = 0;
  do
  {
    auto nend = cmdLine.find(' ', n);
    me.cmdLine_.emplace_back(cmdLine.substr(n, nend - n));
    n = nend;
    if( n != std::string::npos )
    {
      n++;
    }
  } while( n != std::string::npos );

  return is;
}

std::string GetFontFile(std::string const& fontName)
{
  std::string fontFile;
  auto config = FcInitLoadConfigAndFonts();
  auto pat = FcNameParse(reinterpret_cast<FcChar8 const*>(fontName.c_str()));
  FcConfigSubstitute(config, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);

  auto result = FcResultNoMatch;
  auto font = FcFontMatch(config, pat, &result);
  if( result == FcResultMatch && font != nullptr )
  {
    FcChar8* fileName = NULL;
    if( FcPatternGetString(font, FC_FILE, 0, &fileName) == FcResultMatch )
    {
      fontFile = reinterpret_cast<char const*>(fileName);
    }
    FcPatternDestroy(font);
  }
  FcPatternDestroy(pat);
  return fontFile;
}

Menu::Menu(std::string fontName, int width, int height)
  : fontName_(std::move(fontName)),
    width_(width),
    height_(height),
    font_(nullptr, TTF_CloseFont),
    focusIndex_(0),
    topIndex_(0)
{
  std::string fontFile(GetFontFile(fontName_));
  font_.reset(TTF_OpenFont(fontFile.c_str(), 24));
  if( !font_ )
  {
    std::cerr <<
      "Failed to open font " <<
      fontFile << ": " << TTF_GetError() << std::endl;
  }
}

Menu::~Menu()
{
}

void Menu::load(std::istream& is)
{
  int lineHeight = TTF_FontLineSkip(font_.get());
  SDL_Rect pos{margin_, margin_ * 2, width_ - margin_, lineHeight};
  while( !is.eof() )
  {
    auto me = std::make_shared<MenuEntry>(pos);
    is >> *me;
    if( is.good() )
    {
      pos.y += lineHeight;
      items_.emplace_back(me);
    }
  }

  if( !items_.empty() )
  {
    items_.front()->focus(true);
  }
}

void Menu::render(SDL_Renderer* renderer)
{
  int right = width_ - margin_;
  int maxBottom = height_ - margin_;
  int itemBottom = margin_ * 3 + TTF_FontLineSkip(font_.get()) * items_.size();
  int bottom = std::min(itemBottom, maxBottom);
  SDL_Rect boundary{
    margin_,
    margin_ * 2,
    width_ - margin_ * 2,
    height_ - margin_ * 4};

  SDL_SetRenderDrawColor(renderer, grey.r, grey.g, grey.b, 0xFF);
  SDL_RenderDrawLine(renderer, margin_, margin_, right, margin_);
  SDL_RenderDrawLine(renderer, margin_, bottom, right, bottom);

  if( SDL_RenderSetClipRect(renderer, &boundary) < 0 )
  {
    std::cerr << "Failed to set clip rect: " << SDL_GetError() << std::endl;
  }

  for( auto& me: items_ )
  {
    me->render(renderer, font_.get());
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void Menu::handleUp()
{
  if( focusIndex_ > 0 )
  {
    moveFocus(focusIndex_ - 1);
  }
}

void Menu::handleDown()
{
  if( focusIndex_ < items_.size() - 1 )
  {
    moveFocus(focusIndex_ + 1);
  }
}

void Menu::handleSelect()
{
  if( focusIndex_ < items_.size() )
  {
    items_[focusIndex_]->act();
  }
}

void Menu::moveFocus(size_t idx)
{
  if( items_.size() > focusIndex_ )
  {
    items_[focusIndex_]->focus(false);
  }
  focusIndex_ = idx;
  items_[focusIndex_]->focus(true);

  // scroll to ensure current item is visible
  int lineHeight = TTF_FontLineSkip(font_.get());
  size_t effIndex = focusIndex_ - topIndex_;
  int itemTop =  margin_ * 2 + lineHeight * effIndex;
  int itemBottom = itemTop + lineHeight;

  if( itemTop < margin_ * 2 )
  {
    topIndex_--;
  }

  if( itemBottom > height_ - margin_ * 2 )
  {
    topIndex_++;
  }

  int y = margin_ * 2 - static_cast<int>(topIndex_ * lineHeight);
  for( auto& me : items_ )
  {
    me->setPos(y);
    y += lineHeight;
  }
}
