#include "pch.h"
#include "AirRescueCallout.h"

#include "../Callouts.h"
#include "../Checkpoint.h"
#include "../Peds.h"
#include "../Ped.h"
#include "../Escort.h"
#include "../CleoFunctions.h"
#include "../BottomMessage.h"

#include <vector>
#include <cstdlib>
#include <cmath>

extern bool g_onACallout;

namespace
{
    constexpr int AGUIA_MODEL = 497;

    // Carro policial
    constexpr int POLICE_CAR_MODEL = 596;

    // PM
    constexpr int POLICE_SKIN = 280;

    // Hospital SF
    const CVector HOSPITAL_POSITION = CVector(
        -2662.0f,
        632.0f,
        14.0f
    );

    int g_victim = 0;
    int g_supportCar = 0;

    std::vector<int> g_witnesses;

    Checkpoint* g_hospitalCheckpoint = nullptr;

    CVector g_occurrencePosition;

    bool g_sceneCreated = false;
    bool g_supportCreated = false;
    bool g_victimDelivered = false;
    bool g_finished = false;

    bool IsValidActor(int actor)
    {
        return actor != 0 && ACTOR_DEFINED(actor);
    }

    bool IsValidCar(int car)
    {
        return car != 0 && CAR_DEFINED(car);
    }

    float DistanceBetween(const CVector& a, const CVector& b)
    {
        const float x = a.x - b.x;
        const float y = a.y - b.y;
        const float z = a.z - b.z;

        return std::sqrt(
            x * x +
            y * y +
            z * z
        );
    }

    void Cleanup()
    {
        if (g_hospitalCheckpoint)
        {
            Checkpoints::DestroyCheckpoint(g_hospitalCheckpoint);
            g_hospitalCheckpoint = nullptr;
        }

        if (IsValidActor(g_victim))
        {
            REMOVE_REFERENCES_TO_ACTOR(g_victim);
        }

        for (int witness : g_witnesses)
        {
            if (IsValidActor(witness))
            {
                REMOVE_REFERENCES_TO_ACTOR(witness);
            }
        }

        g_witnesses.clear();

        if (IsValidCar(g_supportCar))
        {
            REMOVE_REFERENCES_TO_CAR(g_supportCar);
        }

        g_victim = 0;
        g_supportCar = 0;

        g_sceneCreated = false;
        g_supportCreated = false;
        g_victimDelivered = false;
        g_finished = true;

        g_onACallout = false;
    }

    void FinishCallout()
    {
        if (g_finished)
            return;

        g_finished = true;

        if (g_hospitalCheckpoint)
        {
            Checkpoints::DestroyCheckpoint(g_hospitalCheckpoint);
            g_hospitalCheckpoint = nullptr;
        }

        if (IsValidActor(g_victim))
        {
            REMOVE_REFERENCES_TO_ACTOR(g_victim);
        }

        for (int witness : g_witnesses)
        {
            if (IsValidActor(witness))
            {
                REMOVE_REFERENCES_TO_ACTOR(witness);
            }
        }

        g_witnesses.clear();

        if (IsValidCar(g_supportCar))
        {
            REMOVE_REFERENCES_TO_CAR(g_supportCar);
        }

        g_victim = 0;
        g_supportCar = 0;

        g_onACallout = false;
    }

    void ShowMessage(const char* text)
    {
        BottomMessage::SetMessage(text, 3000);
    }

    void CreateHospitalCheckpoint()
    {
        if (g_hospitalCheckpoint)
            return;

        g_hospitalCheckpoint =
            Checkpoints::CreateCheckpoint(HOSPITAL_POSITION);

        if (!g_hospitalCheckpoint)
            return;

        g_hospitalCheckpoint->radius = 5.0f;

        g_hospitalCheckpoint->onEnterCheckpoint =
            []()
            {
                if (g_victimDelivered)
                    return;

                if (!IsValidActor(g_victim))
                    return;

                int player = GetPlayerActor();

                if (!IsValidActor(player))
                    return;

                Ped* victimPed = nullptr;

for (auto pair : Peds::GetPedsMap())
{
    Ped* ped = pair.second;

    if (ped && ped->ref == g_victim)
    {
        victimPed = ped;
        break;
    }
}

if (!Escort::IsPedBeeingCarried(victimPed))
{
    return;
}

                g_victimDelivered = true;

                ShowMessage(
                    "Civil entregue ao hospital"
                );

                WAIT(
                    1000,
                    []()
                    {
                        FinishCallout();
                    }
                );
            };
    }

