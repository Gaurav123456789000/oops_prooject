#include "erl_hub.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ReliefOps {
namespace Inventory {
namespace {

constexpr std::uint32_t kMaximumRecords = 100000U;
constexpr char kFileMagic[4] = {'E', 'R', 'L', 'H'};
constexpr std::uint32_t kFileVersion = 1U;

template <typename T>
void writeValue(std::ostream& output, const T& value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!output) {
        throw std::runtime_error("Could not write the audit file.");
    }
}

template <typename T>
T readValue(std::istream& input) {
    T value{};
    input.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!input) {
        throw std::runtime_error("Audit file is incomplete or unreadable.");
    }
    return value;
}

void writeString(std::ostream& output, const std::string& value) {
    if (value.size() > kMaximumRecords) {
        throw std::runtime_error("Audit text field is too large.");
    }
    writeValue(output, static_cast<std::uint32_t>(value.size()));
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    if (!output) {
        throw std::runtime_error("Could not write the audit file.");
    }
}

std::string readString(std::istream& input) {
    const auto length = readValue<std::uint32_t>(input);
    if (length > kMaximumRecords) {
        throw std::runtime_error("Audit text field has an invalid size.");
    }
    std::string value(length, '\0');
    if (length != 0) input.read(&value[0], static_cast<std::streamsize>(length));
    if (!input) {
        throw std::runtime_error("Audit file is incomplete or unreadable.");
    }
    return value;
}

std::uint32_t readCount(std::istream& input) {
    const auto count = readValue<std::uint32_t>(input);
    if (count > kMaximumRecords) {
        throw std::runtime_error("Audit file contains an invalid record count.");
    }
    return count;
}

void writeCargo(std::ostream& output, const ReliefCargo& cargo) {
    writeValue(output, static_cast<std::uint32_t>(cargo.type()));
    writeValue(output, static_cast<std::int32_t>(cargo.units()));
    writeValue(output, cargo.unitWeightKg());
}

ReliefCargo readCargo(std::istream& input) {
    const auto rawType = readValue<std::uint32_t>(input);
    const auto units = readValue<std::int32_t>(input);
    const auto unitWeight = readValue<double>(input);
    if (rawType > static_cast<std::uint32_t>(SupplyType::ThermalBlanket) || units < 0) {
        throw std::runtime_error("Audit file contains invalid cargo data.");
    }
    return ReliefCargo(static_cast<SupplyType>(rawType), units, unitWeight);
}

} // namespace

std::string toString(SupplyType type) {
    switch (type) {
    case SupplyType::RationKit: return "Ration kit";
    case SupplyType::TraumaKit: return "Trauma kit";
    case SupplyType::CleanWater: return "Clean water";
    case SupplyType::ThermalBlanket: return "Thermal blanket";
    }
    throw std::invalid_argument("Unknown supply type.");
}

SupplyType supplyTypeFromString(const std::string& value) {
    if (value == "RationKit") return SupplyType::RationKit;
    if (value == "TraumaKit") return SupplyType::TraumaKit;
    if (value == "CleanWater") return SupplyType::CleanWater;
    if (value == "ThermalBlanket") return SupplyType::ThermalBlanket;
    throw std::invalid_argument("Unknown supply type: " + value);
}

std::string toString(MissionPriority priority) {
    switch (priority) {
    case MissionPriority::Routine: return "Routine";
    case MissionPriority::High: return "High";
    case MissionPriority::Critical: return "Critical";
    }
    throw std::invalid_argument("Unknown mission priority.");
}

const char* CargoOverweightException::what() const noexcept {
    return "Cargo exceeds the vehicle payload limit.";
}
const char* FuelDepletionException::what() const noexcept {
    return "The route exceeds the vehicle fuel range.";
}
const char* DamagedCorridorException::what() const noexcept {
    return "The route crosses a damaged corridor.";
}

