#include "Escort.h"

#include "CleoFunctions.h"
#include "TopMessage.h"
#include "Vehicles.h"
#include "Peds.h"
#include "ScriptTask.h"
#include "BottomMessage.h"
#include "Criminals.h"
#include "ModelLoader.h"
#include "PoliceVehicleData.h"

Ped* g_escortingPed = nullptr;
Ped* g_carryingPed = nullptr;

bool g_pedWasInAVehicle = false;
static bool g_transportInProgress = false;
static int g_transportCarRef = 0;

void Escort::Update()
{
    bool playerActor = GetPlayerActor();
    bool isInCar = IS_CHAR_IN_ANY_CAR(playerActor);

    if(g_pedWasInAVehicle != isInCar)
    {
        g_pedWasInAVehicle = isInCar;

        if(isInCar)
        {
            OnPlayerEnterVehicle();
        }
    }

    if(g_carryingPed)
    {
        int playerActor = GetPlayerActor();
        auto playerPosition = GetPedPositionWithOffset(playerActor, CVector(0, 1.1f, 0));

        g_carryingPed->SetPosition(playerPosition);

        // Gira junto com o player (mesma direcao em que esta olhando)
        if (ACTOR_DEFINED(g_carryingPed->ref))
        {
            float heading = GET_CHAR_HEADING(playerActor);
            SET_CHAR_HEADING(g_carryingPed->ref, heading);
        }
    }
}


            void Escort::OpenEscortWindow(Ped* ped)
{
    if (!ped || !Peds::IsValid(ped) || !ACTOR_DEFINED(ped->ref))
        return;

    auto window = menuSZK->CreateWindow(
        g_defaultMenuPosition.x,
        g_defaultMenuPosition.y,
        800,
        GetTranslatedText("window_escort")
    );

    // ============================================================
    // CIVIL FERIDO
    // ============================================================

    if (ped->flags.isInconcious)
    {
        auto button = window->AddButton("Carregar civil ferido");

        button->onClick->Add([window, ped]() {

            window->Close();

            if (!Peds::IsValid(ped) || !ACTOR_DEFINED(ped->ref))
                return;

            if (g_carryingPed)
            {
                BottomMessage::SetMessage(
                    "Ja esta carregando alguem",
                    3000
                );
                return;
            }

            g_carryingPed = ped;

            ped->flags.showWidget = true;

            BottomMessage::SetMessage(
                "Carregando o civil ferido",
                3000
            );
        });

        auto closeButton =
            window->AddButton(
                "~r~" + GetTranslatedText("close")
            );

        closeButton->onClick->Add([window]() {
            window->Close();
        });

        return;
    }

    // ============================================================
    // MENU NORMAL DE CONDUCAO
    // ============================================================

    {
        auto button =
            window->AddButton(
                GetTranslatedText("escort_carry")
            );

        button->onClick->Add([window, ped]() {

            window->Close();

            if(g_carryingPed)
            {
                BottomMessage::SetMessage(
                    GetTranslatedText("error_already_escorting"),
                    3000
                );
                return;
            }

            g_carryingPed = ped;

            BottomMessage::SetMessage(
                "Carregando o suspeito",
                3000
            );
        });
    }

    {
        auto button =
            window->AddButton(
                GetTranslatedText("escort_follow")
            );

        button->onClick->Add([window, ped]() {

            window->Close();

            if(Escort::IsEscortingSomeone())
            {
                BottomMessage::SetMessage(
                    GetTranslatedText("error_already_escorting"),
                    3000
                );
                return;
            }

            ped->flags.showWidget = false;

            Escort::EscortPed(ped);
        });
    }

    {
        auto button =
            window->AddButton(
                "Chamar viatura para conduzir"
            );

        button->onClick->Add([window, ped]() {

            window->Close();

            Escort::CallTransportVehicle(ped);
        });
    }

    {
        auto button =
            window->AddButton(
                "Teleport to prision"
            );

        button->onClick->Add([window, ped]() {

            window->Close();

            if (g_carryingPed == ped)
                g_carryingPed = nullptr;

            if (g_escortingPed == ped)
                g_escortingPed = nullptr;

            Criminals::RemoveCriminal(ped);
            ped->QueueDestroy();

            BottomMessage::SetMessage(
                "~r~individuo foi preso",
                3000
            );
        });
    }

    {
        auto button =
            window->AddButton(
                "~r~" + GetTranslatedText("close")
            );

        button->onClick->Add([window]() {
            window->Close();
        });
    }
}
  
