#ifndef ERL_HUB_HPP
#define ERL_HUB_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ReliefOps {
namespace Inventory {

enum class SupplyType { RationKit, TraumaKit, CleanWater, ThermalBlanket };
enum class MissionPriority { Routine = 1, High = 2, Critical = 3 };

std::string toString(SupplyType type);
SupplyType supplyTypeFromString(const std::string& value);
std::string toString(MissionPriority priority);

class CargoOverweightException : public std::exception {
public:
    const char* what() const noexcept override;
};

class FuelDepletionException : public std::exception {
public:
    const char* what() const noexcept override;
};

class DamagedCorridorException : public std::exception {
public:
    const char* what() const noexcept override;
};

class ReliefCargo {
public:
    ReliefCargo(SupplyType type = SupplyType::RationKit, int units = 0,
                double unitWeightKg = 1.0);

    SupplyType type() const noexcept;
    int units() const noexcept;
    double unitWeightKg() const noexcept;
    double totalWeightKg() const noexcept;

    friend std::ostream& operator<<(std::ostream& output, const ReliefCargo& cargo);
    friend std::istream& operator>>(std::istream& input, ReliefCargo& cargo);

private:
    SupplyType type_;
    int units_;
    double unitWeightKg_;
};

class DistributionDepot {
public:
    explicit DistributionDepot(std::string name);

    const std::string& name() const noexcept;
    int stockOf(SupplyType type) const noexcept;
    void setBarrier(SupplyType type, bool blocked);

    DistributionDepot& operator+=(const ReliefCargo& cargo);
    DistributionDepot& operator-=(const ReliefCargo& cargo);

private:
    friend class IncidentCommander;
    friend int allocateSupply(DistributionDepot& depot, SupplyType type, int requestedUnits);
    friend int allocateSupply(DistributionDepot& depot, SupplyType type,
                              MissionPriority priority, int requestedUnits);

    int withdraw_(SupplyType type, int requestedUnits, bool overrideBarrier);

    std::string name_;
    std::unordered_map<SupplyType, int> stock_;
    std::unordered_map<SupplyType, bool> barriers_;
};

class IncidentCommander {
public:
    int allocateCritical(DistributionDepot& depot, SupplyType type, int requestedUnits) const;
};

// Volume allocation and priority-based triage are separate overloads.
int allocateSupply(DistributionDepot& depot, SupplyType type, int requestedUnits);
int allocateSupply(DistributionDepot& depot, SupplyType type, MissionPriority priority,
                   int requestedUnits);

class Manifest {
public:
    Manifest();
    Manifest(const Manifest& other);
    Manifest(Manifest&& other) noexcept;
    Manifest& operator=(Manifest other) noexcept;
    ~Manifest();

    void swap(Manifest& other) noexcept;
    void add(const ReliefCargo& cargo);
    int remove(SupplyType type, int requestedUnits);
    int unitsOf(SupplyType type) const noexcept;
    std::size_t size() const noexcept;
    const ReliefCargo& at(std::size_t index) const;

    Manifest& operator+=(const ReliefCargo& cargo);
    Manifest& operator-=(const ReliefCargo& cargo);
    bool operator==(const Manifest& other) const noexcept;
    bool operator!=(const Manifest& other) const noexcept;

    friend std::ostream& operator<<(std::ostream& output, const Manifest& manifest);
    friend std::istream& operator>>(std::istream& input, Manifest& manifest);

private:
    void reserve_(std::size_t newCapacity);

    ReliefCargo* items_;
    std::size_t size_;
    std::size_t capacity_;
};

class GPSBeacon {
public:
    explicit GPSBeacon(std::string callSign = "UNASSIGNED");
    const std::string& callSign() const noexcept;
private:
    std::string callSign_;
};

class EmergencyLifeSupportUnit {
public:
    explicit EmergencyLifeSupportUnit(int medicalPackCount = 0);
    int medicalPackCount() const noexcept;
private:
    int medicalPackCount_;
};

class ReliefVehicle {
public:
    ReliefVehicle(std::string id, double payloadLimitKg, double fuelRangeKm,
                  int medicalPackCount);
    virtual ~ReliefVehicle();
    ReliefVehicle(const ReliefVehicle& other);
    ReliefVehicle& operator=(const ReliefVehicle& other);

