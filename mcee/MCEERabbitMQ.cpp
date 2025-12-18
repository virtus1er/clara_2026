//===== MCEERabbitMQ.cpp - Version complète corrigée =====
#include "MCEERabbitMQ.h"
#include "MCEEConfig.h"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <SimpleAmqpClient/Channel.h>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace MCEE {
    RabbitMQManager::RabbitMQManager() {}

    RabbitMQManager::~RabbitMQManager() {
        shutdown();
    }

    bool RabbitMQManager::initialize(const std::string& host, int port,
                                   const std::string& username, const std::string& password) {
        try {
            // Store connection parameters for reconnection
            connection_host_ = host;
            connection_port_ = port;
            connection_username_ = username;
            connection_password_ = password;

            // Create channel - gérer la conversion boost -> std
            try {
                // Tentative avec nouvelle API
                AmqpClient::Channel::OpenOpts opts;
                opts.host = host;
                opts.port = port;

                auto boost_channel = AmqpClient::Channel::Open(opts);
                // Utiliser get() pour récupérer le pointeur brut et créer un std::shared_ptr avec custom deleter
                channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){
                    // Le boost_channel se détruit automatiquement via la capture
                });

            } catch (...) {
                try {
                    // Fallback vers l'ancienne API
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    auto boost_channel = AmqpClient::Channel::Create(host, port, username, password);
                    channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){
                        // Le boost_channel se détruit automatiquement via la capture
                    });
                    #pragma GCC diagnostic pop
                } catch (...) {
                    // Dernière tentative sans auth
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    auto boost_channel = AmqpClient::Channel::Create(host, port);
                    channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){
                        // Le boost_channel se détruit automatiquement via la capture
                    });
                    #pragma GCC diagnostic pop
                }
            }

            if (!channel_) {
                std::cerr << "Failed to create AMQP channel" << std::endl;
                return false;
            }

            connected_ = true;
            std::cout << "Connected to RabbitMQ at " << host << ":" << port << std::endl;

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to RabbitMQ: " << e.what() << std::endl;
            return false;
        }
    }

    void RabbitMQManager::shutdown() {
        should_stop_ = true;
        connected_ = false;

        // Join consumer threads
        if (emotional_consumer_thread_.joinable()) {
            emotional_consumer_thread_.join();
        }

        if (context_consumer_thread_.joinable()) {
            context_consumer_thread_.join();
        }

        // Close channel
        try {
            if (channel_) {
                channel_.reset();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error closing AMQP channel: " << e.what() << std::endl;
        }

        std::cout << "RabbitMQ connection closed" << std::endl;
    }

    void RabbitMQManager::subscribeToEmotionalInput(const std::string& queue,
                                                   EmotionalInputCallback callback) {
        emotional_callback_ = callback;
        emotional_queue_ = queue;

        try {
            declareQueue(queue);

            // Start consumer thread
            emotional_consumer_thread_ = std::thread(&RabbitMQManager::emotionalConsumerLoop, this);

            std::cout << "Subscribed to emotional input queue: " << queue << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Failed to subscribe to emotional input queue: " << e.what() << std::endl;
        }
    }

    void RabbitMQManager::subscribeToContextData(const std::string& queue,
                                                ContextDataCallback callback) {
        context_callback_ = callback;
        context_queue_ = queue;

        try {
            declareQueue(queue);

            // Start consumer thread
            context_consumer_thread_ = std::thread(&RabbitMQManager::contextConsumerLoop, this);

            std::cout << "Subscribed to context data queue: " << queue << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Failed to subscribe to context data queue: " << e.what() << std::endl;
        }
    }

    bool RabbitMQManager::publishContextualizedEmotions(const std::string& queue,
                                                       const ContextualizedEmotions& emotions) {
        if (!connected_ || !channel_) {
            if (!reconnectIfNeeded()) {
                return false;
            }
        }

        try {
            declareQueue(queue);

            std::string message = serializeToJson(emotions);

            AmqpClient::BasicMessage::ptr_t amqp_message = AmqpClient::BasicMessage::Create(message);

            channel_->BasicPublish("", queue, amqp_message);

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to publish contextualized emotions: " << e.what() << std::endl;
            connected_ = false;
            return false;
        }
    }

    bool RabbitMQManager::publishAmygdaleonSignal(const std::string& queue,
                                                 const AmygdaleonSignal& signal) {
        if (!connected_ || !channel_) {
            if (!reconnectIfNeeded()) {
                return false;
            }
        }

        try {
            declareQueue(queue);

            std::string message = serializeToJson(signal);

            AmqpClient::BasicMessage::ptr_t amqp_message = AmqpClient::BasicMessage::Create(message);

            channel_->BasicPublish("", queue, amqp_message);

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to publish Amygdaleon signal: " << e.what() << std::endl;
            connected_ = false;
            return false;
        }
    }

    bool RabbitMQManager::publishMemoryToConsolidate(const std::string& queue,
                                                    const MemoryToConsolidate& memory) {
        if (!connected_ || !channel_) {
            if (!reconnectIfNeeded()) {
                return false;
            }
        }

        try {
            declareQueue(queue);

            std::string message = serializeToJson(memory);

            AmqpClient::BasicMessage::ptr_t amqp_message = AmqpClient::BasicMessage::Create(message);

            channel_->BasicPublish("", queue, amqp_message);

            return true;

        } catch (const std::exception& e) {
            std::cerr << "Failed to publish memory to consolidate: " << e.what() << std::endl;
            connected_ = false;
            return false;
        }
    }

    void RabbitMQManager::emotionalConsumerLoop() {
        std::cout << "Starting emotional consumer loop for queue: " << emotional_queue_ << std::endl;

        try {
            std::string consumer_tag = channel_->BasicConsume(emotional_queue_, "");

            while (!should_stop_ && connected_) {
                try {
                    AmqpClient::Envelope::ptr_t envelope;

                    // Consume with timeout (1 second)
                    if (channel_->BasicConsumeMessage(consumer_tag, envelope, 1000)) {
                        if (envelope && envelope->Message()) {
                            std::string message = envelope->Message()->Body();
                            processEmotionalMessage(message);

                            // Acknowledge the message
                            channel_->BasicAck(envelope);
                        }
                    }

                } catch (const AmqpClient::AmqpException& e) {
                    std::cerr << "AMQP error in emotional consumer: " << e.what() << std::endl;
                    if (!reconnectIfNeeded()) {
                        break;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in emotional consumer: " << e.what() << std::endl;
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Failed to start emotional consumer: " << e.what() << std::endl;
        }

        std::cout << "Emotional consumer loop ended" << std::endl;
    }

    void RabbitMQManager::contextConsumerLoop() {
        std::cout << "Starting context consumer loop for queue: " << context_queue_ << std::endl;

        try {
            std::string consumer_tag = channel_->BasicConsume(context_queue_, "");

            while (!should_stop_ && connected_) {
                try {
                    AmqpClient::Envelope::ptr_t envelope;

                    // Consume with timeout (1 second)
                    if (channel_->BasicConsumeMessage(consumer_tag, envelope, 1000)) {
                        if (envelope && envelope->Message()) {
                            std::string message = envelope->Message()->Body();
                            processContextMessage(message);

                            // Acknowledge the message
                            channel_->BasicAck(envelope);
                        }
                    }

                } catch (const AmqpClient::AmqpException& e) {
                    std::cerr << "AMQP error in context consumer: " << e.what() << std::endl;
                    if (!reconnectIfNeeded()) {
                        break;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in context consumer: " << e.what() << std::endl;
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Failed to start context consumer: " << e.what() << std::endl;
        }

        std::cout << "Context consumer loop ended" << std::endl;
    }

    void RabbitMQManager::processEmotionalMessage(const std::string& message) {
        try {
            if (emotional_callback_) {
                EmotionalInput input = deserializeEmotionalInput(message);
                emotional_callback_(input);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing emotional message: " << e.what() << std::endl;
        }
    }

    void RabbitMQManager::processContextMessage(const std::string& message) {
        try {
            if (context_callback_) {
                ContextData context = deserializeContextData(message);
                context_callback_(context);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing context message: " << e.what() << std::endl;
        }
    }

    void RabbitMQManager::declareQueue(const std::string& queue_name) {
        if (!channel_) return;

        try {
            channel_->DeclareQueue(queue_name, false, true, false, false);
        } catch (const std::exception& e) {
            std::cerr << "Failed to declare queue " << queue_name << ": " << e.what() << std::endl;
        }
    }

    bool RabbitMQManager::reconnectIfNeeded() {
        if (connected_ && channel_) {
            return true;
        }

        std::cout << "Attempting to reconnect to RabbitMQ..." << std::endl;

        try {
            // Mêmes tentatives qu'à l'initialisation
            try {
                AmqpClient::Channel::OpenOpts opts;
                opts.host = connection_host_;
                opts.port = connection_port_;

                auto boost_channel = AmqpClient::Channel::Open(opts);
                channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){});

            } catch (...) {
                try {
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    auto boost_channel = AmqpClient::Channel::Create(connection_host_, connection_port_,
                                                                   connection_username_, connection_password_);
                    channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){});
                    #pragma GCC diagnostic pop
                } catch (...) {
                    #pragma GCC diagnostic push
                    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    auto boost_channel = AmqpClient::Channel::Create(connection_host_, connection_port_);
                    channel_ = std::shared_ptr<AmqpClient::Channel>(boost_channel.get(), [boost_channel](AmqpClient::Channel*){});
                    #pragma GCC diagnostic pop
                }
            }

            if (channel_) {
                connected_ = true;
                std::cout << "Reconnected to RabbitMQ successfully" << std::endl;
                return true;
            }

        } catch (const std::exception& e) {
            std::cerr << "Reconnection failed: " << e.what() << std::endl;
        }

        connected_ = false;
        return false;
    }

    std::string RabbitMQManager::serializeToJson(const ContextualizedEmotions& emotions) {
        json j;
        j["emotions_contextualisees"] = emotions.emotions_contextualisees;
        j["emotion_globale"] = emotions.emotion_globale;
        j["contexte_detecte"] = emotions.contexte_detecte;
        j["confiance_contexte"] = emotions.confiance_contexte;
        j["gradient_danger_global"] = emotions.gradient_danger_global;
        j["niveau_danger"] = static_cast<int>(emotions.niveau_danger);
        j["signal_amyghaleon"] = emotions.signal_amyghaleon;
        j["souvenir_a_consolider"] = emotions.souvenir_a_consolider;
        j["priorite_mlt"] = static_cast<int>(emotions.priorite_mlt);
        j["text_id"] = emotions.text_id;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            emotions.timestamp.time_since_epoch()).count();

        return j.dump();
    }

    std::string RabbitMQManager::serializeToJson(const AmygdaleonSignal& signal) {
        json j;
        j["urgence"] = signal.urgence;
        j["niveau_danger"] = static_cast<int>(signal.niveau_danger);
        j["gradient_danger_global"] = signal.gradient_danger_global;
        j["contexte_detecte"] = signal.contexte_detecte;
        j["emotions_critiques"] = signal.emotions_critiques;
        j["gradients_declencheurs"] = signal.gradients_declencheurs;
        j["recommandation_intervention"] = signal.recommandation_intervention;
        j["text_id"] = signal.text_id;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            signal.timestamp.time_since_epoch()).count();

        return j.dump();
    }

    std::string RabbitMQManager::serializeToJson(const MemoryToConsolidate& memory) {
        json j;
        j["id_mcee"] = memory.id_mcee;
        j["statut"] = memory.statut;
        j["priorite"] = static_cast<int>(memory.priorite);
        j["emotions_brutes"] = memory.emotions_brutes;
        j["emotions_contextualisees"] = memory.emotions_contextualisees;
        j["contexte_detecte"] = memory.contexte_detecte;
        j["score_significativite"] = memory.score_significativite;
        j["recommandation_traitement"] = memory.recommandation_traitement;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            memory.timestamp.time_since_epoch()).count();

        return j.dump();
    }

    EmotionalInput RabbitMQManager::deserializeEmotionalInput(const std::string& json_str) {
        json j = json::parse(json_str);
        EmotionalInput input;

        input.emotions_brutes = j["emotions_brutes"].get<EmotionArray>();
        input.intensite_globale = j["intensite_globale"];
        input.emotions_dominantes = j["emotions_dominantes"].get<std::vector<std::string>>();
        input.text_id = j["text_id"];

        auto timestamp_ms = j["timestamp"].get<int64_t>();
        input.timestamp = Timestamp(std::chrono::milliseconds(timestamp_ms));

        return input;
    }

    ContextData RabbitMQManager::deserializeContextData(const std::string& json_str) {
        json j = json::parse(json_str);
        ContextData context;

        // Physical sensors
        auto& sensors = j["capteurs_physiques"];
        context.capteurs_physiques.temperature_ambiante = sensors["temperature_ambiante"];
        context.capteurs_physiques.volume_sonore = sensors["volume_sonore"];
        context.capteurs_physiques.luminosite = sensors["luminosite"];
        context.capteurs_physiques.gyroscope_stabilite = sensors["gyroscope_stabilite"];

        // Technical states
        auto& states = j["etats_internes"];
        context.etats_internes.temperature_cpu = states["temperature_cpu"];
        context.etats_internes.temperature_gpu = states["temperature_gpu"];
        context.etats_internes.charge_cpu = states["charge_cpu"];
        context.etats_internes.utilisation_ram = states["utilisation_ram"];
        context.etats_internes.stabilite_systeme = states["stabilite_systeme"];

        // External feedbacks
        auto& feedbacks = j["feedbacks_externes"];
        context.feedbacks_externes.validation_positive = feedbacks["validation_positive"];
        context.feedbacks_externes.encouragement_recu = feedbacks["encouragement_recu"];
        context.feedbacks_externes.alerte_externe = feedbacks["alerte_externe"];
        context.feedbacks_externes.interaction_sociale = feedbacks["interaction_sociale"];

        auto timestamp_ms = j["timestamp"].get<int64_t>();
        context.timestamp = Timestamp(std::chrono::milliseconds(timestamp_ms));

        return context;
    }
}