void Escort::OpenCarryingPedOptions(Ped* ped)
{
    auto window = menuSZK->CreateWindow(g_defaultMenuPosition.x, g_defaultMenuPosition.y, 800, GetTranslatedText("window_escort"));
    
    {
        auto button = window->AddButton(GetTranslatedText("stop_carry"));
        button->onClick->Add([window]() {
            window->Close();

            g_carryingPed = nullptr;
        });
    }

    {
        auto button = window->AddButton(GetTranslatedText("put_ped_in_trunk"));
        button->onClick->Add([window]() {
            window->Close();

            auto vehicle = GetClosestOpenTrunk();

            if(!vehicle)
            {
                BottomMessage::SetMessage("~r~O suspeito nao esta perto de um porta malas", 3000);
                return;
            }

            vehicle->trunk->AddPedToTrunk(g_carryingPed->ref);

            g_carryingPed = nullptr;
        });
    }

    {
        auto button = window->AddButton("~r~" + GetTranslatedText("close"));
        button->onClick->Add([window]() {
            window->Close();
        });
    }
}

void Escort::EscortPed(Ped* _ped)
{
    auto oldPed = _ped;

    //

    auto modelId = GET_ACTOR_MODEL(oldPed->ref);
    auto position = oldPed->GetPosition();

    auto newPedRef = CREATE_ACTOR_PEDTYPE(PedType::Special, modelId, position.x, position.y, position.z - 0.8f);
    auto newPed = Peds::RegisterPed(newPedRef);

    newPed->CopyFrom(*oldPed);
    newPed->ClearAnim();
    REMOVE_REFERENCES_TO_ACTOR(newPed->ref);

    oldPed->DestroyImmediate();
    
    //

    g_escortingPed = newPed;

    TopMessage::SetMessage(GetTranslatedText("escort_to_cop_vehicle"));
    
    //

    ScriptTask* taskEnter = new ScriptTask("enter");
    taskEnter->SetStartAgainIfTimeout(3000);
    taskEnter->onBegin = [newPed]() {
        if(newPed->isEnteringCar) return;

        //BottomMessage::SetMessage("make it follow", 1000);

        CLEAR_ACTOR_TASK(newPed->ref);
        TASK_FOLLOW_FOOTSTEPS(newPed->ref, GetPlayerActor());
    };
    taskEnter->onExecute = [newPed, taskEnter]() {        
        if(!Peds::IsValid(newPed)) return SCRIPT_CANCEL;

        if(newPed->IsInAnyCar()) return SCRIPT_SUCCESS;

        if(taskEnter->totalTimeElapsed > 70000)
        {
            return SCRIPT_CANCEL;
        }

        return SCRIPT_KEEP_GOING;
    };
    taskEnter->onCancel = []() {
        TopMessage::ClearMessage();
    };
    taskEnter->onComplete = []() {
    };
    taskEnter->Start();
}

void Escort::OnPlayerEnterVehicle()
{
    auto vehicleRef = GetVehiclePedIsUsing(GetPlayerActor());
    auto vehicle = Vehicles::GetVehicle(vehicleRef);

    if(!vehicle) return;
    if(!g_escortingPed) return;

    TopMessage::ClearMessage();

    auto ped = g_escortingPed;

    auto window = menuSZK->CreateWindow(g_defaultMenuPosition.x, g_defaultMenuPosition.y, 800, GetTranslatedText("window_escort_to_seat"));

    {
        auto button = window->AddButton("Banco de tras");
        button->onClick->Add([window, vehicle, ped]() {
            window->Close();

            //ped->flags.showWidget = false;
            
            //   0
            // 1 2

            int maxPassengers = CAR_MAX_PASSENGERS(vehicle->ref);
            int lastFreeSeat = -1; // -1 = nenhum assento livre

            for(int seatID = 0; seatID <= maxPassengers; seatID++)
            {
                if(CAR_PASSENGER_SEAT_FREE(vehicle->ref, seatID))
                {
                    lastFreeSeat = seatID; // guarda o último assento livre encontrado
                }
            }

            if(lastFreeSeat == -1)
            {
                BottomMessage::SetMessage("~r~Nenhum assento livre", 3000);
                return;
            }

            ped->EnterVehicle(vehicle->ref, lastFreeSeat + 1);
            
            CleoFunctions::AddWaitForFunction("escort_wait_to_enter", [ped]() {
                if(!Peds::IsValid(ped)) return true;
                if(ped->IsInAnyCar()) return true;

                return false;
            }, [ped]() {
                SET_CHAR_STAY_IN_CAR_WHEN_JACKED(ped->ref, true);

                g_escortingPed = nullptr;
                ped->flags.beeingEscorted = true;

                TopMessage::ClearMessage();
                BottomMessage::SetMessage(GetTranslatedText("escort_to_police_base"), 5000);
            });
        });
    }
}

