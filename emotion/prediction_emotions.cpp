// main.cpp
// Reçoit 14 dimensions via RabbitMQ, prédit 24 émotions, et envoie les prédictions à la centrale émotionnelle via RabbitMQ

#include <eigen3/Eigen/Dense>
#include <nlohmann/json.hpp>
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <SimpleAmqpClient/Channel.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>

using json = nlohmann::json;
using MatrixXd = Eigen::MatrixXd;
using VectorXd = Eigen::VectorXd;

// Structure du modèle (inchangée, identique à l'original)
struct Model {
    MatrixXd W_reg;
    VectorXd b_reg;
    std::vector<VectorXd> W_class;
    std::vector<std::string> dimNames, polyDimNames, emoNames;
    VectorXd xMean, xStd, yMean, yStd;
    MatrixXd pcaComponents;
    VectorXd pcaExplainedVariance;
    std::vector<int> rareEmotionIndices;
    int polyDegree;

    std::unordered_map<std::string, double> predict(const std::unordered_map<std::string, double>& input) const {
        std::cout << "Début de predict: input.size() = " << input.size() << ", dimNames.size() = " << dimNames.size() << "\n";

        for (const auto& dim : dimNames) {
            if (!input.count(dim)) {
                throw std::runtime_error("Dimension manquante dans l'input : " + dim);
            }
        }

        VectorXd x(dimNames.size());
        for (size_t i = 0; i < dimNames.size(); ++i) {
            x(i) = input.at(dimNames[i]);
            std::cout << "x[" << i << "] = " << x(i) << " (" << dimNames[i] << ")\n";
        }

        std::cout << "x.size() = " << x.size() << ", xMean.size() = " << xMean.size() << ", xStd.size() = " << xStd.size() << "\n";
        std::cout << "x: " << x.transpose() << "\n";
        std::cout << "xMean: " << xMean.transpose() << "\n";
        std::cout << "xStd: " << xStd.transpose() << "\n";

        if (x.size() != xMean.size() || x.size() != xStd.size()) {
            throw std::runtime_error("Incohérence dans les dimensions : x=" + std::to_string(x.size()) + ", xMean=" + std::to_string(xMean.size()) + ", xStd=" + std::to_string(xStd.size()));
        }

        for (int i = 0; i < xStd.size(); ++i) {
            if (xStd(i) <= 1e-6) {
                throw std::runtime_error("xStd[" + std::to_string(i) + "] = " + std::to_string(xStd(i)) + " est nul ou négatif");
            }
        }

        VectorXd xNorm = (x - xMean).cwiseQuotient(xStd);
        std::cout << "xNorm: " << xNorm.transpose() << "\n";

        VectorXd xProj = pcaComponents.transpose() * xNorm;
        std::cout << "xProj: " << xProj.transpose() << "\n";

        VectorXd xPoly(polyDimNames.size());
        int idx = 0;
        if (polyDegree == 1) {
            for (int i = 0; i < xProj.size(); ++i) {
                xPoly(idx++) = xProj(i);
            }
        } else {
            for (int i = 0; i < xProj.size(); ++i) {
                xPoly(idx++) = xProj(i);
            }
            for (int i = 0; i < xProj.size(); ++i) {
                xPoly(idx++) = xProj(i) * xProj(i);
            }
            for (int i = 0; i < xProj.size(); ++i) {
                for (int j = i + 1; j < xProj.size(); ++j) {
                    xPoly(idx++) = xProj(i) * xProj(j);
                }
            }
        }

        if (idx != W_reg.rows()) {
            throw std::runtime_error("Incohérence dans la taille des features polynomiales : attendu " + std::to_string(W_reg.rows()) + ", obtenu " + std::to_string(idx));
        }

        std::cout << "xPoly: " << xPoly.transpose() << "\n";

        VectorXd yStdPred = W_reg.transpose() * xPoly + b_reg;
        std::cout << "yStdPred: " << yStdPred.transpose() << "\n";

        VectorXd y = yStdPred.cwiseProduct(yStd) + yMean;
        std::cout << "y: " << y.transpose() << "\n";

        std::unordered_map<std::string, double> res;
        for (size_t i = 0; i < emoNames.size(); ++i) {
            double val = std::max(0.0, std::min(1.0, y(i)));
            res[emoNames[i]] = val;
        }

        return res;
    }
};