ReliefCargo::ReliefCargo(SupplyType type, int units, double unitWeightKg)
    : type_(type), units_(units), unitWeightKg_(unitWeightKg) {
    if (units < 0 || unitWeightKg <= 0.0) {
        throw std::invalid_argument("Cargo units must be nonnegative and unit weight must be positive.");
    }
}
SupplyType ReliefCargo::type() const noexcept { return type_; }
int ReliefCargo::units() const noexcept { return units_; }
double ReliefCargo::unitWeightKg() const noexcept { return unitWeightKg_; }
double ReliefCargo::totalWeightKg() const noexcept { return estimatePayloadKg(units_, unitWeightKg_); }

std::ostream& operator<<(std::ostream& output, const ReliefCargo& cargo) {
    return output << toString(cargo.type()) << ' ' << cargo.units() << " units, "
                  << cargo.totalWeightKg() << " kg";
}

std::istream& operator>>(std::istream& input, ReliefCargo& cargo) {
    std::string typeName;
    int units = 0;
    double weight = 0.0;
    if (!(input >> typeName >> units >> weight)) return input;
    try {
        cargo = ReliefCargo(supplyTypeFromString(typeName), units, weight);
    } catch (const std::exception&) {
        input.setstate(std::ios::failbit);
    }
    return input;
}

DistributionDepot::DistributionDepot(std::string name) : name_(std::move(name)) {}
const std::string& DistributionDepot::name() const noexcept { return name_; }

int DistributionDepot::stockOf(SupplyType type) const noexcept {
    const auto found = stock_.find(type);
    return found == stock_.end() ? 0 : found->second;
}

void DistributionDepot::setBarrier(SupplyType type, bool blocked) {
    barriers_[type] = blocked;
}

DistributionDepot& DistributionDepot::operator+=(const ReliefCargo& cargo) {
    int& stock = stock_[cargo.type()];
    if (cargo.units() > std::numeric_limits<int>::max() - stock) {
        throw std::overflow_error("Depot stock count is too large.");
    }
    stock += cargo.units();
    return *this;
}

DistributionDepot& DistributionDepot::operator-=(const ReliefCargo& cargo) {
    const auto found = stock_.find(cargo.type());
    const auto barrier = barriers_.find(cargo.type());
    if (barrier != barriers_.end() && barrier->second) {
        throw std::runtime_error("Supply is behind an access barrier.");
    }
    if (found == stock_.end() || found->second < cargo.units()) {
        throw std::runtime_error("Depot does not have enough stock.");
    }
    found->second -= cargo.units();
    return *this;
}

int DistributionDepot::withdraw_(SupplyType type, int requestedUnits, bool overrideBarrier) {
    if (requestedUnits <= 0) return 0;
    const auto barrier = barriers_.find(type);
    if (!overrideBarrier && barrier != barriers_.end() && barrier->second) return 0;
    const auto found = stock_.find(type);
    if (found == stock_.end()) return 0;
    const int allocated = std::min(requestedUnits, found->second);
    if (overrideBarrier) {
        found->second -= allocated;
    } else if (allocated > 0) {
        *this -= ReliefCargo(type, allocated, 1.0);
    }
    return allocated;
}

int IncidentCommander::allocateCritical(DistributionDepot& depot, SupplyType type,
                                        int requestedUnits) const {
    // The friend class can release critical supplies through restricted inventory.
    const auto barrier = depot.barriers_.find(type);
    const bool wasBlocked = barrier != depot.barriers_.end() && barrier->second;
    const int allocated = depot.withdraw_(type, requestedUnits, true);
    if (wasBlocked && allocated > 0) {
        depot.barriers_[type] = true;
    }
    return allocated;
}

int allocateSupply(DistributionDepot& depot, SupplyType type, int requestedUnits) {
    return depot.withdraw_(type, requestedUnits, false);
}

int allocateSupply(DistributionDepot& depot, SupplyType type, MissionPriority priority,
                   int requestedUnits) {
    if (priority == MissionPriority::Critical) {
        return IncidentCommander{}.allocateCritical(depot, type, requestedUnits);
    }
    return depot.withdraw_(type, requestedUnits, false);
}

