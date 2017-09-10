#include <csignal>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iterator>
#include <hexicord/rest_client.hpp>
#include <hexicord/gateway_client.hpp>

boost::asio::io_service ios;
Hexicord::GatewayClient gclient(ios, "MzM5MzU1NDE3MzY2ODg4NDU4.DG0kwQ.YhHUZyxf768L6aJRUU_WxOUm3Hg");
Hexicord::RestClient rclient(ios, "MzM5MzU1NDE3MzY2ODg4NDU4.DG0kwQ.YhHUZyxf768L6aJRUU_WxOUm3Hg");

int main() {
    uint64_t prevMsgId = 0;
    uint64_t prevChnId = 0;
    nlohmann::json me;

    std::signal(SIGABRT, [](int) {
        gclient.disconnect();
        std::exit(1); 
    });

    std::signal(SIGINT, [](int) {
        gclient.disconnect();
        std::exit(1);
    });

    gclient.eventDispatcher.addHandler(Hexicord::Event::Ready, [&](const nlohmann::json&) {
        nlohmann::json guilds = rclient.getUserGuilds();
        std::cout << "I'm in guilds: ";
        for (auto guild : guilds) {
            std::cout << guild["name"] << " ";
        }
        std::cout << '\n';

        me = rclient.getMe();
    });

    gclient.eventDispatcher.addHandler(Hexicord::Event::MessageCreate, [&](const nlohmann::json& payload) {
        if (payload["author"]["id"] == me["id"]) return;

        std::string text = payload["content"];
        uint64_t channelId = std::stoull(payload["channel_id"].get<std::string>());
        if (text[0] == 'f' && text[1] == '>') {
            if (text.size() > 11 && std::string(text.begin(), text.begin() + 11) == "f>username ") {
                try {
                    rclient.setUsername(std::string(text.begin() + 11, text.end()));
                } catch (std::exception& excp) {
                    rclient.sendTextMessage(channelId, excp.what());
                }
            } else if (text.size() > 9 && std::string(text.begin(), text.begin() + 9) == "f>avatar ") {
                try {
                    rclient.setAvatar(std::ifstream(std::string(text.begin() + 9, text.end()), std::ios::binary));
                } catch (std::exception& excp) {
                    rclient.sendTextMessage(channelId, excp.what());
                }
            } else if (text.size() > 7 && std::string(text.begin(), text.begin() + 7) == "f>echo ") {
                try {
                    rclient.sendTextMessage(channelId, std::string(text.begin() + 6, text.end()));
                } catch (...) {
                }
            } else if (text.size() == 6 && std::string(text.begin(), text.begin() + 6) == "f>ping") {
                auto start = std::chrono::steady_clock::now();
                nlohmann::json msg = rclient.sendTextMessage(channelId, " ... measuring REST API response time ...");
                auto end = std::chrono::steady_clock::now();
                
                uint64_t msgId = std::stoull(msg["id"].get<std::string>());
                uint64_t chnId = std::stoull(msg["channel_id"].get<std::string>());
                auto requestTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                rclient.editMessage(chnId, msgId, std::string(" `") + std::to_string(requestTime) + "` ms");
            } else if (text.size() > 9 && std::string(text.begin(), text.begin() + 9) == "f>suicide") {
                std::exit(0);
            } else if (text.size() > 7 && std::string(text.begin(), text.begin() + 7) == "f>file ") {
                rclient.sendFile(channelId, Hexicord::File(std::string(text.begin() + 7, text.end())));
            } else {
                rclient.sendTextMessage(channelId, "Unknown command.");
            }
        } else if (payload["content"] == "nya") { 
            if (prevMsgId && prevChnId) {
                rclient.deleteMessage(prevChnId, prevMsgId);
            }

            nlohmann::json newMsg = rclient.sendTextMessage(channelId, "nyaa!  \\o/% ");
            prevChnId = std::stoull(newMsg["channel_id"].get<std::string>());
            prevMsgId = std::stoull(newMsg["id"].get<std::string>());
        }
    });


    auto pair = rclient.getGatewayUrlBot();
    std::cout << "Gateway URL: " << pair.first << '\n';
    std::cout << "Recommended shards count: " << pair.second << '\n';

    std::cout << "Connecting to gateway...\n";
    gclient.connect(pair.first);

    ios.run();
}
