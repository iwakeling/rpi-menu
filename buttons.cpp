#include "buttons.h"

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

  std::ofstream exp(baseDir_ + "/export");

  while( config )
  {
    std::string function;
    std::string pin;
    std::getline(config, function, '=');
    std::getline(config, pin);

    exp << pin << std::endl;

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
      buttonMap_[fd] = std::make_pair(function, pin);
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
    // start a background thread to run a poll loop
    pollThreadDone_ = std::async(
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
    pollThreadDone_.wait();
    close(pipe_[0]);
    close(pipe_[1]);
    pipe_[0] = -1;

    // close fds and unexport pins
    std::ofstream unexp(baseDir_ + "/unexport");
    for( auto& button: buttonMap_ )
    {
      close(button.first);
      unexp << button.second.second << std::endl;
    }
  }
}

void Buttons::pollButtons()
{
  bool quit = false;
  std::vector<struct pollfd> fds(buttonMap_.size() + 1);
  fds[0].fd = pipe_[1];
  fds[0].events = POLLIN;

  size_t count = 1;
  for( auto& button: buttonMap_ )
  {
    fds[count].fd = button.first;
    fds[count].events = POLLPRI | POLLERR;
    count++;
  }

  while( !quit )
  {
    poll(fds.data(), count, -1);
    if( (fds[0].revents & POLLIN) != 0 )
    {
      quit = true;
    }
    else
    {
      size_t i = 1;
      for( auto& button: buttonMap_ )
      {
        if( (fds[i].revents & POLLPRI) != 0 )
        {
          char c;
	  lseek(fds[i].fd, 0, SEEK_SET);
          read(fds[i].fd, &c, 1);
          handler_(button.second.first);
        }
        i++;
      }
    }
  }
}