    void CreateSupportUnit()
    {
        if (g_supportCreated)
            return;

        g_supportCreated = true;

        REQUEST_MODEL(POLICE_CAR_MODEL);
        REQUEST_MODEL(POLICE_SKIN);
        LOAD_REQUESTED_MODELS();

        WAIT(
            500,
            []()
            {
                CVector carPosition =
                    GET_CLOSEST_CAR_NODE(
                        g_occurrencePosition.x,
                        g_occurrencePosition.y,
                        g_occurrencePosition.z
                    );

                if (carPosition.x == 0.0f &&
                    carPosition.y == 0.0f &&
                    carPosition.z == 0.0f)
                {
                    carPosition = g_occurrencePosition;
                }

                g_supportCar = CREATE_CAR_AT(
                    POLICE_CAR_MODEL,
                    carPosition.x,
                    carPosition.y,
                    carPosition.z
                );

                if (!IsValidCar(g_supportCar))
                    return;

                SET_CAR_Z_ANGLE(
                    g_supportCar,
                    0.0f
                );

                SET_CAR_TRAFFIC_BEHAVIOUR(
                    g_supportCar,
                    AvoidCars
                );

                SET_CAR_ENGINE_OPERATION(
                    g_supportCar,
                    false
                );

                ENABLE_CAR_SIREN(
                    g_supportCar,
                    false
                );

                // Motorista
                int driver =
                    CREATE_ACTOR_PEDTYPE_IN_CAR_DRIVERSEAT(
                        g_supportCar,
                        PedType::Cop,
                        POLICE_SKIN
                    );

                // Passageiro 1
                int passenger1 =
                    CREATE_ACTOR_PEDTYPE_IN_CAR_PASSENGER_SEAT(
                        g_supportCar,
                        PedType::Cop,
                        POLICE_SKIN,
                        0
                    );

                // Passageiro 2
                int passenger2 =
                    CREATE_ACTOR_PEDTYPE_IN_CAR_PASSENGER_SEAT(
                        g_supportCar,
                        PedType::Cop,
                        POLICE_SKIN,
                        1
                    );

                if (IsValidActor(driver))
                {
                    SET_CHAR_STAY_IN_CAR_WHEN_JACKED(
                        driver,
                        true
                    );
                }

                if (IsValidActor(passenger1))
                {
                    SET_CHAR_STAY_IN_CAR_WHEN_JACKED(
                        passenger1,
                        true
                    );
                }

                if (IsValidActor(passenger2))
                {
                    SET_CHAR_STAY_IN_CAR_WHEN_JACKED(
                        passenger2,
                        true
                    );
                }
            }
        );
    }

    void CreateVictim()
    {
        if (g_victim != 0)
            return;

        REQUEST_MODEL(7);
        LOAD_REQUESTED_MODELS();

        WAIT(
            500,
            []()
            {
                CVector position =
                    STORE_PED_PATH_COORDS_CLOSEST_TO(
                        g_occurrencePosition.x,
                        g_occurrencePosition.y,
                        g_occurrencePosition.z
                    );

                if (position.x == 0.0f &&
                    position.y == 0.0f &&
                    position.z == 0.0f)
                {
                    position = g_occurrencePosition;
                }

                g_victim = CREATE_ACTOR_PEDTYPE(
                    PedType::CivMale,
                    7,
                    position.x,
                    position.y,
                    position.z
                );

                if (!IsValidActor(g_victim))
                    return;

                /*
                 * Deixa o ped com vida praticamente zerada.
                 * A lógica existente do projeto transforma
                 * o ped morto em ferido/inconsciente.
                 */
                SET_ACTOR_HEALTH(
                    g_victim,
                    1
                );

                PERFORM_ANIMATION_AS_ACTOR(
                    g_victim,
                    "crckdeth2",
                    "CRACK",
                    4.0f,
                    true,
                    true,
                    true,
                    true,
                    -1
                );

                auto peds = Peds::GetPedsMap();

                for (auto& entry : peds)
                {
                    Ped* ped = entry.second;

                    if (!ped)
                        continue;

                    if (ped->ref == g_victim)
                    {
                        ped->flags.isInconcious = true;
                        ped->flags.isFleeing = false;
                        ped->flags.showWidget = true;
                        ped->flags.willSurrender = true;
                        break;
                    }
                }
            }
        );
    }