Manifest::Manifest() : items_(nullptr), size_(0), capacity_(0) {}

Manifest::Manifest(const Manifest& other)
    : items_(other.size_ == 0 ? nullptr : new ReliefCargo[other.size_]),
      size_(other.size_), capacity_(other.size_) {
    for (std::size_t index = 0; index < size_; ++index) items_[index] = other.items_[index];
}

Manifest::Manifest(Manifest&& other) noexcept
    : items_(other.items_), size_(other.size_), capacity_(other.capacity_) {
    other.items_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Manifest& Manifest::operator=(Manifest other) noexcept {
    swap(other);
    return *this;
}

Manifest::~Manifest() { delete[] items_; }

void Manifest::swap(Manifest& other) noexcept {
    std::swap(items_, other.items_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

void Manifest::reserve_(std::size_t newCapacity) {
    if (newCapacity <= capacity_) return;
    auto* replacement = new ReliefCargo[newCapacity];
    for (std::size_t index = 0; index < size_; ++index) replacement[index] = items_[index];
    delete[] items_;
    items_ = replacement;
    capacity_ = newCapacity;
}

void Manifest::add(const ReliefCargo& cargo) {
    if (cargo.units() <= 0) return;
    for (std::size_t index = 0; index < size_; ++index) {
        if (items_[index].type() == cargo.type()) {
            const int existingUnits = items_[index].units();
            if (cargo.units() > std::numeric_limits<int>::max() - existingUnits) {
                throw std::overflow_error("Manifest unit count is too large.");
            }
            const int combinedUnits = existingUnits + cargo.units();
            const double combinedWeight = items_[index].totalWeightKg() + cargo.totalWeightKg();
            items_[index] = ReliefCargo(cargo.type(), combinedUnits,
                                        combinedWeight / combinedUnits);
            return;
        }
    }
    if (size_ == capacity_) reserve_(capacity_ == 0 ? 2 : capacity_ * 2);
    items_[size_++] = cargo;
}

int Manifest::remove(SupplyType type, int requestedUnits) {
    if (requestedUnits <= 0) return 0;
    for (std::size_t index = 0; index < size_; ++index) {
        if (items_[index].type() != type) continue;
        const int removed = std::min(requestedUnits, items_[index].units());
        const int remaining = items_[index].units() - removed;
        if (remaining == 0) {
            for (std::size_t move = index + 1; move < size_; ++move) items_[move - 1] = items_[move];
            --size_;
        } else {
            items_[index] = ReliefCargo(type, remaining, items_[index].unitWeightKg());
        }
        return removed;
    }
    return 0;
}

int Manifest::unitsOf(SupplyType type) const noexcept {
    int total = 0;
    for (std::size_t index = 0; index < size_; ++index) {
        if (items_[index].type() == type) total += items_[index].units();
    }
    return total;
}
std::size_t Manifest::size() const noexcept { return size_; }

const ReliefCargo& Manifest::at(std::size_t index) const {
    if (index >= size_) throw std::out_of_range("Manifest item index is out of range.");
    return items_[index];
}

Manifest& Manifest::operator+=(const ReliefCargo& cargo) {
    add(cargo);
    return *this;
}
Manifest& Manifest::operator-=(const ReliefCargo& cargo) {
    remove(cargo.type(), cargo.units());
    return *this;
}

bool Manifest::operator==(const Manifest& other) const noexcept {
    if (size_ != other.size_) return false;
    for (std::size_t index = 0; index < size_; ++index) {
        const auto& cargo = items_[index];
        bool matched = false;
        for (std::size_t candidate = 0; candidate < other.size_; ++candidate) {
            const auto& otherCargo = other.items_[candidate];
            if (cargo.type() == otherCargo.type() && cargo.units() == otherCargo.units() &&
                cargo.unitWeightKg() == otherCargo.unitWeightKg()) {
                matched = true;
                break;
            }
        }
        if (!matched) return false;
    }
    return true;
}
bool Manifest::operator!=(const Manifest& other) const noexcept { return !(*this == other); }

std::ostream& operator<<(std::ostream& output, const Manifest& manifest) {
    output << manifest.size_;
    for (std::size_t index = 0; index < manifest.size_; ++index) {
        const auto& cargo = manifest.items_[index];
        output << '\n' << static_cast<unsigned int>(cargo.type()) << ' '
               << cargo.units() << ' ' << cargo.unitWeightKg();
    }
    return output;
}

std::istream& operator>>(std::istream& input, Manifest& manifest) {
    std::size_t count = 0;
    if (!(input >> count)) return input;
    if (count > kMaximumRecords) {
        input.setstate(std::ios::failbit);
        return input;
    }
    Manifest parsed;
    try {
        for (std::size_t index = 0; index < count; ++index) {
            unsigned int rawType = 0;
            int units = 0;
            double weight = 0.0;
            if (!(input >> rawType >> units >> weight)) return input;
            if (rawType > static_cast<unsigned int>(SupplyType::ThermalBlanket)) {
                input.setstate(std::ios::failbit);
                return input;
            }
            parsed.add(ReliefCargo(static_cast<SupplyType>(rawType), units, weight));
        }
    } catch (const std::exception&) {
        input.setstate(std::ios::failbit);
        return input;
    }
    manifest.swap(parsed);
    return input;
}

GPSBeacon::GPSBeacon(std::string callSign) : callSign_(std::move(callSign)) {}
const std::string& GPSBeacon::callSign() const noexcept { return callSign_; }

EmergencyLifeSupportUnit::EmergencyLifeSupportUnit(int medicalPackCount)
    : medicalPackCount_(medicalPackCount) {}
int EmergencyLifeSupportUnit::medicalPackCount() const noexcept { return medicalPackCount_; }

int ReliefVehicle::activeVehicleCount_ = 0;
double ReliefVehicle::totalDispatchedTonnageKg_ = 0.0;

ReliefVehicle::ReliefVehicle(std::string id, double payloadLimitKg, double fuelRangeKm,
                             int medicalPackCount)
    : id_(std::move(id)), payloadLimitKg_(payloadLimitKg), fuelRangeKm_(fuelRangeKm),
      currentPayloadKg_(0.0), gpsBeacon_(id_), lifeSupport_(medicalPackCount), manifest_() {
    ++activeVehicleCount_;
}

ReliefVehicle::~ReliefVehicle() { --activeVehicleCount_; }
ReliefVehicle::ReliefVehicle(const ReliefVehicle& other)
    : id_(other.id_), payloadLimitKg_(other.payloadLimitKg_), fuelRangeKm_(other.fuelRangeKm_),
      currentPayloadKg_(other.currentPayloadKg_), gpsBeacon_(other.gpsBeacon_),
      lifeSupport_(other.lifeSupport_), manifest_(other.manifest_) {
    ++activeVehicleCount_;
}

ReliefVehicle& ReliefVehicle::operator=(const ReliefVehicle& other) {
    if (this != &other) {
        id_ = other.id_;
        payloadLimitKg_ = other.payloadLimitKg_;
        fuelRangeKm_ = other.fuelRangeKm_;
        currentPayloadKg_ = other.currentPayloadKg_;
        gpsBeacon_ = other.gpsBeacon_;
        lifeSupport_ = other.lifeSupport_;
        manifest_ = other.manifest_;
    }
    return *this;
}

const std::string& ReliefVehicle::id() const noexcept { return id_; }
double ReliefVehicle::payloadLimitKg() const noexcept { return payloadLimitKg_; }
double ReliefVehicle::currentPayloadKg() const noexcept { return currentPayloadKg_; }
const Manifest& ReliefVehicle::manifest() const noexcept { return manifest_; }

bool ReliefVehicle::loadCargo(const ReliefCargo& cargo) {
    const double proposedPayload = currentPayloadKg_ + cargo.totalWeightKg();
    if (proposedPayload > payloadLimitKg_) throw CargoOverweightException{};
    manifest_ += cargo;
    currentPayloadKg_ = proposedPayload;
    return true;
}

void ReliefVehicle::deliverCargo(SupplyType type, int units) {
    const int available = manifest_.unitsOf(type);
    if (units > available || units < 0) throw std::invalid_argument("Delivery exceeds the manifest.");
    double unitWeight = 1.0;
    for (std::size_t index = 0; index < manifest_.size(); ++index) {
        if (manifest_.at(index).type() == type) {
            unitWeight = manifest_.at(index).unitWeightKg();
            break;
        }
    }
    manifest_ -= ReliefCargo(type, units, unitWeight);
    currentPayloadKg_ -= estimatePayloadKg(units, unitWeight);
    totalDispatchedTonnageKg_ += estimatePayloadKg(units, unitWeight);
}

int ReliefVehicle::activeVehicleCount() noexcept { return activeVehicleCount_; }
double ReliefVehicle::totalDispatchedTonnageKg() noexcept { return totalDispatchedTonnageKg_; }
double ReliefVehicle::fuelRangeKm() const noexcept { return fuelRangeKm_; }

void ReliefVehicle::writeBinary(std::ostream& output) const {
    writeString(output, id_);
    writeString(output, vehicleType());
    writeValue(output, static_cast<std::uint32_t>(manifest_.size()));
    for (std::size_t index = 0; index < manifest_.size(); ++index) writeCargo(output, manifest_.at(index));
}

std::ostream& operator<<(std::ostream& output, const ReliefVehicle& vehicle) {
    return output << vehicle.vehicleType() << ' ' << vehicle.id() << ' '
                  << vehicle.currentPayloadKg() << '/' << vehicle.payloadLimitKg() << " kg";
}

AerialTransport::AerialTransport(std::string id, double payloadLimitKg, double fuelRangeKm,
                                 int medicalPackCount)
    : ReliefVehicle(std::move(id), payloadLimitKg, fuelRangeKm, medicalPackCount) {}
std::string AerialTransport::vehicleType() const { return "Aerial transport"; }
double AerialTransport::evaluateRoute(double distanceKm, bool) const {
    if (distanceKm > fuelRangeKm()) throw FuelDepletionException{};
    return distanceKm / 180.0;
}

GroundConvoy::GroundConvoy(std::string id, double payloadLimitKg, double fuelRangeKm,
                           int medicalPackCount)
    : ReliefVehicle(std::move(id), payloadLimitKg, fuelRangeKm, medicalPackCount) {}
std::string GroundConvoy::vehicleType() const { return "Ground convoy"; }
double GroundConvoy::evaluateRoute(double distanceKm, bool corridorDamaged) const {
    if (corridorDamaged) throw DamagedCorridorException{};
    if (distanceKm > fuelRangeKm()) throw FuelDepletionException{};
    return distanceKm / 65.0;
}

AmphibiousCraft::AmphibiousCraft(std::string id, double payloadLimitKg, double fuelRangeKm,
                                 int medicalPackCount)
    : ReliefVehicle(id, payloadLimitKg, fuelRangeKm, medicalPackCount),
      AerialTransport(id, payloadLimitKg, fuelRangeKm, medicalPackCount),
      GroundConvoy(std::move(id), payloadLimitKg, fuelRangeKm, medicalPackCount) {}
std::string AmphibiousCraft::vehicleType() const { return "Amphibious craft"; }
double AmphibiousCraft::evaluateRoute(double distanceKm, bool) const {
    if (distanceKm > fuelRangeKm()) throw FuelDepletionException{};
    return distanceKm / 90.0;
}

} // namespace Inventory

namespace Logistics {

bool UrgencyFirst::operator()(const MissionRequest& left, const MissionRequest& right) const noexcept {
    if (left.priority != right.priority) {
        return static_cast<int>(left.priority) > static_cast<int>(right.priority);
    }
    if (left.supply != right.supply) {
        return left.supply == Inventory::SupplyType::TraumaKit;
    }
    return left.units > right.units;
}

} // namespace Logistics

namespace Inventory {

std::ostream& operator<<(std::ostream& output, TableHeader) {
    return output << "  | INCIDENT     | PRIORITY | SUPPLY           | REQUEST | ALLOCATED | VEHICLE    | STATUS           |\n"
                  << "  |--------------+----------+------------------+---------+-----------+------------+------------------|";
}
std::ostream& operator<<(std::ostream& output, TableRule) {
    return output << "------------------------------------------------";
}
std::ostream& operator<<(std::ostream& output, ManifestTableHeader) {
    return output << "  | SUPPLY            | UNITS | UNIT KG  | TOTAL KG  |\n"
                  << "  |-------------------+-------+----------+-----------|";
}

void saveAuditAtomically(const std::vector<ReliefVehicle*>& fleet,
                         const std::vector<std::string>& transitLogs,
                         const std::string& filePath) {
    const std::string temporaryPath = filePath + ".tmp";
    try {
        std::fstream output(temporaryPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not open the temporary audit file.");
        output.write(kFileMagic, sizeof(kFileMagic));
        writeValue(output, kFileVersion);
        writeValue(output, static_cast<std::uint32_t>(fleet.size()));
        for (const auto* vehicle : fleet) {
            if (vehicle == nullptr) throw std::runtime_error("Fleet contains an empty vehicle entry.");
            vehicle->writeBinary(output);
        }
        writeValue(output, static_cast<std::uint32_t>(transitLogs.size()));
        for (const auto& log : transitLogs) writeString(output, log);
        output.flush();
        if (!output) throw std::runtime_error("Could not finish writing the audit file.");
        output.close();

#ifdef _WIN32
        if (!MoveFileExW(std::wstring(temporaryPath.begin(), temporaryPath.end()).c_str(),
                         std::wstring(filePath.begin(), filePath.end()).c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("Could not atomically replace the audit file.");
        }
#else
        if (std::rename(temporaryPath.c_str(), filePath.c_str()) != 0) {
            throw std::runtime_error("Could not atomically replace the audit file.");
        }
#endif
    } catch (...) {
        std::remove(temporaryPath.c_str());
        throw;
    }
}

AuditSnapshot restoreAudit(const std::string& filePath) {
    std::fstream input(filePath, std::ios::in | std::ios::binary);
    if (!input) throw std::runtime_error("Could not open the audit file for restore.");
    char magic[sizeof(kFileMagic)]{};
    input.read(magic, sizeof(magic));
    if (!input || !std::equal(std::begin(magic), std::end(magic), std::begin(kFileMagic))) {
        throw std::runtime_error("Audit file signature is invalid.");
    }
    if (readValue<std::uint32_t>(input) != kFileVersion) {
        throw std::runtime_error("Audit file version is not supported.");
    }

    AuditSnapshot snapshot;
    const auto vehicleCount = readCount(input);
    snapshot.manifests.reserve(vehicleCount);
    for (std::uint32_t vehicleIndex = 0; vehicleIndex < vehicleCount; ++vehicleIndex) {
        ManifestAudit record;
        record.vehicleId = readString(input);
        record.vehicleType = readString(input);
        const auto cargoCount = readCount(input);
        record.cargo.reserve(cargoCount);
        for (std::uint32_t cargoIndex = 0; cargoIndex < cargoCount; ++cargoIndex) {
            record.cargo.push_back(readCargo(input));
        }
        snapshot.manifests.push_back(std::move(record));
    }
    const auto logCount = readCount(input);
    snapshot.transitLogs.reserve(logCount);
    for (std::uint32_t index = 0; index < logCount; ++index) {
        snapshot.transitLogs.push_back(readString(input));
    }
    return snapshot;
}

} // namespace Inventory
} // namespace ReliefOps