// Charger le modèle depuis model.json (inchangé, identique à l'original)
Model load_model(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Modèle introuvable : " + path);
    json j; f >> j;

    for (const auto& key : {"dim", "poly_dim", "emo", "W", "b", "x_mean", "x_std", "y_mean", "y_std", "pca_components", "pca_explained_variance", "rare_emotion_indices", "W_class", "poly_degree"}) {
        if (!j.contains(key)) {
            throw std::runtime_error("Clé manquante dans model.json : " + std::string(key));
        }
    }

    Model m;
    m.dimNames = j["dim"].get<std::vector<std::string>>();
    m.polyDimNames = j["poly_dim"].get<std::vector<std::string>>();
    m.emoNames = j["emo"].get<std::vector<std::string>>();

    int Nd = j["rows"].get<int>();
    int Ne = j["cols"].get<int>();

    if (m.dimNames.size() != j["x_mean"].size() || m.dimNames.size() != j["x_std"].size()) {
        throw std::runtime_error("Incohérence dans les dimensions : dim=" + std::to_string(m.dimNames.size()) + ", x_mean=" + std::to_string(j["x_mean"].size()) + ", x_std=" + std::to_string(j["x_std"].size()));
    }
    if (m.emoNames.size() != Ne || m.emoNames.size() != j["y_mean"].size() || m.emoNames.size() != j["y_std"].size()) {
        throw std::runtime_error("Incohérence dans les dimensions : emo=" + std::to_string(m.emoNames.size()) + ", y_mean=" + std::to_string(j["y_mean"].size()) + ", y_std=" + std::to_string(j["y_std"].size()));
    }
    if (m.polyDimNames.size() != Nd) {
        throw std::runtime_error("Incohérence dans les dimensions : poly_dim=" + std::to_string(m.polyDimNames.size()) + ", rows=" + std::to_string(Nd));
    }

    m.W_reg = Eigen::Map<MatrixXd>(j["W"].get<std::vector<double>>().data(), Nd, Ne);
    m.b_reg = Eigen::Map<VectorXd>(j["b"].get<std::vector<double>>().data(), Ne);

    auto w_class_data = j["W_class"].get<std::vector<std::vector<double>>>();
    for (const auto& w : w_class_data) {
        std::vector<double> w_copy = w;
        m.W_class.emplace_back(Eigen::Map<VectorXd>(w_copy.data(), w_copy.size()));
    }

    m.xMean = Eigen::Map<VectorXd>(j["x_mean"].get<std::vector<double>>().data(), m.dimNames.size());
    m.xStd  = Eigen::Map<VectorXd>(j["x_std"].get<std::vector<double>>().data(), m.dimNames.size());
    m.yMean = Eigen::Map<VectorXd>(j["y_mean"].get<std::vector<double>>().data(), Ne);
    m.yStd  = Eigen::Map<VectorXd>(j["y_std"].get<std::vector<double>>().data(), Ne);

    auto pca_components_data = j["pca_components"].get<std::vector<std::vector<double>>>();
    int pca_rows = pca_components_data.size();
    int pca_cols = pca_components_data.empty() ? 0 : pca_components_data[0].size();
    m.pcaComponents.resize(pca_rows, pca_cols);
    for (int i = 0; i < pca_rows; ++i) {
        m.pcaComponents.row(i) = Eigen::Map<VectorXd>(pca_components_data[i].data(), pca_cols);
    }

    m.pcaExplainedVariance = Eigen::Map<VectorXd>(
        j["pca_explained_variance"].get<std::vector<double>>().data(),
        j["pca_explained_variance"].size()
    );

    m.rareEmotionIndices = j["rare_emotion_indices"].get<std::vector<int>>();
    m.polyDegree = j["poly_degree"].get<int>();

    std::cout << "Modèle chargé : " << m.dimNames.size() << " dimensions, " << m.polyDimNames.size() << " features polynomiales, " << m.emoNames.size() << " émotions\n";
    return m;
}

