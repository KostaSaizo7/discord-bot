#include "poke.hpp"
#include "colors.h"
#include "dispatcher.h"
#include "message.h"
#include "toml.hpp"
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace poke {

    std::vector<int> legendaries = {
        144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 
        377, 378, 379, 380, 381, 382, 383, 384, 385, 386, 
        480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 
        490, 491, 492, 493, 638, 639, 640, 641, 642, 643, 
        644, 645, 646, 647, 648, 649, 716, 717, 718, 719, 
        720, 721, 785, 786, 787, 788, 789, 790, 791, 792, 
        800, 801, 802, 807, 888, 889, 890, 891, 892, 894, 
        895, 896, 897, 898,

        149,  // Dragonite
        248,  // Tyranitar
        373,  // Salamence
        376,  // Metagross
        445,  // Garchomp
        635,  // Hydreigon
        706,  // Goodra
        784,  // Kommo-o
        883,  // Dragapult
        997,  // Baxcalibur

        3,    // Venusaur
        6,    // Charizard
        9,    // Blastoise
        65,   // Alakazam
        94,   // Gengar
        95,   // Onix
        112,  // Rhydon
        130,  // Gyarados
        131,  // Lapras
        143,  // Snorlax
        197,  // Umbreon
        208,  // Steelix
        212,  // Scizor
        282,  // Gardevoir
        330,  // Flygon
        448,  // Lucario
        466,  // Electivire
        467,  // Magmortar
        477,  // Dusknoir
        530,  // Excadrill
        571,  // Zoroark
        609,  // Chandelure
        745,  // Lycanroc
        763,  // Tsareena
        859,  // Grimmsnarl

        25,   // Pikachu
        172,  // Pichu
        201,  // Unown
        282,  // Gardevoir
        134,  // Vaporeon
        135,  // Jolteon
        136,  // Flareon
        196,  // Espeon
        197,  // Umbreon
        470,  // Leafeon
        471,  // Glaceon
        700,  // Sylveon
    };
    
    time_t lastBanner;
    int bannerPokemon;

    bool leaderboardInvalidated = true;
    std::vector<std::pair<uint64_t, int>> leaderboard;

    using Pokemon = std::vector<std::string>;

    enum PokemonIndex {
        ID = 0,
        SHINY = 1,

        SIZE,
    };

    struct User {
        int wishes;
        std::vector<Pokemon> pokemon;
        time_t daily;
        time_t autoclaim;
        int pity;
    };

    std::unordered_map<uint64_t, User> users;

    std::mutex mtx;

    std::string getPath(uint64_t id)
    {
        return "poke/" + std::to_string(id) + ".toml";
    }

    void Initialize() {
        srand(time(nullptr));
        std::filesystem::create_directory("poke");
        auto dirIterator = std::filesystem::directory_iterator("poke");

        for (const auto& entry : dirIterator) {
            auto table = toml::parse(entry.path().string());

            User user;
            user.wishes = toml::get<int>(table["wishes"]);
            user.daily = toml::get<time_t>(table["daily"]);
            user.pokemon = toml::get<std::vector<Pokemon>>(table["pokemon"]);
            user.pity = toml::get<int>(table["pity"]);

            for (auto& pokemon : user.pokemon)
            {
                if (pokemon.size() < PokemonIndex::SIZE)
                {
                    pokemon.resize(PokemonIndex::SIZE);
                }

                if (pokemon[SHINY] != "true" && pokemon[SHINY] != "false")
                {
                    pokemon[SHINY] = "false";
                }
            }

            users[std::stoull(entry.path().filename().string())] = user;
        }

        if (std::filesystem::exists("banner.toml")) {
            auto table = toml::parse("banner.toml");
            lastBanner = toml::get<time_t>(table["lastBanner"]);
            bannerPokemon = toml::get<int>(table["bannerPokemon"]);
        } else {
            lastBanner = time(nullptr);
            bannerPokemon = legendaries[rand() % legendaries.size()];

            toml::table table;
            table["lastBanner"] = lastBanner;
            table["bannerPokemon"] = bannerPokemon;

            std::ofstream file("banner.toml");
            file << toml::value(table);
        }
    }

    void CheckAndCreateUser(uint64_t nid)
    {
        if (time(nullptr) - lastBanner > 259200)
        {
            lastBanner = time(nullptr);
            bannerPokemon = legendaries[rand() % legendaries.size()];

            toml::table table;
            table["lastBanner"] = lastBanner;
            table["bannerPokemon"] = bannerPokemon;

            std::ofstream file("banner.toml");
            file << toml::value(table);
        }

        std::string id = std::to_string(nid);

        if (users.find(std::stoull(id)) != users.end())
        {
            time_t now = time(nullptr);
            time_t daily = users[std::stoull(id)].daily;

            time_t diff = now - daily;
            time_t interval = 86400 / 4;

            if (diff >= interval)
            {
                int newWishes = (diff / interval) * 5;
                if (newWishes > 30)
                {
                    newWishes = 30;
                }

                users[std::stoull(id)].wishes += newWishes;
                users[std::stoull(id)].daily = time(nullptr) + (diff % interval);
            }

            toml::table table;
            table["wishes"] = users[std::stoull(id)].wishes;
            table["daily"] = users[std::stoull(id)].daily;
            table["pokemon"] = users[std::stoull(id)].pokemon;
            table["pity"] = users[std::stoull(id)].pity;

            std::ofstream file(getPath(nid));
            file << toml::value(table);
            return;
        } else {
            User user;
            user.wishes = 10;
            user.daily = time(nullptr);
            user.pokemon = {};
            user.pity = 0;
            users[std::stoull(id)] = user;

            toml::table table;
            table["wishes"] = user.wishes;
            table["daily"] = user.daily;
            table["pokemon"] = user.pokemon;
            table["pity"] = user.pity;

            std::ofstream file(getPath(nid));
            file << toml::value(table);
        }
    }

    void Daily(const dpp::slashcommand_t& event)
    {
        event.reply("/daily has been replaced with an automatic claiming system. Every 6 hours five wishes are added to your account, up to 30 wishes. You can check your wishes with /wishes.");
        // std::lock_guard<std::mutex> lock(mtx);
        // uint64_t id = event.command.get_issuing_user().id;
        // CheckAndCreateUser(id);

        // time_t now = time(nullptr);
        // time_t daily = users[id].daily;

        // if (now - daily < 86400 / 4)
        // {
        //     event.reply("You already claimed your daily reward. Next claim <t:" + std::to_string(daily + 86400 / 4) + ":R>.");
        // } else {
        //     users[id].daily = now;
        //     users[id].wishes += 5;

        //     std::ofstream file(getPath(id));
        //     toml::table table;
        //     table["wishes"] = users[id].wishes;
        //     table["daily"] = users[id].daily;
        //     table["pokemon"] = users[id].pokemon;
        //     table["pity"] = users[id].pity;
        //     file << toml::value(table);

        //     event.reply("You claimed your daily reward. You now have " + std::to_string(users[id].wishes) + " wishes. Next daily reset <t:" + std::to_string(now + 86400 / 4) + ":R>.");
        // }
    }

    void Wishes(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        int wishes = users[id].wishes;
        event.reply("You have " + std::to_string(wishes) + " wishes.");
    }

    void Wish(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        leaderboardInvalidated = true;
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        int wishes = users[id].wishes;
        
        if (wishes == 0)
        {
            event.reply("You have no wishes left.");
        } else {
            int roll1 = (rand() % 920) + 1;
            int roll2 = (rand() % 920) + 1;
            bool shiny = (rand() % 128) == 0;
            bool lucky = (rand() % 40) == 0;
            bool luckier = (rand() % 256) == 0;
            bool legendary = false;
            int roll;

            if (roll1 == bannerPokemon || roll2 == bannerPokemon)
            {
                roll = bannerPokemon;
                legendary = true;
            } else {
                roll = roll1;

                // Reroll once if legendary, to make them rarer
                if (std::find(legendaries.begin(), legendaries.end(), roll) != legendaries.end())
                {
                    roll = (rand() % 920) + 1;
                }

                if (std::find(legendaries.begin(), legendaries.end(), roll) != legendaries.end())
                {
                    legendary = true;
                }
            }

            if (users[id].pity >= 50)
            {
                users[id].pity = 0;
                roll = bannerPokemon;
                legendary = true;
            }
            
            std::string url = "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/other/showdown/" + std::string(shiny ? "shiny/" : "") + std::to_string(roll) + ".gif";
            dpp::embed embed = dpp::embed()
                .set_image(url);

            users[id].wishes--;

            std::string shinyText = shiny ? "SHINY! ✨ " : "";

            if (legendary)
            {
                embed.set_title(shinyText + "WOAH! You got #" + std::to_string(roll) + "! ");
                embed.set_color(dpp::colors::orange);
            } else {
                embed.set_title(shinyText + "You got #" + std::to_string(roll) + ".");
                embed.set_color(dpp::colors::green);
            }

            bool found = false;

            for (auto& pokemon : users[id].pokemon)
            {
                if (pokemon[ID] == std::to_string(roll))
                {
                    found = true;
                    break;
                }
            }

            std::string footer;

            if (!found)
            {
                Pokemon pokemon;
                pokemon.push_back(std::to_string(roll));
                pokemon.push_back(shiny ? "true" : "false");
                users[id].pokemon.push_back(pokemon);
            } else {
                if (shiny) {
                    footer += "You already have this pokemon as a non-shiny, so your non-shiny variant decomposed into 1 wish.\n";
                    users[id].wishes++;
                    
                    for (auto& pokemon : users[id].pokemon)
                    {
                        if (pokemon[ID] == std::to_string(roll))
                        {
                            pokemon[SHINY] = "true";
                            break;
                        }
                    }
                } else {
                    bool decompose = (rand() % 3) == 0;

                    if (decompose)
                    {
                        footer += "You already have this pokemon so it decomposed into 1 wish.\n";
                        users[id].wishes++;
                    } else {
                        footer += "You already have this pokemon so it decomposed.\n";
                    }
                }
            }

            if (lucky)
            {
                footer += "Today is your lucky day! At a 1/40 chance you got 5 free wishes!\n";
                users[id].wishes += 5;
            }

            if (luckier)
            {
                footer += "Today is your lucky day! At a 1/256 chance you got 20 free wishes!\n";
                users[id].wishes += 20;
            }

            embed.set_footer(footer, "");

            if (roll == bannerPokemon)
            {
                users[id].pity = 0;
            } else {
                users[id].pity++;
            }

            event.reply(dpp::message(event.command.channel_id, embed));

            std::ofstream file(getPath(id));
            toml::table table;
            table["wishes"] = users[id].wishes;
            table["daily"] = users[id].daily;
            table["pokemon"] = users[id].pokemon;
            table["pity"] = users[id].pity;
            file << toml::value(table);
        }
    }

    void List(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        dpp::embed embed = dpp::embed()
            .set_title("You have " + std::to_string(users[id].pokemon.size()) + " Pokemon.")
            .set_color(0x00FF00);

        if (users[id].pokemon.size() > 0) {
            bool shiny = users[id].pokemon[0][SHINY] == "true";
            embed.set_thumbnail("https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/other/showdown/" + std::string(shiny ? "shiny/" : "") + users[id].pokemon[0][ID] + ".gif");
        }

        std::string description = "";
        for (auto& pokemon : users[id].pokemon)
        {
            description += "#" + pokemon[ID] + (pokemon[SHINY] == "true" ? "✨" : "") + "\n";
        }

        embed.set_description(description);

        event.reply(dpp::message(event.command.channel_id, embed));
    }

    void Favorite(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        std::string pokemon = std::get<std::string>(event.get_parameter("pokemon"));

        bool found = false;

        Pokemon selectedPokemon;

        for (auto& p : users[id].pokemon)
        {
            if (p[ID] == pokemon)
            {
                found = true;
                selectedPokemon = p;
                break;
            }
        }

        if (!found)
        {
            event.reply("You don't have that Pokemon.");
        } else {
            users[id].pokemon.erase(std::remove(users[id].pokemon.begin(), users[id].pokemon.end(), selectedPokemon), users[id].pokemon.end());
            users[id].pokemon.insert(users[id].pokemon.begin(), selectedPokemon);

            std::ofstream file(getPath(id));
            toml::table table;
            table["wishes"] = users[id].wishes;
            table["daily"] = users[id].daily;
            table["pokemon"] = users[id].pokemon;
            table["pity"] = users[id].pity;
            file << toml::value(table);

            event.reply("You favorited " + pokemon + ".");
        }
    }

    void Banner(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        std::string url = "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/other/showdown/" + std::to_string(bannerPokemon) + ".gif";

        dpp::embed embed = dpp::embed()
            .set_thumbnail(url)
            .set_title("Today's banner is #" + std::to_string(bannerPokemon) + "!")
            .set_description("This pokemon has double chance to appear when you wish and you are guaranteed to get it after 50 wishes.\n\nNext banner reset: <t:" + std::to_string(lastBanner + 259200) + ":R>.")
            .set_footer("Wishes left until banner pull: " + std::to_string(50 - users[id].pity), "")
            .set_color(dpp::colors::orange);
        
        event.reply(dpp::message(event.command.channel_id, embed));
    }

    void Leaderboard(const dpp::slashcommand_t &event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        if (leaderboardInvalidated) {
            leaderboard.clear();

            for (auto& user : users)
            {
                int sum = 0;
                for (auto& pokemon : user.second.pokemon)
                {
                    if (pokemon[SHINY] == "true")
                    {
                        sum += 5;
                    } else {
                        sum++;
                    }

                    if (std::find(legendaries.begin(), legendaries.end(), std::stoi(pokemon[ID])) != legendaries.end())
                    {
                        sum += 10;
                    }
                }

                leaderboard.push_back({ user.first, sum });
            }

            std::sort(leaderboard.begin(), leaderboard.end(), [](const std::pair<uint64_t, int>& a, const std::pair<uint64_t, int>& b) {
                return a.second > b.second;
            });

            leaderboardInvalidated = false;
        }

        dpp::embed embed = dpp::embed()
            .set_title("Leaderboard")
            .set_color(0x00FF00);

        std::string description = "";
        int i = 1;

        for (auto& user : leaderboard)
        {
            description += std::to_string(i) + ". <@" + std::to_string(user.first) + "> - " + std::to_string(user.second) + " points\n";
            i++;

            if (i > 10)
            {
                break;
            }
        }

        embed.set_description(description);

        event.reply(dpp::message(event.command.channel_id, embed));
    }

    void Roulette(const dpp::slashcommand_t& event)
    {
        std::lock_guard<std::mutex> lock(mtx);
        uint64_t id = event.command.get_issuing_user().id;
        CheckAndCreateUser(id);

        int wishesOnRed = std::get<int64_t>(event.get_parameter("wishes_on_red"));
        int wishesOnBlack = std::get<int64_t>(event.get_parameter("wishes_on_black"));

        if (wishesOnRed < 0 || wishesOnBlack < 0)
        {
            event.reply("You can't bet negative wishes. Your favorite pokemon has been decomposed as a punishment.");
            return;
        }

        if (wishesOnRed + wishesOnBlack == 0) {
            event.reply("You have to bet at least 1 wish. Your favorite pokemon has been decomposed as a punishment.");
            return;
        }

        if (wishesOnRed + wishesOnBlack > users[id].wishes)
        {
            event.reply("You don't have enough wishes. Your favorite pokemon has been decomposed as a punishment.");
            return;
        }

        if (wishesOnRed + wishesOnBlack > 20) {
            event.reply("You can't bet more than 20 wishes. No martingale system for you!");
            return;
        }

        users[id].wishes -= wishesOnRed + wishesOnBlack;

        int roll = rand() % 37;
        bool even = roll % 2 == 0;
        std::string result;

        if (roll >= 1 && roll <= 10) {
            if (even) {
                result = "black";
            } else {
                result = "red";
            }
        } else if (roll >= 11 && roll <= 18) {
            if (even) {
                result = "red";
            } else {
                result = "black";
            }
        } else if (roll >= 19 && roll <= 28) {
            if (even) {
                result = "black";
            } else {
                result = "red";
            }
        } else if (roll != 0) {
            if (even) {
                result = "red";
            } else {
                result = "black";
            }
        }

        if (result.empty()) {
            result = "green";
        }

        int winnings = 0;

        if (result == "red") {
            winnings = wishesOnRed * 2;
        } else if (result == "black") {
            winnings = wishesOnBlack * 2;
        }

        users[id].wishes += winnings;

        std::ofstream file(getPath(id));
        toml::table table;
        table["wishes"] = users[id].wishes;
        table["daily"] = users[id].daily;
        table["pokemon"] = users[id].pokemon;
        table["pity"] = users[id].pity;
        file << toml::value(table);

        dpp::embed embed = dpp::embed()
            .set_title("Roulette 🎰")
            .set_color(0x00FF00);

        if (winnings > 0) {
            embed.set_description("The roulette landed on " + std::to_string(roll) + " (" + result + "). You won " + std::to_string(winnings) + " wishes.");
        } else {
            embed.set_description("The roulette landed on " + std::to_string(roll) + " (" + result + "). You lost " + std::to_string(wishesOnRed + wishesOnBlack) + " wishes.");
        }

        event.reply(dpp::message(event.command.channel_id, embed));
    }

}