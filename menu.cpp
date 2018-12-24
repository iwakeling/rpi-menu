#include "menu.h"

#include <unistd.h>
//#include <fontconfig/fontconfig.h>

MenuEntry::MenuEntry(SDL_Rect const& pos)
  : focussed_(false),
    texture_(nullptr, SDL_DestroyTexture),
    pos_(pos)
{
}

MenuEntry::~MenuEntry()
{
}

void MenuEntry::setPos(int y)
{
  pos_.y = y;
}

void MenuEntry::render(SDL_Renderer* renderer, TTF_Font* font)
{
  if( !texture_ && font != nullptr)
  {
    auto surface = TTF_RenderUTF8_Blended(font, title_.c_str(), white);
    if( surface == nullptr )
    {
      std::cerr << "Failed to create text surface: " << TTF_GetError() << std::endl;
    }
    else
    {
      TTF_SizeUTF8(font, title_.c_str(), &pos_.w, &pos_.h);
      texture_.reset(
        SDL_CreateTextureFromSurface(renderer, surface));
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

void MenuEntry::focus(bool hasFocus)
{
  focussed_ = hasFocus;
}

void MenuEntry::act()
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
}

std::istream& operator>>(std::istream& is, MenuEntry& me)
{
  std::string cmdLine;
  std::getline(is, me.title_, '=');
  std::getline(is, cmdLine);

  size_t n = 0;
  do
  {
    auto nend = cmdLine.find(' ', n);
    me.cmdLine_.emplace_back(cmdLine.substr(n, nend));
    n = nend;
    if( n != std::string::npos )
    {
      n++;
    }
  } while( n != std::string::npos );

  return is;
}

Menu::Menu(int width, int height)
  :width_(width),
   height_(height),
   font_(nullptr, TTF_CloseFont),
   focusIndex_(0),
   topIndex_(0)
{
  font_.reset(TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24));
  if( !font_ )
  {
    std::cerr << "Failed to open font: " << TTF_GetError() << std::endl;
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