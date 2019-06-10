#include "buttons.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <fstream>
#include <vector>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std::chrono_literals;

bool bounce_time_elapsed(
  std::chrono::steady_clock::time_point refPoint,
  std::chrono::steady_clock::time_point now)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    now - refPoint) > 500ms;
}

Buttons::Buttons(Handler handler)
  : handler_(handler),
    pipe_{-1,-1}
{
}

Buttons::~Buttons()
{
  stop();
}

void Buttons::loadConfig(std::istream& config)
{
  if( pipe_[0] != -1 )
  {
    stop();
  }

  std::getline(config, baseDir_);

  while( config )
  {
    std::string function;
    std::string pin;
    std::getline(config, function, '=');
    std::getline(config, pin);

    if( !pin.empty() )
    {
      buttons_[pin] = std::make_pair(function, 0);
    }
  }

  start();
}

void Buttons::start()
{
  std::ofstream exp(baseDir_ + "/export");
  for( auto& button: buttons_ )
  {
    exp << button.first << std::endl;
  }
  exp.close();

  for( auto& button: buttons_ )
  {
    std::string const& pin = button.first;

    std::ofstream directn(baseDir_ + "/gpio" + pin + "/direction");
    directn << "in" << std::endl;
    directn << "low" << std::endl;
    directn.close();

    std::ofstream edge(baseDir_ + "/gpio" + pin + "/edge");
    edge << "rising" << std::endl;
    edge.close();

    std::string valFileName(baseDir_ + "/gpio" + pin + "/value");
    int fd = open(valFileName.c_str(), O_RDWR);
    if( fd < 0 )
    {
      std::cerr << "Failed to open pin " << pin << ": " <<
        std::strerror(errno) << std::endl;
    }
    else
    {
      // clear any initial interrupt
      char c;
      read(fd, &c, 1);
      button.second.second = fd;
    }
  }

  // create socket pair to control thread
  if( socketpair(AF_LOCAL, SOCK_DGRAM, 0, pipe_) < 0 )
  {
    std::cerr << "Failed to create socket pair: " <<
      std::strerror(errno) << std::endl;
  }
  else
  {
    // start an asynchronous poll loop
    pollingStopped_ = std::async(
      std::launch::async,
      [this]{pollButtons();});
  }
}

void Buttons::stop()
{
  if( pipe_[0] != -1 )
  {
    // poke poll thread to stop
    write(pipe_[0], "0", 1);
    pollingStopped_.wait();
    close(pipe_[0]);
    close(pipe_[1]);
    pipe_[0] = -1;

    // close fds and unexport pins
    std::ofstream unexp(baseDir_ + "/unexport");
    for( auto& button: buttons_ )
    {
      close(button.second.second);
      unexp << button.first << std::endl;
    }
  }
}

void Buttons::pollButtons()
{
  bool quit = false;
  std::vector<struct pollfd> fds(buttons_.size() + 1);
  fds[0].fd = pipe_[1];
  fds[0].events = POLLIN;

  size_t count = 1;
  for( auto& button: buttons_ )
  {
    fds[count].fd = button.second.second;
    fds[count].events = POLLPRI | POLLERR;
    count++;
  }

  auto last_read = std::chrono::steady_clock::now();
  size_t last_idx = 0;
  std::string last_action;

  while( !quit )
  {
    // wait up to half a second for something to change
    poll(fds.data(), count, 500);
    if( (fds[0].revents & POLLIN) != 0 )
    {
      quit = true;
    }
    else
    {
      auto now = std::chrono::steady_clock::now();

      // find the first button that's been pressed
      size_t i = 1;
      auto it = buttons_.begin();
      while( it != buttons_.end() && (fds[i].revents & POLLPRI) == 0 )
      {
        it++;
        i++;
      }

      if( it != buttons_.end() )
      {
        // a new button was pressed
        if( bounce_time_elapsed(last_read, now) ||
            i != last_idx)
        {
          char c;
          lseek(fds[i].fd, 0, SEEK_SET);
          read(fds[i].fd, &c, 1);
          handler_(it->second.first);

          last_read = now;
          last_idx = i;
          last_action = it->second.first;
        }
      }
      else if( last_idx > 0 )
      {
        // no button pressed, see if the previous one is still pressed
        char c;
        lseek(fds[last_idx].fd, 0, SEEK_SET);
        read(fds[last_idx].fd, &c, 1);
        if( c == '1' )
        {
          handler_(last_action);
        }
      }
    }
  }
}