bool Escort::IsEscortingSomeone()
{
    return g_escortingPed != nullptr;
}

bool Escort::IsCarryingSomeone()
{
    return g_carryingPed != nullptr;
}

bool Escort::IsPedBeeingCarried(Ped* ped)
{
    return ped == g_carryingPed;
}

Vehicle* Escort::GetClosestOpenTrunk()
{
    if(!g_carryingPed) return nullptr;

    auto pedPosition = g_carryingPed->GetPosition();

    auto vehicles = Vehicles::GetAllCarsInSphere(pedPosition, 10.0f);

    for(auto vehicle : vehicles)
    {
        if(!vehicle->trunkCheckpoint) continue;

        if(vehicle->trunkCheckpoint->IsInRange(pedPosition))
        {
            return vehicle;
        }
    }

    return nullptr;
}


void Escort::CallTransportVehicle(Ped* criminal)
{
    if (!criminal || !Peds::IsValid(criminal) || !ACTOR_DEFINED(criminal->ref))
    {
        BottomMessage::SetMessage("~r~Individuo invalido", 2500);
        return;
    }

    if (g_transportInProgress)
    {
        BottomMessage::SetMessage("~r~Ja ha uma viatura a caminho", 3000);
        return;
    }

    g_transportInProgress = true;
    g_transportCarRef = 0;

    if (g_carryingPed == criminal)
        g_carryingPed = nullptr;
    if (g_escortingPed == criminal)
        g_escortingPed = nullptr;

    criminal->flags.showWidget = false;
    criminal->ClearAnim();
    CLEAR_ACTOR_TASK(criminal->ref);

    // Fica parado ate a viatura chegar
    FREEZE_CHAR_POSITION(criminal->ref, true);
    criminal->SetCanDoHandsup();

    BottomMessage::SetMessage("~b~Chamando viatura para conduzir...", 3000);

    static const int kModels[3] = { 596, 597, 598 };
    int vehicleModelId = kModels[getRandomNumber(0, 2)];
    const int pedModelId = 280;

    auto position = criminal->GetPosition();
    // Mesma distancia de spawn da ambulancia
    auto spawnPosition = GET_CLOSEST_CAR_NODE(position.x + 120.0f, position.y, position.z);
    auto targetPosition = GET_CLOSEST_CAR_NODE(position.x, position.y, position.z);

    int criminalRef = criminal->ref;

    ModelLoader::AddModelToLoad(vehicleModelId);
    ModelLoader::AddModelToLoad(pedModelId, true);
    ModelLoader::LoadAll([vehicleModelId, pedModelId, spawnPosition, targetPosition, criminalRef]() {

        auto carRef = CREATE_CAR_AT(vehicleModelId, spawnPosition.x, spawnPosition.y, spawnPosition.z);
        auto car = Vehicles::RegisterVehicle(carRef);
        g_transportCarRef = carRef;

        // Dados de porta-malas (necessario para checkpoint + AddPedToTrunk)
        static PoliceVehicleData transportData;
        transportData.vehicleModelId = vehicleModelId;
        transportData.skinModelId = pedModelId;
        transportData.occupants = 2;
        transportData.trunkDisabled = false;
        transportData.trunkOffset = CVector(0.0f, -4.0f, 0.0f);
        transportData.trunkSeatPosition = CVector(0.50f, -1.80f, 0.90f);
        car->policeVehicleData = &transportData;
        car->trunkOffset = transportData.trunkOffset;

        // 2 policiais (280)
        auto driverRef = CREATE_ACTOR_PEDTYPE_IN_CAR_DRIVERSEAT(carRef, PedType::Special, pedModelId);
        Peds::RegisterPed(driverRef);
        auto passengerRef = CREATE_ACTOR_PEDTYPE_IN_CAR_PASSENGER_SEAT(carRef, PedType::Special, pedModelId, 0);
        Peds::RegisterPed(passengerRef);

        car->SetOwners();
        car->ShowBlip(COLOR_POLICE);
        ENABLE_CAR_SIREN(carRef, true);
        SET_CAR_ENGINE_OPERATION(carRef, true);

        auto safeCleanupAll = [carRef, driverRef, passengerRef, criminalRef]() {
            auto criminal = Peds::GetPed(criminalRef);
            if (criminal && Peds::IsValid(criminal))
            {
                Criminals::RemoveCriminal(criminal);
                if (g_carryingPed == criminal)
                    g_carryingPed = nullptr;
                criminal->QueueDestroy();
            }

            auto veh = Vehicles::GetVehicle(carRef);
            if (veh && Vehicles::IsValid(veh) && veh->trunk)
            {
                for (auto r : veh->trunk->GetPedsInTrunk())
                {
                    auto p = Peds::GetPed(r);
                    if (p && Peds::IsValid(p))
                    {
                        Criminals::RemoveCriminal(p);
                        p->QueueDestroy();
                    }
                }
            }

            auto d = Peds::GetPed(driverRef);
            if (d && Peds::IsValid(d))
                d->QueueDestroy();
            auto psg = Peds::GetPed(passengerRef);
            if (psg && Peds::IsValid(psg))
                psg->QueueDestroy();

            if (veh && Vehicles::IsValid(veh))
                veh->QueueDestroy(false);

            g_transportInProgress = false;
            g_transportCarRef = 0;
        };

        ScriptTask* taskDrive = new ScriptTask("transport_arrive");
        taskDrive->onBegin = [carRef, targetPosition]() {
            if (!CAR_DEFINED(carRef)) return;
            SET_CAR_MAX_SPEED(carRef, 25.0f);
            SET_CAR_TRAFFIC_BEHAVIOUR(carRef, DrivingMode::AvoidCars);
            ENABLE_CAR_SIREN(carRef, true);
            CAR_DRIVE_TO(carRef, targetPosition.x, targetPosition.y, targetPosition.z);
        };
        taskDrive->onExecute = [carRef, targetPosition]() {
            if (!CAR_DEFINED(carRef)) return SCRIPT_CANCEL;
            auto distance = DistanceFromVehicle(carRef, targetPosition);
            if (distance < 12.0f) return SCRIPT_SUCCESS;
            return SCRIPT_KEEP_GOING;
        };
        taskDrive->onCancel = [criminalRef]() {
            auto criminal = Peds::GetPed(criminalRef);
            if (criminal && Peds::IsValid(criminal) && ACTOR_DEFINED(criminal->ref))
                FREEZE_CHAR_POSITION(criminal->ref, false);
            g_transportInProgress = false;
            g_transportCarRef = 0;
        };
        taskDrive->onComplete = [carRef, driverRef, passengerRef, criminalRef, safeCleanupAll]() {
            if (!CAR_DEFINED(carRef))
            {
                g_transportInProgress = false;
                return;
            }

            auto car = Vehicles::GetVehicle(carRef);
            if (!car)
            {
                g_transportInProgress = false;
                return;
            }

            SET_CAR_MAX_SPEED(carRef, 0.0f);
            ENABLE_CAR_SIREN(carRef, false);

            // Libera o criminoso (player vai carregar ate o porta-malas)
            auto criminal = Peds::GetPed(criminalRef);
            if (criminal && Peds::IsValid(criminal) && ACTOR_DEFINED(criminal->ref))
            {
                FREEZE_CHAR_POSITION(criminal->ref, false);
                criminal->flags.showWidget = true;
            }

            // Policiais descem e ficam parados ao lado da viatura
            car->MakeOccupantsLeave();

            CleoFunctions::AddWaitForFunction("transport_cops_out", [carRef, driverRef, passengerRef]() {
                if (!CAR_DEFINED(carRef)) return true;
                auto d = Peds::GetPed(driverRef);
                auto p = Peds::GetPed(passengerRef);
                bool dOut = !d || !Peds::IsValid(d) || !IS_CHAR_IN_ANY_CAR(d->ref);
                bool pOut = !p || !Peds::IsValid(p) || !IS_CHAR_IN_ANY_CAR(p->ref);
                return dOut && pOut;
            }, [carRef, driverRef, passengerRef, criminalRef, safeCleanupAll]() {
                if (!CAR_DEFINED(carRef))
                {
                    g_transportInProgress = false;
                    return;
                }

                // Apenas param onde desceram — sem teleporte/offset
                for (int pr : { driverRef, passengerRef })
                {
                    auto ped = Peds::GetPed(pr);
                    if (!ped || !Peds::IsValid(ped) || !ACTOR_DEFINED(ped->ref)) continue;
                    CLEAR_ACTOR_TASK(ped->ref);
                    FREEZE_CHAR_POSITION(ped->ref, true); // parado no lugar em que saiu do carro
                }

                BottomMessage::SetMessage("~b~Coloque o individuo no porta-malas da viatura", 4000);

                // Espera o criminoso (ou qualquer um) entrar no porta-malas DESTA viatura
                CleoFunctions::AddWaitForFunction("transport_wait_trunk", [carRef, criminalRef]() {
                    if (!CAR_DEFINED(carRef)) return true;
                    auto veh = Vehicles::GetVehicle(carRef);
                    if (!veh || !veh->trunk) return false;

                    // Checkpoint do porta-malas so aparece com IsCarryingSomeone + policeVehicleData
                    // (ja configuramos policeVehicleData)

                    if (!veh->trunk->HasPedsInside())
                        return false;

                    // Preferencialmente o criminoso alvo
                    for (auto r : veh->trunk->GetPedsInTrunk())
                    {
                        if (r == criminalRef)
                            return true;
                    }
                    // Qualquer ped no porta-malas desta viatura tambem conta
                    return true;
                }, [carRef, driverRef, passengerRef, criminalRef, safeCleanupAll]() {
                    if (!CAR_DEFINED(carRef))
                    {
                        g_transportInProgress = false;
                        return;
                    }

                    // 3 segundos e vao embora
                    WAIT(3000, [carRef, driverRef, passengerRef, criminalRef, safeCleanupAll]() {
                        if (!CAR_DEFINED(carRef))
                        {
                            g_transportInProgress = false;
                            return;
                        }

                        auto veh = Vehicles::GetVehicle(carRef);
                        if (!veh)
                        {
                            g_transportInProgress = false;
                            return;
                        }

                        // Solta policiais e manda entrar
                        for (int pr : { driverRef, passengerRef })
                        {
                            auto ped = Peds::GetPed(pr);
                            if (ped && Peds::IsValid(ped) && ACTOR_DEFINED(ped->ref))
                            {
                                FREEZE_CHAR_POSITION(ped->ref, false);
                                CLEAR_ACTOR_TASK(ped->ref);
                            }
                        }

                        veh->MakeOwnersEnter();

                        CleoFunctions::AddWaitForFunction("transport_cops_in", [carRef, driverRef]() {
                            if (!CAR_DEFINED(carRef)) return true;
                            auto d = Peds::GetPed(driverRef);
                            if (!d || !Peds::IsValid(d)) return true;
                            return IS_CHAR_IN_ANY_CAR(d->ref);
                        }, [carRef, driverRef, passengerRef, criminalRef, safeCleanupAll]() {
                            if (!CAR_DEFINED(carRef))
                            {
                                g_transportInProgress = false;
                                return;
                            }

                            SET_CAR_MAX_SPEED(carRef, 30.0f);
                            SET_CAR_TRAFFIC_BEHAVIOUR(carRef, DrivingMode::AvoidCars);
                            ENABLE_CAR_SIREN(carRef, true);

                            auto driver = Peds::GetPed(driverRef);
                            if (driver && Peds::IsValid(driver))
                                driver->StartDrivingRandomly();

                            // Mensagem so quando saem dirigindo
                            BottomMessage::SetMessage("~b~Individuo sendo conduzido", 3500);

                            // Longe: some viatura + policiais + criminoso (porta-malas)
                            CleoFunctions::AddWaitForFunction("transport_despawn", [carRef]() {
                                if (!CAR_DEFINED(carRef)) return true;
                                auto carPos = GetCarPosition(carRef);
                                auto playerPos = GetPlayerPosition();
                                return distanceBetweenPoints(carPos, playerPos) > 150.0f;
                            }, [safeCleanupAll]() {
                                safeCleanupAll();
                            });
                        });
                    });
                });
            });
        };
        taskDrive->Start();
    });
}

