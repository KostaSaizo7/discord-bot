#include "ai.hpp"
#include "restresults.h"
#include <liboai.h>
#include <fstream>
#include <iostream>
#include <functional>
#include <dpp/dpp.h>
#include <mutex>

liboai::OpenAI oai;
bool initialized;
uint64_t lastImageTimestamp = 0;
uint64_t lastQuestionTimestamp = 0;
liboai::Conversation convo;
liboai::Conversation convoFunny;
std::mutex mutex;
int lazyResetPromptCounter = 0;
std::string nonFunnyPrompt, originalPrompt;

float random_float(float min, float max) {
	return ((float)rand() / RAND_MAX) * (max - min) + min;
}

namespace artificial {
    
    void Initialize() {
        std::string token;
        std::ifstream keyFile("ai_key.txt");
        if (keyFile.is_open()) {
            std::getline(keyFile, token);
            keyFile.close();
        } else {
            std::cerr << "Failed to open AI token file" << std::endl;
            return;
        }

        bool success = oai.auth.SetKey(token);

        if (!success) {
            std::cerr << "Failed to set OpenAI API key" << std::endl;
            return;
        }

        std::ifstream promptFile("ai_prompt.txt");
        std::string data;
        promptFile.seekg(0, std::ios::end);
        data.resize(promptFile.tellg());
        promptFile.seekg(0, std::ios::beg);
        promptFile.read(&data[0], data.size());
        promptFile.close();
        nonFunnyPrompt = data;
        originalPrompt = data;
        convo.SetSystemData(data);

        std::ifstream funnyPromptFile("ai_funny_prompt.txt");
        std::string funnyData;
        funnyPromptFile.seekg(0, std::ios::end);
        funnyData.resize(funnyPromptFile.tellg());
        funnyPromptFile.seekg(0, std::ios::beg);
        funnyPromptFile.read(&funnyData[0], funnyData.size());
        funnyPromptFile.close();
        convoFunny.SetSystemData(funnyData);

        initialized = true;
    }

    void GenerateImage(const dpp::slashcommand_t& event, const std::string& prompt) {
        if (!initialized) {
            event.reply(dpp::message("AI not initialized").set_flags(dpp::m_ephemeral));
            return;
        }

        uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (timestamp - lastImageTimestamp < 3000) {
            event.reply(dpp::message("You can only generate an image every 20 minutes, or else Paris is going to go bankrupt. Next image available <t:" + std::to_string(lastImageTimestamp + 1200) + ":R>"));
            return;
        }

        try {
            liboai::Conversation convo;
            convo.SetSystemData("You are about to be given an image generation query. Respond with a single 'yes' and nothing else (without the quotes, just 3 letters) if it would be acceptable and OK to generate such an image. Respond with No and a reasoning if its not acceptable. This is for a safe for work wholesome server.");
            convo.AddUserData("Prompt: " + prompt);
            liboai::Response response = oai.ChatCompletion->create(
                "gpt-4o-mini", convo
            );
            convo.Update(response);
            std::string res = convo.GetLastResponse();
            if (res == "yes") {
                // Carry on
            } else {
                event.reply(dpp::message("The AI has rejected your prompt. The reason it gave: " + res));
                return;
            }
        } catch (...) {
            event.reply("The AI crashed while checking if this prompt is acceptable");
            return;
        }

        lastImageTimestamp = timestamp;
        event.thinking(false, [event, prompt](const dpp::confirmation_callback_t& callback) {
            std::thread t([event, prompt] {
                try {
                    liboai::Response res = oai.Image->create(
                        prompt,
                        1,
                        "256x512"
                    );

                    std::string url = res["data"][0]["url"];
                    dpp::embed embed = dpp::embed()
                        .set_image(url)
                        .set_title("This costed Paris: " + std::to_string(random_float(0.00005, 0.005)) + " euros");
                    event.edit_original_response(dpp::message(event.command.channel_id, embed));
                } catch (std::exception& e) {
                    printf("Exception: %s\n", e.what());
                    event.edit_original_response(dpp::message("This message crashed the AI lmao"));
                }
            });
            t.detach();
        });
    }

    void AskQuestion(const dpp::message_create_t& event, const std::string& prompt) {
        if (event.msg.channel_id != 1216695684665704479) {
            return;
        }

        if (!initialized) {
            event.reply(dpp::message("AI not initialized").set_flags(dpp::m_ephemeral));
            return;
        }

        std::thread t([event, prompt] {
            try {
                uint64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                // if (timestamp - lastQuestionTimestamp < 1) {
                //     event.reply(dpp::message("You can only ask a question every 1 second, or else Paris is going to go bankrupt."));
                //     return;
                // }
                lastQuestionTimestamp = timestamp;

                srand(time(0));
                int randomness = rand() % 20;
                printf("randomness: %d\n", randomness);
                if (randomness == 0) {
                    std::unique_lock<std::mutex> lock(mutex);
                    convoFunny.AddUserData("HUMAN: " + prompt + "\n");
                    lock.unlock();
                    liboai::Response response = oai.ChatCompletion->create(
                        "gpt-4o-mini", convoFunny
                    );
                    lock.lock();
                    convoFunny.Update(response);
                    event.reply(dpp::message(convoFunny.GetLastResponse()));
                } else {
                    std::unique_lock<std::mutex> lock(mutex);
                    lazyResetPromptCounter++;
                    if (lazyResetPromptCounter > 20) {
                        convo = liboai::Conversation();
                        convo.SetSystemData(nonFunnyPrompt);
                    }
                    convo.AddUserData("HUMAN: " + prompt + "\n");
                    lock.unlock();
                    liboai::Response response = oai.ChatCompletion->create(
                        "gpt-4o-mini", convo
                    );
                    lock.lock();
                    convo.Update(response);
                    event.reply(dpp::message(convo.GetLastResponse()));
                }
            } catch (std::exception& e) {
                std::unique_lock<std::mutex> lock(mutex);
                event.reply(dpp::message("This message crashed the AI lmao"));
                convo = liboai::Conversation();
                convo.SetSystemData(nonFunnyPrompt);
            }
        });
        t.detach();
    }

    void SetPrompt(const std::string& prompt) {
        std::thread t([prompt] {
            std::lock_guard<std::mutex> lock(mutex);
            nonFunnyPrompt = prompt;
            convo = liboai::Conversation();
            convo.SetSystemData(prompt);
        });
        t.detach();
    }

    void ResetPrompt() {
        std::thread t([] {
            std::lock_guard<std::mutex> lock(mutex);
            convo = liboai::Conversation();
            convo.SetSystemData(originalPrompt);
        });
        t.detach();
    }

    void ClearContext() {
        std::thread t([] {
            std::lock_guard<std::mutex> lock(mutex);
            convo = liboai::Conversation();
            convo.SetSystemData(nonFunnyPrompt);
        });
        t.detach();
    }

}