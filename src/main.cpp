#include "erl_hub.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

using namespace ReliefOps::Inventory;
using namespace ReliefOps::Logistics;

namespace {

constexpr int kTableWidth = 102;

void printRule(char fill = '-') {
    std::cout << '+' << std::string(kTableWidth - 2, fill) << "+\n";
}

void printBanner() {
    printRule('=');
    std::cout << "|" << std::left << std::setw(kTableWidth - 2)
              << " POST-EARTHQUAKE RELIEF LOGISTICS HUB" << "|\n";
    std::cout << "|" << std::left << std::setw(kTableWidth - 2)
              << " Supply allocation  |  Priority dispatch  |  Fleet routes  |  Audit" << "|\n";
    printRule('=');
}

void printSection(const std::string& title) {
    std::cout << '\n';
    printRule();
    std::cout << "  " << title << '\n';
    printRule();
}

void printField(const std::string& label) {
    std::cout << "  " << std::left << std::setw(34) << label << " : ";
}

int readInteger(const std::string& label, int minimum, int maximum) {
    int value = 0;
    while (true) {
        if (label.empty()) std::cout << "> ";
        else printField(label);
        if (std::cin >> value && value >= minimum && value <= maximum) return value;
        if (std::cin.eof()) throw std::runtime_error("Input ended before the scenario was complete.");
        std::cout << "Use a whole number from " << minimum << " to " << maximum << ".\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

double readPositiveNumber(const std::string& label) {
    double value = 0.0;
    while (true) {
        printField(label);
        if (std::cin >> value && value > 0.0) return value;
        if (std::cin.eof()) throw std::runtime_error("Input ended before the scenario was complete.");
        std::cout << "Use a number greater than zero.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

std::string readWord(const std::string& label) {
    std::string value;
    while (true) {
        printField(label);
        if (std::cin >> value) return value;
        if (std::cin.eof()) throw std::runtime_error("Input ended before the scenario was complete.");
        std::cout << "Use one word without spaces.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

SupplyType readSupplyType(int choice) {
    if (choice < 1 || choice > 4) throw std::invalid_argument("Supply choice must be 1 to 4.");
    return static_cast<SupplyType>(choice - 1);
}

double unitWeight(SupplyType type) {
    switch (type) {
    case SupplyType::RationKit: return 1.2;
    case SupplyType::TraumaKit: return 2.5;
    case SupplyType::CleanWater: return 1.0;
    case SupplyType::ThermalBlanket: return 1.8;
    }
    return 1.0;
}

struct VehicleInput {
    std::string id;
    double payloadKg;
    double rangeKm;
};

VehicleInput readVehicleInput(const std::string& vehicleName) {
    VehicleInput input{};
    while (true) {
        std::cout << "  " << std::left << std::setw(20) << vehicleName
                  << " | enter: ID  payload_kg  fuel_range_km\n  > ";
        if (std::cin >> input.id >> input.payloadKg >> input.rangeKm &&
            input.payloadKg > 0.0 && input.rangeKm > 0.0) return input;
        if (std::cin.eof()) throw std::runtime_error("Input ended before the scenario was complete.");
        std::cout << "  Example: TRK-01 2400 500\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

void printManifest(const ReliefVehicle& vehicle) {
    std::ostringstream title;
    title << vehicle.id() << " | " << vehicle.vehicleType() << " | payload "
          << std::fixed << std::setprecision(1) << vehicle.currentPayloadKg() << "/"
          << vehicle.payloadLimitKg() << " kg";
    printSection(title.str());
    std::cout << "  +-------------------+-------+----------+-----------+\n"
              << ManifestTableHeader{} << '\n';
    for (std::size_t index = 0; index < vehicle.manifest().size(); ++index) {
        const ReliefCargo& cargo = vehicle.manifest().at(index);
        std::cout << "  | " << std::left << std::setw(17) << toString(cargo.type())
                  << " | " << std::right << std::setw(5) << cargo.units()
                  << " | " << std::setw(8) << std::fixed << std::setprecision(1)
                  << cargo.unitWeightKg() << " | " << std::setw(9)
                  << cargo.totalWeightKg() << " |\n";
    }
    std::cout << "  +-------------------+-------+----------+-----------+\n";
}

void printMissionTableHeader() {
    std::cout << TableHeader{} << '\n';
}

void printMissionTableFooter() {
    std::cout << "  |--------------+----------+------------------+---------+-----------+------------+------------------|\n";
}

void printQueueTableHeader() {
    std::cout << "  +--------------+----------+------------------+-------+----------------+\n"
              << "  | INCIDENT     | PRIORITY | SUPPLY           | UNITS | QUEUE POSITION |\n"
              << "  +--------------+----------+------------------+-------+----------------+\n";
}

void printMissionRow(const MissionRequest& mission, int allocated,
                     const std::string& vehicle, const std::string& status) {
    std::cout << "  | " << std::left << std::setw(12) << mission.incidentId
              << " | " << std::setw(8) << toString(mission.priority)
              << " | " << std::setw(16) << toString(mission.supply)
              << " | " << std::right << std::setw(7) << mission.units
              << " | " << std::setw(9) << allocated
              << " | " << std::left << std::setw(10) << vehicle
              << " | " << std::setw(16) << status << " |\n";
}

void printRouteHeader() {
    std::cout << "  +------------+---------------------+---------------+-------------------------------+----------+\n"
              << "  | VEHICLE    | TYPE                | LOAD KG       | ROUTE STATUS                  | HRS      |\n"
              << "  +------------+---------------------+---------------+-------------------------------+----------+\n";
}

void assessRoutes(const std::vector<ReliefVehicle*>& fleet, double routeKm,
                  bool corridorDamaged, std::vector<std::string>& transitLogs) {
    printSection("FLEET ROUTE ASSESSMENT");
    std::cout << "  Route distance: " << std::fixed << std::setprecision(1) << routeKm
              << " km | Road corridor damaged: " << (corridorDamaged ? "yes" : "no") << "\n";
    printRouteHeader();
    for (const ReliefVehicle* vehicle : fleet) {
        std::string status = "Accepted";
        std::string hours = "-";
        try {
            std::ostringstream time;
            time << std::fixed << std::setprecision(2)
                 << vehicle->evaluateRoute(routeKm, corridorDamaged);
            hours = time.str();
            transitLogs.push_back(vehicle->id() + " route accepted");
        } catch (const DamagedCorridorException&) {
            status = "Damaged corridor";
            transitLogs.push_back(vehicle->id() + " route rejected: damaged corridor");
        } catch (const FuelDepletionException&) {
            status = "Fuel range exceeded";
            transitLogs.push_back(vehicle->id() + " route rejected: fuel range");
        }
        std::cout << "  | " << std::left << std::setw(10) << vehicle->id()
                  << " | " << std::setw(19) << vehicle->vehicleType()
                  << " | " << std::right << std::setw(6) << std::fixed << std::setprecision(1)
                  << vehicle->currentPayloadKg() << "/" << std::setw(6) << vehicle->payloadLimitKg()
                  << " | " << std::left << std::setw(29) << status
                  << " | " << std::right << std::setw(8) << hours << " |\n";
    }
    std::cout << "  +------------+---------------------+---------------+-------------------------------+----------+\n";
}

void printPersistenceResult(const std::vector<ReliefVehicle*>& fleet,
                            const std::vector<std::string>& transitLogs) {
    const std::string auditFile = "erl_hub_audit.bin";
    saveAuditAtomically(fleet, transitLogs, auditFile);
    const AuditSnapshot restored = restoreAudit(auditFile);
    printSection("BINARY AUDIT");
    std::cout << "  +------------+---------------------+---------------+\n"
              << "  | VEHICLE    | TYPE                | CARGO ROWS    |\n"
              << "  +------------+---------------------+---------------+\n";
    for (const auto& record : restored.manifests) {
        std::cout << "  | " << std::left << std::setw(10) << record.vehicleId
                  << " | " << std::setw(19) << record.vehicleType
                  << " | " << std::right << std::setw(13) << record.cargo.size() << " |\n";
    }
    std::cout << "  +------------+---------------------+---------------+\n"
              << "  Restored " << restored.transitLogs.size() << " transit log entries.\n";
}

void printManifestRoundTrip(const ReliefVehicle& vehicle) {
    Manifest copy = vehicle.manifest();
    std::stringstream stream;
    stream << vehicle.manifest();
    Manifest restored;
    stream >> restored;
    printSection("MANIFEST COPY AND STREAM CHECK");
    std::cout << "  Deep copy: " << (copy == vehicle.manifest() ? "pass" : "fail")
              << " | Stream restore: " << (restored == vehicle.manifest() ? "pass" : "fail") << '\n';
}

void runCustomScenario() {
    printSection("DEPOT INVENTORY INPUT");
    const std::string depotName = readWord("Depot name (one word)");
    DistributionDepot depot(depotName);
    const std::array<SupplyType, 4> types{{SupplyType::RationKit, SupplyType::TraumaKit,
                                          SupplyType::CleanWater, SupplyType::ThermalBlanket}};
    std::cout << "\n  +---------------------+-------------+\n"
              << "  | SUPPLY              | STOCK UNITS |\n"
              << "  +---------------------+-------------+\n";
    for (const SupplyType type : types) {
        std::cout << "  | " << std::left << std::setw(19) << toString(type) << " | ";
        const int stock = readInteger("", 0, 1000000);
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        depot += ReliefCargo(type, stock, unitWeight(type));
    }
    std::cout << "  +---------------------+-------------+\n";
    const int barrier = readInteger("Trauma-kit access barrier (0=no, 1=yes)", 0, 1);
    depot.setBarrier(SupplyType::TraumaKit, barrier == 1);

    printSection("MISSION REQUEST INPUT");
    std::cout << "  Enter one mission per line: INCIDENT_ID  SUPPLY#  UNITS  PRIORITY#\n"
              << "  Supply: 1 ration | 2 trauma | 3 water | 4 blanket\n"
              << "  Priority: 1 routine | 2 high | 3 critical\n"
              << "  +--------------+------------+----------+--------------------+\n"
              << "  | MISSION      | INCIDENT   | SUPPLY # | UNITS / PRIORITY # |\n"
              << "  +--------------+------------+----------+--------------------+\n";
    const int missionCount = readInteger("Number of missions (1-12)", 1, 12);
    PriorityDispatchQueue<MissionRequest, UrgencyFirst> dispatchQueue;
    for (int index = 0; index < missionCount; ++index) {
        MissionRequest mission{};
        int supplyChoice = 0;
        int priorityChoice = 0;
        while (true) {
            std::cout << "  " << std::left << std::setw(12) << ("Mission " + std::to_string(index + 1))
                      << " | ";
            if (std::cin >> mission.incidentId >> supplyChoice >> mission.units >> priorityChoice &&
                supplyChoice >= 1 && supplyChoice <= 4 && mission.units > 0 &&
                mission.units <= 1000000 && priorityChoice >= 1 && priorityChoice <= 3) {
                mission.supply = readSupplyType(supplyChoice);
                mission.priority = static_cast<MissionPriority>(priorityChoice);
                dispatchQueue.push(mission);
                break;
            }
            if (std::cin.eof()) throw std::runtime_error("Input ended before the missions were complete.");
            std::cout << "\n  Use: EQ-101 2 8 3\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    printSection("VEHICLE INPUT");
    std::cout << "  Enter each row as: VEHICLE_ID  PAYLOAD_KG  FUEL_RANGE_KM\n";
    const VehicleInput groundInput = readVehicleInput("Ground convoy");
    const VehicleInput airInput = readVehicleInput("Aerial transport");
    const VehicleInput amphibiousInput = readVehicleInput("Amphibious craft");
    GroundConvoy convoy(groundInput.id, groundInput.payloadKg, groundInput.rangeKm, 1);
    AerialTransport aircraft(airInput.id, airInput.payloadKg, airInput.rangeKm, 2);
    AmphibiousCraft amphibious(amphibiousInput.id, amphibiousInput.payloadKg,
                               amphibiousInput.rangeKm, 3);
    std::vector<ReliefVehicle*> fleet{&convoy, &aircraft, &amphibious};
    std::vector<std::string> transitLogs;

    printSection("PRIORITY DISPATCH RESULTS");
    printMissionTableHeader();
    std::size_t assignment = 0;
    while (!dispatchQueue.empty()) {
        const MissionRequest mission = dispatchQueue.pop();
        const int allocated = allocateSupply(depot, mission.supply, mission.priority, mission.units);
        ReliefVehicle* vehicle = fleet[assignment % fleet.size()];
        int loaded = allocated;
        std::string status = allocated == mission.units ? "Loaded" : "Stock limited";
        if (allocated > 0) {
            try {
                vehicle->loadCargo(ReliefCargo(mission.supply, allocated, unitWeight(mission.supply)));
            } catch (const CargoOverweightException&) {
                depot += ReliefCargo(mission.supply, allocated, unitWeight(mission.supply));
                loaded = 0;
                status = "Payload limit";
            }
        } else {
            status = "No stock / blocked";
        }
        printMissionRow(mission, loaded, vehicle->id(), status);
        transitLogs.push_back(mission.incidentId + " requested " +
                              std::to_string(mission.units) + " " + toString(mission.supply));
        ++assignment;
    }
    printMissionTableFooter();

    printSection("ROUTE INPUT");
    const double routeKm = readPositiveNumber("Route distance to affected area (km)");
    const bool corridorDamaged = readInteger("Road corridor damaged (0=no, 1=yes)", 0, 1) == 1;
    assessRoutes(fleet, routeKm, corridorDamaged, transitLogs);

    printSection("VEHICLE MANIFESTS");
    for (const ReliefVehicle* vehicle : fleet) printManifest(*vehicle);
    printManifestRoundTrip(convoy);

    const int recordDelivery = readInteger("Record a completed delivery (0=no, 1=yes)", 0, 1);
    if (recordDelivery == 1) {
        const int vehicleChoice = readInteger("Delivery vehicle (1=ground, 2=aerial, 3=amphibious)", 1, 3);
        ReliefVehicle* vehicle = fleet[static_cast<std::size_t>(vehicleChoice - 1)];
        if (vehicle->manifest().size() == 0) {
            std::cout << "  Selected vehicle has no manifest cargo.\n";
        } else {
            printManifest(*vehicle);
            const int row = readInteger("Manifest row to deliver", 1,
                                        static_cast<int>(vehicle->manifest().size()));
            const ReliefCargo& cargo = vehicle->manifest().at(static_cast<std::size_t>(row - 1));
            const SupplyType type = cargo.type();
            const int units = readInteger("Units delivered", 1, cargo.units());
            vehicle->deliverCargo(type, units);
            transitLogs.push_back(vehicle->id() + " delivered " + std::to_string(units) + " " +
                                  toString(type));
        }
    }

    printSection("OPERATIONS SUMMARY");
    std::cout << "  Depot: " << depot.name() << '\n'
              << "  Active vehicles: " << ReliefVehicle::activeVehicleCount() << '\n'
              << "  Delivered tonnage: " << std::fixed << std::setprecision(3)
              << ReliefVehicle::totalDispatchedTonnageKg() / 1000.0 << " tonnes\n";
    printPersistenceResult(fleet, transitLogs);
}

void runSampleScenario() {
    printSection("SAMPLE SCENARIO: NORTH FIELD DEPOT");
    DistributionDepot depot("NorthFieldDepot");
    depot += ReliefCargo(SupplyType::RationKit, 60, 1.2);
    depot += ReliefCargo(SupplyType::TraumaKit, 20, 2.5);
    depot += ReliefCargo(SupplyType::CleanWater, 80, 1.0);
    depot += ReliefCargo(SupplyType::ThermalBlanket, 25, 1.8);
    depot.setBarrier(SupplyType::TraumaKit, true);

    const int rationUnits = allocateSupply(depot, SupplyType::RationKit, 30);
    const int traumaUnits = allocateSupply(depot, SupplyType::TraumaKit,
                                          MissionPriority::Critical, 8);
    const int waterUnits = allocateSupply(depot, SupplyType::CleanWater,
                                          MissionPriority::High, 15);

    GroundConvoy convoy("TRK-01", 2400.0, 500.0, 1);
    AerialTransport aircraft("AIR-01", 600.0, 800.0, 2);
    AmphibiousCraft amphibious("AMP-01", 1800.0, 650.0, 3);
    convoy.loadCargo(ReliefCargo(SupplyType::RationKit, rationUnits, unitWeight(SupplyType::RationKit)));
    aircraft.loadCargo(ReliefCargo(SupplyType::TraumaKit, traumaUnits, unitWeight(SupplyType::TraumaKit)));
    amphibious.loadCargo(ReliefCargo(SupplyType::CleanWater, waterUnits, unitWeight(SupplyType::CleanWater)));

    PriorityDispatchQueue<MissionRequest, UrgencyFirst> queue;
    queue.push({"EQ-102", SupplyType::RationKit, 30, MissionPriority::Routine});
    queue.push({"EQ-101", SupplyType::TraumaKit, 8, MissionPriority::Critical});
    queue.push({"EQ-103", SupplyType::CleanWater, 15, MissionPriority::High});
    printSection("DISPATCH QUEUE");
    printQueueTableHeader();
    int queuePosition = 0;
    while (!queue.empty()) {
        const MissionRequest mission = queue.pop();
        std::cout << "  | " << std::left << std::setw(12) << mission.incidentId
                  << " | " << std::setw(8) << toString(mission.priority)
                  << " | " << std::setw(16) << toString(mission.supply)
                  << " | " << std::right << std::setw(7) << mission.units
                  << " | " << std::left << std::setw(14) << ++queuePosition << " |\n";
    }
    std::cout << "  +--------------+----------+------------------+-------+----------------+\n";

    std::vector<ReliefVehicle*> fleet{&convoy, &aircraft, &amphibious};
    std::vector<std::string> logs;
    const double routeKm = estimateRouteKm(28.61, 77.20, 30.37, 78.43);
    assessRoutes(fleet, routeKm, true, logs);

    printSection("FLEET MANIFESTS");
    for (const ReliefVehicle* vehicle : fleet) printManifest(*vehicle);
    printManifestRoundTrip(convoy);

    printSection("SAFETY CHECKS");
    try {
        aircraft.loadCargo(ReliefCargo(SupplyType::ThermalBlanket, 100, 12.0));
    } catch (const CargoOverweightException& error) {
        std::cout << "  Overweight cargo: " << error.what() << '\n';
    }
    try {
        static_cast<void>(aircraft.evaluateRoute(900.0, false));
    } catch (const FuelDepletionException& error) {
        std::cout << "  Fuel range: " << error.what() << '\n';
        logs.push_back("AIR-01 route rejected: fuel range");
    }

    convoy.deliverCargo(SupplyType::RationKit, 10);
    logs.push_back("TRK-01 delivered 10 ration kits");
    printSection("OPERATIONS SUMMARY");
    std::cout << "  Active vehicles: " << ReliefVehicle::activeVehicleCount() << '\n'
              << "  Delivered tonnage: " << std::fixed << std::setprecision(3)
              << ReliefVehicle::totalDispatchedTonnageKg() / 1000.0 << " tonnes\n";
    printPersistenceResult(fleet, logs);
}

} // namespace

int main() {
    try {
        printBanner();
        printSection("SELECT RUN MODE");
        std::cout << "  +------+---------------------------------------------------------------+\n"
                  << "  | 1    | Enter your own depot, missions, vehicles, and route           |\n"
                  << "  | 2    | Run the sample scenario                                     |\n"
                  << "  +------+---------------------------------------------------------------+\n";
        const int choice = readInteger("Choose mode (1 or 2)", 1, 2);
        if (choice == 1) runCustomScenario();
        else runSampleScenario();
        printRule('=');
        std::cout << "|" << std::left << std::setw(kTableWidth - 2)
                  << " RUN COMPLETE" << "|\n";
        printRule('=');
    } catch (const std::exception& error) {
        std::cerr << "\nERL-HUB error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
