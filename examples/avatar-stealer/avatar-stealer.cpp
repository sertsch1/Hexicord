#include <cstdlib>
#include <iostream>
#include <hexicord/gateway_client.hpp>
#include <hexicord/rest_client.hpp>

boost::asio::io_service ioService;

void stealUserAvatar(const nlohmann::json& userObject) {
    std::string dbgJson = userObject.dump(4); 

    if (userObject["avatar"].is_null()) {
        Hexicord::ImageReference<Hexicord::DefaultUserAvatar>(std::stoi(userObject["discriminator"].get<std::string>()))
            .download<Hexicord::Png>(ioService, 2048)
            .file.write(std::string("user_") + userObject["id"].get<std::string>() +
                        "_default.png");
    } else {
        Hexicord::ImageReference<Hexicord::UserAvatar>(Hexicord::Snowflake(userObject["id"].get<std::string>()), userObject["avatar"])
            .download<Hexicord::Png>(ioService, 2048)
            .file.write(std::string("user_") + userObject["id"].get<std::string>() +
                        "_" + userObject["avatar"].get<std::string>() + ".png");
    }
}

void stealGuildAvatar(const nlohmann::json& guildObject) {
    std::string dbgJson = guildObject.dump(4); 
    Hexicord::ImageReference<Hexicord::GuildIcon>(Hexicord::Snowflake(guildObject["id"].get<std::string>()), guildObject["icon"])
        .download<Hexicord::Png>(ioService, 2048)
        .file.write(std::string("guild_") + guildObject["id"].get<std::string>() +
                    "_" + guildObject["icon"].get<std::string>() + ".png");
}

int main(int argc, char** argv) {
    const char* botToken = std::getenv("BOT_TOKEN");
    if (!botToken) {
        std::cerr << "Set bot token using BOT_TOKEN enviroment variable.\n"
                  << "E.g. env BOT_TOKEN=token_here " << argv[0] << '\n';
        return 1;
    }

    Hexicord::GatewayClient gclient(ioService, botToken);
    Hexicord::RestClient    rclient(ioService, botToken);

    gclient.eventDispatcher.addHandler(Hexicord::Event::GuildCreate, [](const nlohmann::json& payload) {
        stealGuildAvatar(payload);
        for (const nlohmann::json& member : payload["members"]) {
            stealUserAvatar(member["user"]);
        }
    });

    gclient.eventDispatcher.addHandler(Hexicord::Event::GuildMemberAdd, [](const nlohmann::json& payload) {
        stealUserAvatar(payload["user"]);
    });

    gclient.connect(rclient.getGatewayUrlBot().first);
    ioService.run();
}
