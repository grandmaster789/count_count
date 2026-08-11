#include <iostream>
#include "platform/platform.h"
#include "app/application.h"

#include <filesystem>

#if CVC_PLATFORM != CVC_PLATFORM_WINDOWS
    #error "Currently only Windows is supported"
#endif

int main(int, char* argv[]) {
    namespace fs = std::filesystem;

    try {
        // absolute(): argv[0] is a bare filename when launched from the exe's own
        // directory, which leaves find_data_folder with no parent path to walk up
        fs::path exe_path = fs::absolute(argv[0]);
        cc::app::Application le_application(exe_path);

        le_application.run();
    }
    catch (std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << '\n';
        return -1;
    }
    catch (...) {
        std::cerr << "Unknown exception\n";
        return -1;
    }

    return 0;
}
