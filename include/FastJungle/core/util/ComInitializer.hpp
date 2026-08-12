#pragma once

namespace fjr::util {

    class ComInitializer final {
    public:
        ComInitializer();
        ~ComInitializer();

        ComInitializer(const ComInitializer&) = delete;
        ComInitializer& operator=(const ComInitializer&) = delete;
        ComInitializer(ComInitializer&&) = delete;
        ComInitializer& operator=(ComInitializer&&) = delete;

    private:
        bool initialized_ = false;
    };

} // namespace fjr::util
