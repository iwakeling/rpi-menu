#include <future>
#include <istream>
#include <map>
#include <string>

class Buttons
{
public:
  typedef std::function<void(std::string const&)> Handler;

public:
  Buttons(Handler handler);
  ~Buttons();

  Buttons(Buttons const&) = delete;
  Buttons(Buttons&&) = delete;
  Buttons& operator=(Buttons const&) = delete;

  // load config from the specified stream and start
  // listening for button presses
  void loadConfig(std::istream& config);

  // stop listening for button presses and release all resources
  // called automatically by dtor
  void stop();

private:
  void pollButtons();

private:
  std::string baseDir_;
  std::map<int,std::pair<std::string,std::string>> buttonMap_;
  Handler handler_;
  std::future<void> pollThreadDone_;
  int pipe_[2];
};