    const std::string& id() const noexcept;
    double payloadLimitKg() const noexcept;
    double currentPayloadKg() const noexcept;
    const Manifest& manifest() const noexcept;
    bool loadCargo(const ReliefCargo& cargo);
    void deliverCargo(SupplyType type, int units);

    static int activeVehicleCount() noexcept;
    static double totalDispatchedTonnageKg() noexcept;

    virtual std::string vehicleType() const = 0;
    virtual double evaluateRoute(double distanceKm, bool corridorDamaged) const = 0;
    virtual void writeBinary(std::ostream& output) const;

    friend std::ostream& operator<<(std::ostream& output, const ReliefVehicle& vehicle);

protected:
    double fuelRangeKm() const noexcept;

private:
    static int activeVehicleCount_;
    static double totalDispatchedTonnageKg_;

    std::string id_;
    double payloadLimitKg_;
    double fuelRangeKm_;
    double currentPayloadKg_;
    GPSBeacon gpsBeacon_;
    EmergencyLifeSupportUnit lifeSupport_;
    Manifest manifest_;
};

// The assignment names TransportVehicle in Unit II and ReliefVehicle in Unit III.
using TransportVehicle = ReliefVehicle;

class AerialTransport : virtual public ReliefVehicle {
public:
    AerialTransport(std::string id, double payloadLimitKg = 600.0,
                    double fuelRangeKm = 800.0, int medicalPackCount = 2);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

class GroundConvoy : virtual public ReliefVehicle {
public:
    GroundConvoy(std::string id, double payloadLimitKg = 5000.0,
                 double fuelRangeKm = 500.0, int medicalPackCount = 1);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

class AmphibiousCraft : public AerialTransport, public GroundConvoy {
public:
    AmphibiousCraft(std::string id, double payloadLimitKg = 1800.0,
                    double fuelRangeKm = 650.0, int medicalPackCount = 3);
    std::string vehicleType() const override;
    double evaluateRoute(double distanceKm, bool corridorDamaged) const override;
};

} // namespace Inventory

namespace Logistics {

using Inventory::MissionPriority;
using Inventory::SupplyType;

struct MissionRequest {
    std::string incidentId;
    SupplyType supply;
    int units;
    MissionPriority priority;
};

struct UrgencyFirst {
    bool operator()(const MissionRequest& left, const MissionRequest& right) const noexcept;
};

template <typename T, typename Compare = std::less<T>>
class PriorityDispatchQueue {
public:
    void push(const T& value) {
        items_.push_back(value);
        std::sort(items_.begin(), items_.end(), compare_);
    }

    bool empty() const noexcept { return items_.empty(); }
    std::size_t size() const noexcept { return items_.size(); }

    T pop() {
        if (items_.empty()) {
            throw std::out_of_range("Dispatch queue is empty");
        }
        T next = items_.front();
        items_.erase(items_.begin());
        return next;
    }

private:
    std::vector<T> items_;
    Compare compare_{};
};

} // namespace Logistics

namespace Inventory {

struct ManifestAudit {
    std::string vehicleId;
    std::string vehicleType;
    std::vector<ReliefCargo> cargo;
};

struct AuditSnapshot {
    std::vector<ManifestAudit> manifests;
    std::vector<std::string> transitLogs;
};

struct TableHeader {};
struct TableRule {};
struct ManifestTableHeader {};
std::ostream& operator<<(std::ostream& output, TableHeader);
std::ostream& operator<<(std::ostream& output, TableRule);
std::ostream& operator<<(std::ostream& output, ManifestTableHeader);

// Inline utility required for quick distance and payload calculations.
inline double estimatePayloadKg(int units, double unitWeightKg) noexcept {
    return static_cast<double>(units) * unitWeightKg;
}

inline double estimateRouteKm(double latitudeA, double longitudeA,
                              double latitudeB, double longitudeB) noexcept {
    const double latitudeDelta = latitudeB - latitudeA;
    const double longitudeDelta = longitudeB - longitudeA;
    return 111.0 * std::sqrt(latitudeDelta * latitudeDelta +
                             longitudeDelta * longitudeDelta);
}

void saveAuditAtomically(const std::vector<ReliefVehicle*>& fleet,
                         const std::vector<std::string>& transitLogs,
                         const std::string& filePath);
AuditSnapshot restoreAudit(const std::string& filePath);

} // namespace Inventory
} // namespace ReliefOps

#endif



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