    void CreateWitnesses()
    {
        if (!g_witnesses.empty())
            return;

        int amount = 3 + (std::rand() % 8);

        if (amount > 10)
            amount = 10;

        REQUEST_MODEL(7);
        LOAD_REQUESTED_MODELS();

        WAIT(
            500,
            [amount]()
            {
                for (int i = 0; i < amount; i++)
                {
                    float angle =
                        static_cast<float>(i) *
                        6.2831853f /
                        static_cast<float>(amount);

                    float distance =
                        4.0f +
                        static_cast<float>(
                            std::rand() % 500
                        ) / 100.0f;

                    float x =
                        g_occurrencePosition.x +
                        std::cos(angle) * distance;

                    float y =
                        g_occurrencePosition.y +
                        std::sin(angle) * distance;

                    float z =
                        g_occurrencePosition.z;

                    CVector position =
                        STORE_PED_PATH_COORDS_CLOSEST_TO(
                            x,
                            y,
                            z
                        );

                    if (position.x == 0.0f &&
                        position.y == 0.0f &&
                        position.z == 0.0f)
                    {
                        position = CVector(x, y, z);
                    }

                    int witness =
                        CREATE_ACTOR_PEDTYPE(
                            PedType::CivMale,
                            7,
                            position.x,
                            position.y,
                            position.z
                        );

                    if (!IsValidActor(witness))
                        continue;

                    g_witnesses.push_back(witness);

                    SET_CHAR_HEADING(
                        witness,
                        static_cast<float>(
                            std::rand() % 360
                        )
                    );
                }
            }
        );
    }

    void StartScene()
    {
        if (g_sceneCreated)
            return;

        g_sceneCreated = true;

        CreateVictim();
        CreateWitnesses();
    }

    void UpdateScene()
    {
        if (g_finished)
            return;

        int player =
            GetPlayerActor();

        if (!IsValidActor(player))
            return;

        CVector playerPosition =
            GetPedPosition(player);

        float distance =
            DistanceBetween(
                playerPosition,
                g_occurrencePosition
            );

        // Chegou na ocorrência
        if (!g_supportCreated &&
            distance <= 45.0f)
        {
            CreateSupportUnit();
        }

        /*
         * Se o jogador estiver carregando o civil,
         * cria o checkpoint do hospital.
         */
        if (!g_hospitalCheckpoint &&
            IsValidActor(g_victim))
        {
            auto peds = Peds::GetPedsMap();

            for (auto& entry : peds)
            {
                Ped* ped = entry.second;

                if (!ped)
                    continue;

                if (ped->ref != g_victim)
                    continue;

                if (Escort::IsPedBeeingCarried(ped))
                {
                    CreateHospitalCheckpoint();
                    break;
                }
            }
        }
    }
}

std::string AirRescueCallout::GetBroadcastMessage()
{
    return
        "Emergencia medica: civil gravemente ferido. "
        "Solicitamos apoio policial.";
}

AudioVariationGroup*
AirRescueCallout::GetBroadcastAudio()
{
    return nullptr;
}

int AirRescueCallout::GetWeight() const
{
    return 10;
}

int AirRescueCallout::GetLevel() const
{
    return 1;
}

void AirRescueCallout::OnAccept()
{
    g_onACallout = true;

    g_victim = 0;
    g_supportCar = 0;

    g_witnesses.clear();

    g_hospitalCheckpoint = nullptr;

    g_sceneCreated = false;
    g_supportCreated = false;
    g_victimDelivered = false;
    g_finished = false;

    int player =
        GetPlayerActor();

    if (!IsValidActor(player))
    {
        FinishCallout();
        return;
    }

    CVector playerPosition =
        GetPedPosition(player);

    /*
     * Pega um ponto de pedestre próximo ao jogador,
     * mas suficientemente afastado para virar ocorrência.
     */
    CVector occurrence =
        STORE_PED_PATH_COORDS_CLOSEST_TO(
            playerPosition.x + 60.0f,
            playerPosition.y + 60.0f,
            playerPosition.z
        );

    if (occurrence.x == 0.0f &&
        occurrence.y == 0.0f &&
        occurrence.z == 0.0f)
    {
        occurrence =
            GetPedPositionWithOffset(
                player,
                CVector(60.0f, 60.0f, 0.0f)
            );
    }

    g_occurrencePosition = occurrence;

    // MARCADOR DA OCORRÊNCIA
    CreateMarker(
        g_occurrencePosition.x,
        g_occurrencePosition.y,
        g_occurrencePosition.z,
        5,
        3,
        3
    );

    ShowMessage(
        "Ocorrencia de resgate medico marcada no mapa"
    );

    WAIT(
        1000,
        []()
        {
            StartScene();
        }
    );

    /*
     * Atualização periódica da ocorrência.
     */
    WAIT(
        500,
        []()
        {
            if (g_finished)
                return;

            UpdateScene();

            WAIT(
                500,
                []()
                {
                    if (g_finished)
                        return;

                    UpdateScene();
                }
            );
        }
    );
}