int main() {
    try {
        // Chemins des fichiers
        const std::string model_path = "model.json";

        // Charger le modèle
        Model model = load_model(model_path);

        // Connexion à RabbitMQ
        AmqpClient::Channel::OpenOpts opts;
        opts.host = "localhost";
        opts.port = 5672;
        opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{"virtus", "virtus@83"};

        auto channel = AmqpClient::Channel::Open(opts);


        const std::string input_queue = "dimensions_queue";
        const std::string output_exchange = "mcee.emotional.input";
        const std::string routing_key = "emotions.predictions";

        // Déclarer la queue pour recevoir les dimensions
        channel->DeclareQueue(input_queue, false, true, false, false);

        // Déclarer l'échange pour envoyer les prédictions
        channel->DeclareExchange(output_exchange, AmqpClient::Channel::EXCHANGE_TYPE_TOPIC, false, true, false);

        // Liste des émotions dans l'ordre
        std::vector<std::string> ordered_emo = {
            "Admiration", "Adoration", "Appréciation esthétique", "Amusement", "Anxiété",
            "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
            "Dégoût", "Douleur empathique", "Fascination", "Excitation",
            "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie",
            "Soulagement", "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
        };

        std::cout << "En attente de messages RabbitMQ sur la queue " << input_queue << "...\n";

        // Consommateur de messages
        // Consommateur de messages
    std::string consumer_tag = channel->BasicConsume(input_queue, "", true, false, false, 1); // Changed no_ack to false explicitly

    while (true) {
        // Recevoir un message RabbitMQ
        AmqpClient::Envelope::ptr_t envelope;
        bool received = channel->BasicConsumeMessage(consumer_tag, envelope, 1000); // Timeout de 1 seconde
        if (!received || !envelope) {
            continue; // Pas de message, continuer
        }

        try {
            // Convertir le message en JSON
            std::string msg_str(envelope->Message()->Body().begin(), envelope->Message()->Body().end());
            std::cout << "Message brut reçu : " << msg_str << "\n";
            json input_json;
            try {
                input_json = json::parse(msg_str);
            } catch (const json::parse_error& e) {
                std::cerr << "Erreur de parsing JSON : " << e.what() << "\n";
                channel->BasicAck(envelope); // Acquitter même en cas d'erreur
                continue;
            }

            // Extraire les 14 dimensions
            std::unordered_map<std::string, double> dims;
            bool valid_input = true;
            for (const auto& dim : model.dimNames) {
                if (input_json.contains(dim)) {
                    try {
                        dims[dim] = input_json[dim].get<double>();
                        if (dims[dim] < 0.0 || dims[dim] > 1.0) {
                            std::cerr << "Valeur hors limites pour " << dim << ": " << dims[dim] << "\n";
                            valid_input = false;
                            break;
                        }
                    } catch (const json::type_error& e) {
                        std::cerr << "Type invalide pour " << dim << ": " << e.what() << "\n";
                        valid_input = false;
                        break;
                    }
                } else {
                    std::cerr << "Dimension manquante : " << dim << "\n";
                    valid_input = false;
                    break;
                }
            }

            if (!valid_input) {
                std::cerr << "Message JSON invalide, ignoré\n";
                channel->BasicAck(envelope); // Acquitter le message
                continue;
            }

            // Afficher les dimensions reçues
            std::cout << "\n=== Nouveau message reçu ===\n";
            std::cout << "Dimensions d'entrée :\n";
            for (const auto& dim : model.dimNames) {
                std::cout << std::setw(15) << std::left << dim << " : " << std::fixed << std::setprecision(3) << dims[dim] << "\n";
            }
            std::cout << "\n";

            // Faire la prédiction
            auto pred = model.predict(dims);

            // Préparer le message JSON pour la centrale émotionnelle
            json output_json;
            for (const auto& emo : ordered_emo) {
                output_json[emo] = pred.at(emo);
            }

            // Afficher les prédictions
            std::cout << "Prédictions des émotions :\n";
            std::cout << std::setw(30) << std::left << "Émotion" << " | " << std::setw(10) << "Prédite" << "\n";
            std::cout << std::string(50, '-') << "\n";
            for (const auto& emo : ordered_emo) {
                std::cout << std::setw(30) << std::left << emo << " | "
                          << std::fixed << std::setprecision(3) << pred.at(emo) << "\n";
            }

            // Envoyer les prédictions via RabbitMQ
            std::string output_str = output_json.dump();
            channel->BasicPublish(
                output_exchange,
                routing_key,
                AmqpClient::BasicMessage::Create(output_str),
                false, // Non obligatoire
                false  // Non persistant
            );

            std::cout << "\nPrédictions envoyées à la centrale émotionnelle via RabbitMQ\n";

            // Acquitter le message reçu
            channel->BasicAck(envelope);
        } catch (const std::exception& e) {
            std::cerr << "Erreur lors du traitement du message : " << e.what() << "\n";
            channel->BasicReject(envelope, true); // Rejeter et remettre en queue
            continue;
        }
    }
    } catch (const std::exception& e) {
        std::cerr << "\nErreur : " << e.what() << "\n";
        return 1;
    }

    return 0;
}