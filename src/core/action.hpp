#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../config/config.hpp"

namespace nexyth {

class App;

class ActionRegistry {
 public:
  using Handler = std::function<void(App&, const std::vector<std::string>&)>;
  static ActionRegistry& instance();
  void execute(const std::string& name, App& app,
               const std::vector<std::string>& args);
  void registerAction(const std::string& name, Handler handler);

 private:
  ActionRegistry();
  std::map<std::string, Handler> handlers_;
};

}  // namespace nexyth
