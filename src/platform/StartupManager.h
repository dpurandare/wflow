#pragma once

namespace wflow {

class StartupManager {
public:
    bool IsEnabled() const;
    bool SetEnabled(bool enabled) const;
};

}  // namespace wflow
