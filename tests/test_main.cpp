#include <iostream>

void RunSerializationTests();
void RunLockFreeQueueTests();
void RunSecurityTests();
void RunGamePrimitivesTests();
void RunNetworkTransportTests();

int main() {
    std::cout << "========================================================\n";
    std::cout << "  DarkFireNetworking 2.0 Comprehensive Test Suite\n";
    std::cout << "========================================================\n";

    try {
        RunSerializationTests();
        RunLockFreeQueueTests();
        RunSecurityTests();
        RunGamePrimitivesTests();
        RunNetworkTransportTests();

        std::cout << "\n========================================================\n";
        std::cout << "  ALL TESTS PASSED SUCCESSFULLY! (100% PASS)\n";
        std::cout << "========================================================\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED WITH EXCEPTION: " << e.what() << "\n";
        return 1;
    }
}
