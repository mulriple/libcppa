#include <thread>
#include <string>
#include <sstream>
#include <iostream>

#include "test.hpp"
#include "ping_pong.hpp"
#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;

namespace {

std::vector<string_pair> get_kv_pairs(int argc, char** argv, int begin = 1) {
    std::vector<string_pair> result;
    for (int i = begin; i < argc; ++i) {
        auto vec = split(argv[i], '=');
        if (vec.size() != 2) {
            cerr << "\"" << argv[i] << "\" is not a key-value pair" << endl;
        }
        else if (std::any_of(result.begin(), result.end(),
                             [&](const string_pair& p) { return p.first == vec[0]; })) {
            cerr << "key \"" << vec[0] << "\" is already defined" << endl;
        }
        else {
            result.emplace_back(vec[0], vec[1]);
        }
    }
    return result;
}

void client_part(const std::vector<string_pair>& args) {
    auto i = std::find_if(args.begin(), args.end(),
                          [](const string_pair& p) { return p.first == "port"; });
    if (i == args.end()) {
        throw std::runtime_error("no port specified");
    }
    auto port = static_cast<std::uint16_t>(std::stoi(i->second));
    auto ping_actor = remote_actor("localhost", port);
    //spawn_event_based_pong(ping_actor);
    spawn<detached>(pong, ping_actor);
    await_all_others_done();
}

} // namespace <anonymous>

int main(int argc, char** argv) {
    const char* app_path = argv[0];
    if (argc > 1) {
        auto args = get_kv_pairs(argc, argv);
        client_part(args);
        return 0;
    }
    CPPA_TEST(test__remote_actor);
    auto ping_actor = spawn_event_based_ping(10);
    //auto ping_actor = spawn(ping, 10);
    std::uint16_t port = 4242;
    bool success = false;
    do {
        try {
            publish(ping_actor, port);
            success = true;
        }
        catch (bind_failure&) {
            // try next port
            ++port;
        }
    }
    while (!success);
    std::ostringstream oss;
    oss << app_path << " run=remote_actor port=" << port;// << " &>/dev/null";
    // execute client_part() in a separate process,
    // connected via localhost socket
    std::thread child([&oss]() {
        std::string cmdstr = oss.str();
        if (system(cmdstr.c_str()) != 0) {
            cerr << "FATAL: command \"" << cmdstr << "\" failed!" << endl;
            abort();
        }
    });
    await_all_others_done();
    CPPA_CHECK_EQUAL(10, pongs());
    // kill pong actor

    // wait until separate process (in sep. thread) finished execution
    child.join();
    return CPPA_TEST_RESULT;
}
